/*=============================================================================
  @file sns_sensor.c

  Sole controller of the list of Sensors available on the system.  All other
  modules must query this one to create a new Sensor, or search for a specific
  Sensor.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdatomic.h>

#include "sns_assert.h"
#include "sns_cstruct_extn.h"
#include "sns_data_stream.h"
#include "sns_dl_utils.h"
#include "sns_fw_attribute_service.h"
#include "sns_fw_basic_services.h"
#include "sns_fw_diag_service.h"
#include "sns_fw_memory_service.h"
#include "sns_fw_sensor_instance.h"
#include "sns_fw_sensor.h"
#include "sns_fw_service_manager.h"
#include "sns_fw.pb.h"
#include "sns_isafe_list.h"
#include "sns_island.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_printf_int.h"
#include "sns_qshtech_island.h"
#include "sns_rc.h"
#include "sns_registry_util.h"
#include "sns_sensor_event.h"
#include "sns_sensor_uid.h"
#include "sns_sensor.h"
#include "sns_suid_sensor.h"
#include "sns_thread_manager.h"
#include "sns_types.h"
#include "sns_ulog.h"

/*=============================================================================
  Macros
  ===========================================================================*/

#define SNS_BUSY_WAIT_DURATION_NS (1000000)

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef struct sns_sensor_state_init
{
  sns_rc (*init)(sns_fw_sensor *sensor);
  void (*deinit)(sns_fw_sensor *sensor);
  bool (*unavail)(sns_fw_sensor *sensor);
} sns_sensor_state_init;

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/

/* Complete list of all registered libraries */
static sns_isafe_list libraries SNS_SECTION(".data.sns");
/* Lock to be held whenever adding or removing entries from libraries list */
static sns_osa_lock libraries_lock SNS_SECTION(".data.sns");

/* Used for all sns_sensor_api functions */
// PEND: Return to static after sns_fw_printf is made a real sensor
sns_sensor_cb sensor_cb SNS_SECTION(".data.sns");

/* Maximum size allowed for a Sensor to request for its state buffer */
static const uint32_t state_len_max SNS_SECTION(".rodata.sns") = 1200;

/**
 * Each entry is called once per sensor init/deinit.
 * Called functions are intended to: deinitialize any state stored within the
 * sensor, and/or remove any separately managed state associated
 * with this Sensor.
 */
static sns_sensor_state_init state_init[] = {
    {.init = NULL, .deinit = &sns_diag_svc_sensor_deinit, .unavail = NULL},
    {.init = &sns_attr_svc_sensor_init,
     .deinit = &sns_attr_svc_sensor_deinit,
     .unavail = NULL},
    {.init = NULL, .deinit = NULL, .unavail = &sns_pwr_rail_svc_remove_client},
};

/* Temporary storage array; used by sns_sensor_foreach;
 * protected by libraries_lock */
static sns_sensor_library *tmp_libraries[10];

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/* See sns_sensor_cb::get_service_manager */
SNS_SECTION(".text.sns")
static struct sns_service_manager *get_service_manager(sns_sensor const *this)
{
  UNUSED_VAR(this);
  return (sns_service_manager *)sns_service_manager_ref();
}

/* See sns_sensor_cb::get_sensor_instances */
SNS_SECTION(".text.sns")
static sns_sensor_instance *get_sensor_instance(sns_sensor const *this,
                                                bool first)
{
  sns_fw_sensor *sensor = (sns_fw_sensor *)this;
  sns_fw_sensor_instance *rv = NULL;
  sns_isafe_list_iter *iter = &sensor->instances_iter;
  sns_isafe_list_item *item = NULL;

  while(NULL == rv)
  {
    if(first)
    {
      sns_isafe_list_iter_init(iter, &sensor->sensor_instances, true);
      sns_isafe_list_iter_set_always_check_island_ptrs(iter, true);
      item = sns_isafe_list_iter_curr(iter);
      first = false;
    }
    else if(NULL != iter->list)
    {
      item = sns_isafe_list_iter_advance(iter);
    }

    if(NULL != item)
    {
      rv = (sns_fw_sensor_instance *)sns_isafe_list_item_get_data(item);
      if(rv->inst_removing != SNS_INST_REMOVING_NOT_START)
        rv = NULL;
    }
    else
      break;
  }

  if(NULL != rv && SNS_ISLAND_STATE_IN_ISLAND != rv->island_operation)
  {
    SNS_ISLAND_EXIT();
  }

  return &rv->instance;
}

/* See sns_sensor_cb::get_library_sensors */
SNS_SECTION(".text.sns")
static sns_sensor *get_library_sensor(sns_sensor const *this, bool first)
{
  sns_fw_sensor *sensor = (sns_fw_sensor *)this;
  sns_isafe_list_iter *iter = &sensor->library->sensors_iter;
  sns_sensor *rv = NULL;
  sns_isafe_list_item *item = NULL;

  if(first)
  {
    sns_isafe_list_iter_init(iter, &sensor->library->sensors, true);
    sns_isafe_list_iter_set_always_check_island_ptrs(iter, true);
    item = sns_isafe_list_iter_curr(iter);
  }
  else if(NULL != iter->list)
  {
    item = sns_isafe_list_iter_advance(iter);
  }

  if(NULL != item)
    rv = (sns_sensor *)sns_isafe_list_item_get_data(item);

  return rv;
}

/* See sns_sensor_cb::get_registration_index */
static uint32_t get_registration_index(sns_sensor const *this)
{
  return ((sns_fw_sensor *)this)->library->registration_index;
}

/**
 * Determines whether the Sensor associated with the specified API should
 * be eligible to be placed in Island memory.
 */
SNS_SECTION(".text.sns")
bool sns_is_island_sensor_instance(
    struct sns_sensor_instance_api const *instance_api)
{
  return (NULL != instance_api &&
          (sns_island_is_island_ptr((intptr_t)instance_api) &&
           sns_island_is_island_ptr((intptr_t)instance_api->notify_event)));
}

/**
 * Determines whether the Sensor associated with the specified API should
 * be eligible to be placed in Island memory.
 */
SNS_SECTION(".text.sns")
bool sns_is_island_sensor(struct sns_sensor_api const *sensor_api,
                          struct sns_sensor_instance_api const *instance_api)
{
  return sns_island_is_island_ptr((intptr_t)sensor_api) &&
         sns_island_is_island_ptr((intptr_t)sensor_api->get_sensor_uid) &&
         (NULL == instance_api ||
          (sns_island_is_island_ptr((intptr_t)instance_api) &&
           sns_island_is_island_ptr((intptr_t)instance_api->notify_event)));
}

/**
 * Updates the island state of all instances associated with a sensor.
 *
 * @param[i] sensor Sensor to be updated
 * @param[i] enabled True if island mode will be enabled for use
 */
SNS_SECTION(".text.sns")
static void set_all_instance_island_state(sns_fw_sensor *sensor, bool enabled)
{
  sns_fw_sensor_instance *instance;

  for(instance =
          (sns_fw_sensor_instance *)sensor->sensor.cb->get_sensor_instance(
              &sensor->sensor, true);
      NULL != instance;
      instance =
          (sns_fw_sensor_instance *)sensor->sensor.cb->get_sensor_instance(
              &sensor->sensor, false))
  {
    sns_sensor_instance_set_island(instance, enabled);
  }
}

/**
 * Add a library to the temporary array.  Do not invoke this function
 * unless array has at least one empty spot.  If library is already present,
 * move to end of array (and shift-up others as necessary).
 */
static void add_library(sns_sensor_library *library)
{
  sns_sensor_library *temp = NULL;

  for(int i = 0; i < ARR_SIZE(tmp_libraries); i++)
    if(tmp_libraries[i] == library)
      tmp_libraries[i] = NULL;

  for(int i = ARR_SIZE(tmp_libraries) - 1; i >= 0; i--)
  {
    temp = tmp_libraries[i];
    tmp_libraries[i] = library;

    library = temp;
    if(NULL == temp)
      break;
  }
}

/**
 * Utility function to be used in conjunction with sns_sensor_foreach.
 * Get the next library and lock its mutex.  May be blocking.
 *
 * @param[io] iter Initialized iterator over a list of libraries
 *
 * @return Library, or NULL when all libraries have been processed
 */
static sns_sensor_library *get_next_library(sns_isafe_list_iter *iter)
{
  sns_sensor_library *library = NULL;
  uint32_t attempts = 0;

  while(NULL == library)
  {
    uint32_t arr_len = 0;

    for(int i = 0; i < ARR_SIZE(tmp_libraries); i++)
      if(NULL != tmp_libraries[i])
        arr_len++;

    if(arr_len != ARR_SIZE(tmp_libraries) &&
       NULL != sns_isafe_list_iter_curr(iter))
    {
      library = (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(iter);
      sns_isafe_list_iter_advance(iter);
    }
    else
    {
      for(int i = 0; i < ARR_SIZE(tmp_libraries); i++)
      {
        if(NULL != tmp_libraries[i])
        {
          library = tmp_libraries[i];
          tmp_libraries[i] = NULL;
          break;
        }
      }
      attempts++;
    }

    if(NULL == library)
      break;

    if(library->is_multithreaded)
    {
      if(atomic_load(&library->exec_count))
      {
        // If library execution count check fails, and we either don't have any
        // more room on the temporary array, or are out of libraries from the
        // iterator
        if(attempts >= arr_len && (arr_len == ARR_SIZE(tmp_libraries) ||
                                   NULL == sns_isafe_list_iter_curr(iter)))
        {
          while(atomic_load(&library->exec_count))
          {
            sns_busy_wait(sns_convert_ns_to_ticks(SNS_BUSY_WAIT_DURATION_NS));
          }
          break;
        }
        add_library(library);
        library = NULL;
      }
    }
    else
    {
      if(SNS_RC_SUCCESS != sns_osa_lock_try_acquire(library->library_lock))
      {
        // If try-lock fails, and we either don't have any more room on the
        // temporary array, or are out of libraries from the iterator
        if(attempts >= arr_len && (arr_len == ARR_SIZE(tmp_libraries) ||
                                   NULL == sns_isafe_list_iter_curr(iter)))
        {
          sns_osa_lock_acquire(library->library_lock);
          break;
        }
        add_library(library);
        library = NULL;
      }
    }
  }

  return library;
}

/**
 * Check whether the SUID contained in attr_info matches the provided SUID.
 *
 * @see sns_attr_svc_sensor_foreach_func
 *
 * @return False if the two SUIDs are a match; if the SUIDs do not match,
 *         return true.
 */
static bool suid_match(struct sns_attribute_info *attr_info, void *arg)
{
  sns_sensor_uid suid;
  sns_sensor_uid *suid_arg = (sns_sensor_uid *)arg;

  suid = sns_attr_info_get_suid(attr_info);
  if(sns_sensor_uid_compare(&suid, suid_arg))
    return false;

  return true;
}

/*
 * Call sensor init for the allocated sensor
 */
static sns_rc sns_sensor_start(sns_fw_sensor *sensor)
{
  sns_rc rc = sensor->sensor.sensor_api->init((sns_sensor *)sensor);
  if(SNS_RC_SUCCESS != rc)
  {
    SNS_PRINTF(MED, sns_fw_printf,
               "Initialization failed for sensor " SNS_DIAG_PTR, sensor);
    sns_sensor_deinit(sensor);
  }
  else
  {
    // If island memory vote cb function is NULL, assume it's ssc island sensor
    // Update island memory vote cb function for ssc sensors
    if((true == sns_is_island_sensor_instance(sensor->sensor.instance_api) ||
        true == sns_is_island_sensor(sensor->sensor.sensor_api,
                                     sensor->sensor.instance_api)) &&
       (NULL == sensor->island_memory_vote_cb))
    {
      sensor->island_memory_vote_cb = &ssc_island_memory_vote;
    }
  }

  return rc;
}

/*
 * Allocate memory for fw_sensor and sns_sensor state
 */
static sns_rc
sns_sensor_init_common(sns_sensor_library *library, uint32_t state_len,
                       struct sns_sensor_api const *sensor_api,
                       struct sns_sensor_instance_api const *instance_api)
{
  sns_rc rv = SNS_RC_SUCCESS;

  if(state_len > state_len_max)
  {
    SNS_ULOG_INIT_SEQ_ERR("Init:     %X/0 api=%X len=%u", library, sensor_api,
                          state_len);
    SNS_PRINTF(ERROR, sns_fw_printf, "Init: %X/0 api=%X len=%u", library,
               sensor_api, state_len);
    rv = SNS_RC_POLICY;
  }
  else
  {
    sns_isafe_list_iter iter;
    sns_fw_sensor *sensor = NULL;
    void *sensor_state_ptr = NULL;
    void *attribute_info_ptr = NULL;
    bool island_sensor = sns_is_island_sensor(sensor_api, instance_api);
    uint8_t attr_idx = 0;
    size_t sensor_state_size = 0;
    size_t fw_sensor_size = 0;
    sns_cstruct_extn extn;
    sns_mem_heap_id heap_id;
    sns_mem_segment mem_segment = library->mem_segment;

    /**
     * Memory Allocation logic for sns_fw_sensor and sns_sensor_state structures
     * will be on following Cases 1.if island_sensor== false and
     * island_sensor_instance== true then place sns_fw_sensor on island and
     * sns_sensor_state on big image 2.if island_sensor== true then place both
     * on island 3.if memory allocation is incomplete in above two conditions,
     * allocate both structures on big image
     *
     */

    fw_sensor_size = sns_cstruct_extn_compute_total_size(
        sizeof(*sensor), 1, ALIGN_8(sizeof(struct sns_attribute_info)));
    sensor_state_size = ALIGN_8(sizeof(struct sns_sensor_state) + state_len);

    if(!island_sensor && sns_is_island_sensor_instance(instance_api))
    {
      /*Case 1*/
      sensor = sns_malloc(SNS_HEAP_ISLAND, fw_sensor_size);
      if(sensor != NULL)
      {
        sensor_state_ptr = sns_malloc(SNS_HEAP_MAIN, sensor_state_size);
      }
    }
    else if(island_sensor)
    {
      heap_id = sns_memmgr_get_sensor_heap_id(mem_segment);

      /*Case 2*/
      sensor = sns_malloc(SNS_HEAP_ISLAND, fw_sensor_size);
      if(sensor != NULL)
      {
        sensor_state_ptr = sns_malloc(heap_id, sensor_state_size);
        if(NULL == sensor_state_ptr)
        {
          sns_free(sensor);
          sensor = NULL;
        }
      }
    }

    if(!sensor)
    {
      /*Case 3*/
      sensor = sns_malloc(SNS_HEAP_MAIN, fw_sensor_size);
      sensor_state_ptr = sns_malloc(SNS_HEAP_MAIN, sensor_state_size);
    }

    if(library->is_multithreaded && NULL != sensor && NULL != sensor_state_ptr)
    {
      sns_osa_lock_attr lattr;
      sns_osa_lock_attr_init(&lattr);
      sns_osa_lock_attr_set_memory_partition(
          &lattr,
          !island_sensor ? SNS_OSA_MEM_TYPE_NORMAL : SNS_OSA_MEM_TYPE_ISLAND);
      sns_osa_lock_create(&lattr, &sensor->sensor_instances_list_lock);
      if(NULL == sensor->sensor_instances_list_lock)
      {
        sns_free(sensor);
        sensor = NULL;
        sns_free(sensor_state_ptr);
        sensor_state_ptr = NULL;
      }
    }

    if(NULL != sensor && NULL != sensor_state_ptr)
    {
      sns_cstruct_extn_init(&extn, sensor, sizeof(*sensor), 1);
      attr_idx = sns_cstruct_extn_setup_buffer(
          &extn, sizeof(struct sns_attribute_info));
      attribute_info_ptr = sns_cstruct_extn_get_buffer(&extn, attr_idx);
      SNS_ASSERT(NULL != attribute_info_ptr);

      SNS_FW_DBG_LOG("Init:     %X/%X", library, sensor);
      sensor->attr_info = (struct sns_attribute_info *)attribute_info_ptr;
      sensor->sensor.state = (struct sns_sensor_state *)sensor_state_ptr;
      sensor->sensor.state->state_len = state_len;
      sensor->sensor.cb = &sensor_cb;
      sensor->sensor.sensor_api = sensor_api;
      sensor->sensor.instance_api = instance_api;

      sensor->island_operation = island_sensor ? SNS_ISLAND_STATE_IN_ISLAND
                                               : SNS_ISLAND_STATE_NOT_IN_ISLAND;

      for(uint8_t i = 0; i < ARR_SIZE(state_init); i++)
      {
        if(NULL != state_init[i].init)
        {
          state_init[i].init(sensor);
        }
      }

      sensor->removing = false;
      sensor->library = library;
      sensor->diag_config.config = default_datatype;

      sns_isafe_list_init(&sensor->data_streams);
      sns_isafe_list_init(&sensor->sensor_instances);
      sns_isafe_list_item_init(&sensor->list_entry, sensor);

      sns_osa_lock_acquire(library->library_lock);
      sns_isafe_list_iter_init(&iter, &library->sensors, island_sensor);
      sns_isafe_list_iter_insert(&iter, &sensor->list_entry, !island_sensor);
      sns_osa_lock_release(library->library_lock);

      rv = sns_sensor_start(sensor);
    }
    else
    {
      rv = SNS_RC_FAILED;
    }
  }
  return rv;
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_sensor_init_fw(void)
{
  sns_rc rc;
  sns_osa_lock_attr attr;
  sns_isafe_list_init(&libraries);

  rc = sns_osa_lock_attr_init(&attr);
  rc |= sns_osa_lock_init(&attr, &libraries_lock);

  if(SNS_RC_SUCCESS == rc)
  {
    sensor_cb = (sns_sensor_cb){
        .struct_len = sizeof(sensor_cb),
        .get_service_manager = &get_service_manager,
        .get_sensor_instance = &get_sensor_instance,
        .create_instance = &sns_sensor_instance_init,
        .remove_instance = &sns_sensor_instance_deinit,
        .get_library_sensor = &get_library_sensor,
        .get_registration_index = &get_registration_index,
        .create_instance_v2 = &sns_sensor_instance_init_v2,
    };
  }
  else
  {
    sns_osa_lock_deinit(&libraries_lock);
  }

  return rc;
}

// Constant value used for validating library
static const uint32_t
    dynamic_lib_magic_number SNS_SECTION(".rodata.sns") = 0xCAFECAFE;
void sns_sensor_dynamic_library_add_checksum(sns_sensor_library *library)
{
  if(NULL != library && library->is_dynamic_lib)
  {
    library->dynamic_lib_integrity =
        (dynamic_lib_magic_number ^ (uint32_t)library->creation_ts ^
         (uint32_t)library->sensors.cnt ^
         (uint32_t)(uintptr_t)library->library_lock ^
         (uint32_t)(uintptr_t)library);
  }
}

SNS_SECTION(".text.sns")
bool sns_sensor_dynamic_library_validate(sns_sensor_library *library)
{
  bool lib_valid = false;
  if(NULL != library)
  {
    lib_valid =
        (0 == (dynamic_lib_magic_number ^ (uint32_t)library->creation_ts ^
               (uint32_t)library->sensors.cnt ^
               (uint32_t)(uintptr_t)library->library_lock ^
               (uint32_t)(uintptr_t)library ^ library->dynamic_lib_integrity));
  }
  return lib_valid;
}

SNS_SECTION(".text.sns")
void sns_sensor_print_lib_stats(void)
{
#ifdef SNS_TMGR_TASK_DEBUG
  sns_sensor_library *cur_lib = NULL;
  sns_isafe_list_iter lib_iter;
  sns_osa_lock_acquire(libraries_lock);
  for(sns_isafe_list_iter_init(&lib_iter, &libraries, true);
      NULL !=
      (cur_lib =
           (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(&lib_iter));
      sns_isafe_list_iter_advance(&lib_iter))
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "Library : 0x" SNS_DIAG_PTR ", is_multithreaded : %d, "
               "num_tasks : %u, queue_ts_min : %x%08x, queue_ts_max : %x%08x, "
               "queue_ts_avg = %x%08x, ",
               cur_lib, cur_lib->is_multithreaded, cur_lib->lib_stats.num_tasks,
               (uint32_t)(cur_lib->lib_stats.queue_time.min >> 32),
               (uint32_t)cur_lib->lib_stats.queue_time.min,
               (uint32_t)(cur_lib->lib_stats.queue_time.max >> 32),
               (uint32_t)cur_lib->lib_stats.queue_time.max,
               (uint32_t)(cur_lib->lib_stats.queue_time.avg >> 32),
               (uint32_t)cur_lib->lib_stats.queue_time.avg);
    SNS_PRINTF(
        ERROR, sns_fw_printf,
        "exec_ts_min : %x%08x, exec_ts_max : %x%08x, exec_ts_avg : %x%08x",
        (uint32_t)(cur_lib->lib_stats.exec_time.min >> 32),
        (uint32_t)cur_lib->lib_stats.exec_time.min,
        (uint32_t)(cur_lib->lib_stats.exec_time.max >> 32),
        (uint32_t)cur_lib->lib_stats.exec_time.max,
        (uint32_t)(cur_lib->lib_stats.exec_time.avg >> 32),
        (uint32_t)cur_lib->lib_stats.exec_time.avg);
  }
  sns_osa_lock_release(libraries_lock);
#endif
}

sns_sensor_library *sns_sensor_library_init(sns_register_sensors register_func,
                                            uint32_t registration_index,
                                            bool is_island_lib,
                                            bool is_multithreaded,
                                            sns_mem_segment mem_segment)
{
  sns_isafe_list_iter iter;
  sns_osa_lock_attr attr;
  sns_rc rc;
  sns_sensor_library *library = NULL;
  if(is_island_lib)
    library = sns_malloc(SNS_HEAP_ISLAND, sizeof(*library));
  else
    library = sns_malloc(SNS_HEAP_MAIN, sizeof(*library));

  if(NULL == library)
  {
    SNS_ULOG_INIT_SEQ_ERR("%s: malloc failed, reg_func=%X", __FUNCTION__,
                          register_func);
    return NULL;
  }

  SNS_PRINTF(HIGH, sns_fw_printf,
             "Initializing new sensor library 0x" SNS_DIAG_PTR
             " is_multithreaded : %d",
             library, is_multithreaded);
  library->creation_ts = sns_get_system_time();
  sns_isafe_list_item_init(&library->list_entry, library);

  sns_isafe_list_init(&library->sensors);
  library->register_func = register_func;
  library->registration_index = registration_index;
  library->is_multithreaded = is_multithreaded;
  library->exec_count = 0;
  library->sensor_task_count.count = 0;
  library->mem_segment = mem_segment;
#ifdef SNS_TMGR_TASK_DEBUG
  library->lib_stats.num_tasks = 0;
  library->lib_stats.queue_time.avg = 0;
  library->lib_stats.queue_time.max = 0;
  library->lib_stats.queue_time.min = UINT64_MAX;
  library->lib_stats.exec_time.avg = 0;
  library->lib_stats.exec_time.max = 0;
  library->lib_stats.exec_time.min = UINT64_MAX;
#endif
  atomic_store(&library->removing, SNS_LIBRARY_INIT);

  rc = sns_osa_lock_attr_init(&attr);

  if(is_island_lib)
    rc |=
        sns_osa_lock_attr_set_memory_partition(&attr, SNS_OSA_MEM_TYPE_ISLAND);
  else
    rc |=
        sns_osa_lock_attr_set_memory_partition(&attr, SNS_OSA_MEM_TYPE_NORMAL);

  if(!library->is_multithreaded)
  {
    rc |= sns_osa_lock_create(&attr, &library->library_lock);
  }
  if(SNS_RC_SUCCESS != rc)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "failed to create lib lock, reg_func=%X",
               register_func);
    SNS_ULOG_INIT_SEQ_ERR("%s: failed to create lib lock, reg_func=%X",
                          __FUNCTION__, register_func);
    sns_free(library);
    return NULL;
  }
  sns_osa_lock_acquire(&libraries_lock);
  sns_isafe_list_iter_init(&iter, &libraries, false);
  sns_isafe_list_iter_insert(&iter, &library->list_entry, true);
  sns_osa_lock_release(&libraries_lock);

  return library;
}

sns_rc sns_sensor_library_deinit(sns_sensor_library *library)
{
  sns_isafe_list_iter iter;
  sns_fw_sensor *sensor;

  SNS_FW_DBG_LOG("library_deinit: %X, removing=%u #sensors=%u is_dyn=%u",
                 library, atomic_load(&library->removing), library->sensors.cnt,
                 library->is_dynamic_lib);

  if(SNS_LIBRARY_REMOVE_START > atomic_load(&library->removing))
  {
    for(sns_isafe_list_iter_init(&iter, &library->sensors, true);
        NULL !=
        (sensor = (sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(&iter));)
    {
      // Advance to next before deiniting current as deinit would remove it from
      // the list.
      sns_isafe_list_iter_advance(&iter);
      sns_sensor_deinit(sensor);
    }
    atomic_store(&library->removing, SNS_LIBRARY_REMOVE_START);
    SNS_FW_DBG_LOG(
        "library_deinit: Library removal started... removing_state=%d",
        atomic_load(&library->removing));
  }

  sns_isafe_list_iter_init(&iter, &library->sensors, true);
  if(0 == sns_isafe_list_iter_len(&iter))
  {
    if(library->is_dynamic_lib)
    {
      sns_sensor_signal_registry_task(SNS_REGISTRY_DL_LOAD_SIGNAL);
    }
    atomic_store(&library->removing, SNS_LIBRARY_REMOVE_COMPLETE);
    SNS_FW_DBG_LOG("library_deinit: Signal Library Delete... removing_state=%d",
                   atomic_load(&library->removing));
    sns_sensor_signal_registry_task(SNS_REGISTRY_LIB_DELETE_SIGNAL);
  }

  return SNS_RC_SUCCESS;
}

/**
 *
 */
bool sns_sensor_library_has_physical_sensor(sns_sensor_library *library)
{
  bool found = false;
  sns_fw_sensor *sensor = NULL;
  sns_isafe_list_iter iter;

  sns_osa_lock_acquire(library->library_lock);
  for(sns_isafe_list_iter_init(&iter, &library->sensors, true);
      NULL != (sensor =
                   (sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(&iter)) &&
      !found;
      sns_isafe_list_iter_advance(&iter))
  {
    found |= sns_attr_info_get_is_physical_sensor(sensor->attr_info);
  }
  sns_osa_lock_release(library->library_lock);
  return found;
}

bool sns_sensor_populate_suid(sns_fw_sensor *sensor)
{
  bool ret = true;
  if(NULL != sensor)
  {
    sns_sensor_uid empty_suid = (sns_sensor_uid){.sensor_uid = {0}};
    sns_sensor_uid suid = sns_attr_info_get_suid(sensor->attr_info);

    if(sns_sensor_uid_compare(&empty_suid, &suid))
    {
      sensor->diag_config.suid =
          sensor->sensor.sensor_api->get_sensor_uid((sns_sensor *)sensor);

      SNS_PRINTF(MED, sns_fw_printf,
                 "Populating sensor " SNS_DIAG_PTR
                 " in island: %d, with SUID %" PRIsuid,
                 sensor, sensor->island_operation,
                 SNS_PRI_SUID(sensor->diag_config.suid));

      if(sns_sensor_uid_compare(sensor->diag_config.suid, &empty_suid) ||
         !sns_attr_svc_sensor_foreach(&suid_match,
                                      (void *)sensor->diag_config.suid))
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "Invalid SUID for sensor " SNS_DIAG_PTR, sensor);
        ret = false;
      }
      else
      {
        sns_attr_info_set_suid(sensor->attr_info, sensor->diag_config.suid);
      }
    }
  }
  else
  {
    ret = false;
  }
  return ret;
}

sns_rc sns_sensor_init(uint32_t state_len,
                       struct sns_sensor_api const *sensor_api,
                       struct sns_sensor_instance_api const *instance_api)
{
  sns_rc rv = SNS_RC_FAILED;
  sns_isafe_list_iter iter;
  sns_sensor_library *library;
  // The current library will always be at the end of the list
  sns_isafe_list_iter_init(&iter, &libraries, false);
  library = (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(&iter);
  if(NULL != library)
  {
    rv = sns_sensor_init_common(library, state_len, sensor_api, instance_api);
  }

  return rv;
}

sns_rc sns_sensor_create(const sns_sensor *sensor, uint32_t state_len,
                         struct sns_sensor_api const *sensor_api,
                         struct sns_sensor_instance_api const *instance_api)
{
  sns_rc rv = SNS_RC_FAILED;
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
  if(NULL != fw_sensor)
  {
    rv = sns_sensor_init_common(fw_sensor->library, state_len, sensor_api,
                                instance_api);
  }

  return rv;
}

/**
 *
 * Frees the given sensor's instances, requests, and resources
 *
 * @param[i] sensor
 *
 * @return number of instances marked for removal.
 */
int sns_sensor_clean_up(sns_fw_sensor *sensor)
{
  int instances = 0;
  int resources = 0;
  int num_requests = 0;
  sns_sensor_instance *instance;
  char data_type[32];
  bool get_first_instance = true;

  sns_attr_info_get_data_type(sensor->attr_info, data_type, sizeof(data_type));

  while(NULL != (instance = sensor->sensor.cb->get_sensor_instance(
                     &sensor->sensor, get_first_instance)))
  {
    num_requests = sns_sensor_instance_remove_all_requests(
        (sns_fw_sensor_instance *)instance);
    if(num_requests > 0)
    {
      instances++;
      get_first_instance = true;
    }
    else
    {
      get_first_instance = false;
      sns_sensor_instance_deinit(instance);
    }
  }

  for(uint8_t i = 0; i < ARR_SIZE(state_init); i++)
  {
    if(NULL != state_init[i].unavail && state_init[i].unavail(sensor))
    {
      resources++;
    }
  }

  // clear if there are any outstanding memory service allocations
  sns_memory_service_cleanup(sensor);

  SNS_FW_DBG_LOG("Cleanup:  %08X, #inst = %u, resources = %u", sensor,
                 instances, resources);
  return instances;
}

/**
 * As with removing Sensor Instances, removing Sensors is typically an
 * asynchronous process.  First we must start the deinitiation process
 * for every active stream, as well as every active Instance.  Upon the
 * asynchronous completion of the last deinitiation, this Sensor will
 * be removed.
 */
sns_rc sns_sensor_deinit(sns_fw_sensor *sensor)
{
  if(sns_isafe_list_item_present(&sensor->list_entry) && !sensor->removing)
  {
    sns_isafe_list_iter iter;
    bool freeable = true;
    char data_type[32];

    sns_attr_info_get_data_type(sensor->attr_info, data_type,
                                sizeof(data_type));

    SNS_SPRINTF(HIGH, sns_fw_printf, "sns_sensor_deinit " SNS_DIAG_PTR " %s",
                sensor, data_type);
    SNS_FW_DBG_LOG("Deinit:   %X / %X", sensor->library, sensor);

    sensor->removing = true;
    sns_attr_info_set_available(sensor->attr_info, false);

    sns_attr_info_get_data_type(sensor->attr_info, data_type,
                                sizeof(data_type));
    sns_suid_sensor_add_task(
        data_type); // informs existing clients about change in availability

    if(0 < sns_sensor_clean_up(sensor)) // stops service to existing clients
    {
      freeable = false;
    }

    // initiate the sensor deletion
    if(NULL != sensor->sensor.sensor_api &&
       NULL != sensor->sensor.sensor_api->deinit)
    {
      sensor->sensor.sensor_api->deinit((sns_sensor *)sensor);
    }

    sns_isafe_list_iter_init(&iter, &sensor->data_streams, true);
    if(0 < sns_isafe_list_iter_len(&iter))
    {
      sns_service_manager *svc_mgr = get_service_manager(&sensor->sensor);
      sns_stream_service *stream_svc =
          (sns_stream_service *)svc_mgr->get_service(svc_mgr,
                                                     SNS_STREAM_SERVICE);

      for(sns_isafe_list_iter_init(&iter, &sensor->data_streams, true);
          NULL != sns_isafe_list_iter_curr(&iter);
          sns_isafe_list_iter_advance(&iter))
      {
        sns_data_stream *stream = sns_isafe_list_iter_get_curr_data(&iter);
        // discards from stream all existing events other than DESTROY_COMPLETE
        // event
        for(sns_sensor_event *event = stream->api->peek_input(stream);
            NULL != event &&
            SNS_FW_MSGID_SNS_DESTROY_COMPLETE_EVENT != event->message_id;
            event = stream->api->get_next_input(stream))
        {
        }
        stream_svc->api->remove_stream(
            stream_svc, stream); // removes own dependency on other sensors
      }
      freeable = false;
    }

    if(freeable)
    {
      sns_sensor_delete(sensor);
    }
  }

  return SNS_RC_SUCCESS;
}

static void __attribute__((noinline))
sns_sensor_delete_internal(sns_fw_sensor *sensor)
{
  sns_isafe_list_iter iter;
  sns_sensor_library *library = sensor->library;
  char data_type[32];
  sns_attr_info_get_data_type(sensor->attr_info, data_type, sizeof(data_type));

  SNS_FW_DBG_LOG("sns_sensor_delete: %X/%X api=%X phys=%u", library, sensor,
                 sensor->sensor.sensor_api,
                 sensor->attr_info->is_physical_sensor);
  SNS_SPRINTF(HIGH, sns_fw_printf, "sns_sensor_delete " SNS_DIAG_PTR " %s",
              sensor, data_type);

  for(uint8_t i = 0; i < ARR_SIZE(state_init); i++)
  {
    if(NULL != state_init[i].deinit)
    {
      state_init[i].deinit(sensor);
    }
  }

  if(sensor->library->is_multithreaded)
  {
    sns_osa_lock_delete(sensor->sensor_instances_list_lock);
  }

  sns_isafe_list_item_remove(&sensor->list_entry);
  sns_free(sensor->sensor.state);
  sns_free(sensor);

  sns_isafe_list_iter_init(&iter, &library->sensors, true);
  if(0 == sns_isafe_list_iter_len(&iter))
  {
    sns_sensor_library_deinit(library);
  }
}

SNS_SECTION(".text.sns") bool sns_sensor_delete(sns_fw_sensor *sensor)
{
  sns_isafe_list_iter instance_iter, stream_iter;
  if(!sns_island_is_island_ptr((intptr_t)sensor))
  {
    SNS_ISLAND_EXIT();
  }
  sns_isafe_list_iter_init(&instance_iter, &sensor->sensor_instances, true);
  sns_isafe_list_iter_init(&stream_iter, &sensor->data_streams, true);
  if(sensor->removing && 0 == sns_isafe_list_iter_len(&instance_iter) &&
     0 == sns_isafe_list_iter_len(&stream_iter))
  {
    SNS_ISLAND_EXIT();
    sns_sensor_delete_internal(sensor);

    return true;
  }
  return false;
}

bool sns_sensor_foreach(sns_sensor_foreach_func func, void *arg)
{
  bool bail = false;
  sns_isafe_list_iter iter_library, iter_sensor;
  sns_sensor_library *library;

  sns_osa_lock_acquire(&libraries_lock);
  sns_memset(tmp_libraries, 0, sizeof(tmp_libraries));

  sns_isafe_list_iter_init(&iter_library, &libraries, true);
  while(!bail && NULL != (library = get_next_library(&iter_library)))
  {
    for(sns_isafe_list_iter_init(&iter_sensor, &library->sensors, true);
        NULL != sns_isafe_list_iter_curr(&iter_sensor) && !bail;
        sns_isafe_list_iter_advance(&iter_sensor))
    {
      sns_fw_sensor *sensor =
          (sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(&iter_sensor);

      if(!func(sensor, arg))
        bail = true;
    }

    if(!library->is_multithreaded)
    {
      sns_osa_lock_release(library->library_lock);
    }
  }

  sns_osa_lock_release(&libraries_lock);
  return !bail;
}

SNS_SECTION(".text.sns")
bool sns_sensor_set_island(sns_fw_sensor *sensor, bool enabled)
{
  bool rv = false;
  if(!enabled)
  {
    if(SNS_ISLAND_STATE_IN_ISLAND == sensor->island_operation)
    {
      sensor->island_operation = SNS_ISLAND_STATE_ISLAND_DISABLED;
      set_all_instance_island_state(sensor, enabled);
    }
    rv = true;
  }
  else
  {
    if(SNS_ISLAND_STATE_ISLAND_DISABLED == sensor->island_operation)
    {
      sns_isafe_list_iter iter;
      struct sns_fw_data_stream *data_stream;

      rv = true;
      sns_isafe_list_iter_init(&iter, &sensor->data_streams, true);
      for(data_stream = sns_isafe_list_iter_get_curr_data(&iter);
          NULL != data_stream; data_stream = sns_isafe_list_item_get_data(
                                   sns_isafe_list_iter_advance(&iter)))
      {
        if(!sns_island_is_island_ptr((intptr_t)data_stream))
        {
          rv = false;
          break;
        }
      }

      if(rv)
      {
        sensor->island_operation = SNS_ISLAND_STATE_IN_ISLAND;
        set_all_instance_island_state(sensor, enabled);
      }
    }
  }

  return rv;
}

sns_rc sns_sensor_library_get_status(sns_sensor_library **library,
                                     bool *is_empty, bool *is_ready)
{
  sns_rc rc = SNS_RC_NOT_AVAILABLE; /* assume library is no longer present */
  sns_sensor_library *cur_lib = NULL;
  sns_isafe_list_iter lib_iter;

  if(NULL == *library)
  {
    return rc;
  }

  sns_osa_lock_acquire(&libraries_lock);
  for(sns_isafe_list_iter_init(&lib_iter, &libraries, true);
      NULL !=
      (cur_lib =
           (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(&lib_iter));
      sns_isafe_list_iter_advance(&lib_iter))
  {
    if(cur_lib == *library)
    {
      sns_isafe_list_iter sensor_iter;
      sns_fw_sensor *sensor = NULL;

      sns_osa_lock_acquire(cur_lib->library_lock);
      sns_isafe_list_iter_init(&sensor_iter, &cur_lib->sensors, true);
      if(NULL != is_empty)
      {
        *is_empty = (0 == sns_isafe_list_iter_len(&sensor_iter));
      }
      if(NULL != is_ready)
      {
        *is_ready = false;
        if(0 != sns_isafe_list_iter_len(&sensor_iter))
        {
          while(NULL !=
                (sensor = ((sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(
                     &sensor_iter))))
          {
            *is_ready |= sns_attr_info_get_available(sensor->attr_info);
            sns_isafe_list_iter_advance(&sensor_iter);
          }
        }
      }
      sns_osa_lock_release(cur_lib->library_lock);
      rc = SNS_RC_SUCCESS;
      break;
    }
  }
  sns_osa_lock_release(&libraries_lock);

  if(cur_lib != *library)
  {
    SNS_FW_DBG_LOG("library_get_status: %X no longer valid", *library);
    /* The given library is no longer present */
    *library = NULL;
  }
  return rc;
}

void sns_framework_delete_unavailable_phy_sensors(void)
{
  sns_isafe_list_iter iter;
  sns_sensor_library *library;

  SNS_PRINTF(HIGH, sns_fw_printf, "Deleting unavailable phy sensor");
  sns_osa_lock_acquire(&libraries_lock);
  sns_isafe_list_iter_init(&iter, &libraries, true);
  while(NULL !=
        (library =
             (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(&iter)))
  {
    sns_isafe_list_iter sensor_iter;
    sns_fw_sensor *sensor;
    sns_isafe_list_iter_init(&sensor_iter, &library->sensors, true);
    for(; NULL != (sensor = (sns_fw_sensor *)sns_isafe_list_iter_get_curr_data(
                       &sensor_iter));)
    {
      // Advance to next before deiniting current as deinit would remove it from
      // the list.
      sns_isafe_list_iter_advance(&sensor_iter);
      if(!(sensor->attr_info->available) &&
         (sensor->attr_info->is_physical_sensor))
      {
        char data_type[32];
        sns_attr_info_get_data_type(sensor->attr_info, data_type,
                                    sizeof(data_type));
        SNS_SPRINTF(HIGH, sns_fw_printf,
                    "sns_sensor_deinit " SNS_DIAG_PTR " %s", sensor, data_type);
        sns_sensor_deinit(sensor);
      }
    }
    sns_isafe_list_iter_advance(&iter);
  }
  sns_osa_lock_release(&libraries_lock);
}

/** Executes in Registry Task context   */
void sns_framework_delete_empty_libraries(void)
{
  sns_isafe_list_iter iter;
  sns_sensor_library *library;

  sns_osa_lock_acquire(&libraries_lock);
  sns_isafe_list_iter_init(&iter, &libraries, true);
  while(NULL !=
        (library =
             (sns_sensor_library *)sns_isafe_list_iter_get_curr_data(&iter)))
  {
    bool lib_freeable = false;
    uint32_t num_sensors = 0;
    sns_isafe_list_iter sensor_iter;
    bool iter_advance = true;

    if(SNS_LIBRARY_REMOVE_COMPLETE == atomic_load(&library->removing))
    {
      sns_isafe_list_iter_init(&sensor_iter, &library->sensors, true);
      num_sensors = sns_isafe_list_iter_len(&sensor_iter);
      if(0 == num_sensors)
      {
        lib_freeable = sns_thread_manager_remove(library);
        SNS_FW_DBG_LOG("delete_empty_lib: %X dyn=%u freeable=%u", library,
                       library->is_dynamic_lib, lib_freeable);
        if(lib_freeable)
        {
          if(library->is_dynamic_lib)
          {
            sns_sensor_signal_registry_task(SNS_REGISTRY_DL_LOAD_SIGNAL);
          }
          if(NULL != library->library_lock)
          {
            sns_osa_lock_delete(library->library_lock);
          }
          sns_isafe_list_iter_return(&iter);
          sns_isafe_list_item_remove(&library->list_entry);
          SNS_FW_DBG_LOG("delete_empty_lib: Freeing Library");
          sns_free(library);
          iter_advance = false;
        }
        else
        {
          sns_sensor_signal_registry_task(SNS_REGISTRY_LIB_DELETE_SIGNAL);
        }
      }
    } // SNS_LIBRARY_REMOVE_COMPLETE
    if(iter_advance)
    {
      sns_isafe_list_iter_advance(&iter);
    }
  }
  sns_osa_lock_release(&libraries_lock);
}
/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
uint16_t sns_get_sensor_active_req_cnt(sns_fw_sensor *sensor)
{
  uint16_t active_req_cnt = 0;
  sns_fw_sensor_instance *instance;
  sns_client_req_list *client_req_list;

  sns_isafe_list_iter iter_instances;
  sns_isafe_list_iter iter_client_req_lists;

  sns_osa_lock_acquire(sensor->sensor_instances_list_lock);
  // Loop over all the sensor instances
  for(sns_isafe_list_iter_init(&iter_instances, &sensor->sensor_instances,
                               true);
      NULL != sns_isafe_list_iter_curr(&iter_instances);
      sns_isafe_list_iter_advance(&iter_instances))
  {
    instance = (sns_fw_sensor_instance *)sns_isafe_list_iter_get_curr_data(
        &iter_instances);

    // Loop over all the client req lists
    for(sns_isafe_list_iter_init(&iter_client_req_lists,
                                 &instance->client_req_lists, true);
        NULL != sns_isafe_list_iter_curr(&iter_client_req_lists);
        sns_isafe_list_iter_advance(&iter_client_req_lists))
    {
      client_req_list =
          (sns_client_req_list *)sns_isafe_list_iter_get_curr_data(
              &iter_client_req_lists);

      active_req_cnt += client_req_list->client_requests.cnt;
    }
  }
  sensor->active_req_cnt = active_req_cnt;
  sns_osa_lock_release(sensor->sensor_instances_list_lock);
  return active_req_cnt;
}
/*----------------------------------------------------------------------------*/
// Return TRUE if sensor is placed in qshtech island memory pool
SNS_SECTION(".text.sns")
static bool is_sensor_placed_in_qshtech_island(sns_sensor *sensor)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
  if((fw_sensor->library->mem_segment != SNS_MEM_SEG_QSH) &&
     (fw_sensor->library->mem_segment != SNS_MEM_SEG_SSC))
  {
    return true;
  }
  return false;
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns")
void sns_vote_sensor_island_mode(sns_sensor *sensor, sns_ssc_island_vote vote)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;

  bool enable = (SNS_ISLAND_ENABLE == vote) ? true : false;

  if((true == sns_is_island_sensor_instance(fw_sensor->sensor.instance_api) ||
      true == sns_is_island_sensor(fw_sensor->sensor.sensor_api,
                                   fw_sensor->sensor.instance_api)))
  {
    // vote for qsh island memory if any island sensor is active
    qsh_island_memory_vote(sensor, enable);

    if(NULL != fw_sensor->island_memory_vote_cb)
    {
      if(true == is_sensor_placed_in_qshtech_island(sensor))
      {
        // for non ssc sensors exit island before calling callback function.
        SNS_ISLAND_EXIT();

        // Vote for qshtech island, if sensor is placed in qshtech island heap
        sns_qshtech_island_memory_vote(sensor, enable);
      }
      fw_sensor->island_memory_vote_cb(sensor, enable);
    }
    else
    {
      SNS_PRINTF(
          ERROR, sns_fw_printf,
          "island_memory_vote_cb() func is not registered for sensor(0x%x)",
          sensor);
    }
  }
}
/*----------------------------------------------------------------------------*/

SNS_SECTION(".text.sns") void sns_isafe_sensor_access(sns_fw_sensor *fw_sensor)
{
  // Exit island if sensor is big image sensor
  if(false == sns_island_is_island_ptr((intptr_t)fw_sensor))
  {
    SNS_ISLAND_EXIT();
  }
  else
  {
    // check if sensor is ssc island sensor
    if(&ssc_island_memory_vote == fw_sensor->island_memory_vote_cb)
    {
      // Exit island if any other ssc island sensor use case is not active.
      if(false == is_ssc_island_usecase_active())
      {
        SNS_ISLAND_EXIT();
      }
    }
    // Exit island if there are no requests active to this non ssc sensor
    else if(0 == fw_sensor->active_req_cnt)
    {
      SNS_ISLAND_EXIT();
    }
  }
}
/*----------------------------------------------------------------------------*/
