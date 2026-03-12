/**============================================================================
  @file sns_memory_service.c

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include <string.h>
#include "sns_fw_memory_service.h"
#include "sns_fw_sensor.h"
#include "sns_island_config.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_memory_service.h"
#include "sns_printf.h"
#include "sns_register.h"
#include "sns_types.h"

/*=============================================================================
  Macros and constants
  ===========================================================================*/

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef struct sns_mem_seg_mem_pool_map
{
  sns_mem_segment mem_segment;
  char pool_name[SNS_MAX_MEM_POOL_NAME_LEN];

} sns_mem_seg_mem_pool_map;

/*=============================================================================
  Forward Declarations
  ===========================================================================*/
static void *instance_malloc(struct sns_sensor_instance *, size_t);

static void instance_free(struct sns_sensor_instance *, void *);

static void register_island_memory_vote_cb(sns_sensor *const,
                                           sns_island_memory_vote_cb);

static void *instance_malloc_island(struct sns_sensor_instance *, size_t);

static sns_rc get_memory_pool_name(struct sns_sensor *, char *, size_t);

static void *sensor_malloc(struct sns_sensor *sensor, size_t size);

static void sensor_free(struct sns_sensor *sensor, void *ptr);

static void *sensor_malloc_island(struct sns_sensor *sensor, size_t size);

/*=============================================================================
  Static Definitions
  ===========================================================================*/

static sns_fw_memory_service memory_service SNS_SECTION(".data.sns");

static const sns_memory_service_api
    memory_service_api SNS_SECTION(".rodata.sns") = {
        .struct_len = sizeof(memory_service_api),
        .malloc = &instance_malloc,
        .free = &instance_free,
        .register_island_memory_vote_cb = &register_island_memory_vote_cb,
        .malloc_island = &instance_malloc_island,
        .get_memory_pool_name = &get_memory_pool_name,
        .sensor_malloc = &sensor_malloc,
        .sensor_free = &sensor_free,
        .sensor_malloc_island = &sensor_malloc_island,
};

// Memory segment to Memory pool name mapping
static const sns_mem_seg_mem_pool_map mem_seg_mem_pool_map[] = {
#ifdef QSH_ISLAND_POOL
    {SNS_MEM_SEG_QSH, QSH_ISLAND_POOL},
#endif
#ifdef SSC_ISLAND_POOL
    {SNS_MEM_SEG_SSC, SSC_ISLAND_POOL},
#endif
#ifdef QSHTECH_ISLAND_POOL
    {SNS_MEM_SEG_QSHTECH, QSHTECH_ISLAND_POOL},
#endif
    {SNS_MEM_SEG_UNKNOWN, ""},
};

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

SNS_SECTION(".text.sns")
static int find_alloc_cnt_idx(void *ptr)
{
  for(int i = 0; i < SNS_MEMORY_SERVICE_NUM_SENSORS; i++)
  {
    if(memory_service.sensor_info_lst[i].ptr == ptr)
    {
      return i;
    }
  }
  return -1;
}

SNS_SECTION(".text.sns")
static void increment_reference_count(void *ptr)
{
  sns_osa_lock_acquire(&memory_service.lock);

  int idx = find_alloc_cnt_idx(ptr);
  if(idx != -1)
  {
    memory_service.sensor_info_lst[idx].alloc_cnt++;
  }
  else
  {
    idx = find_alloc_cnt_idx(NULL); // find empty index
    if(idx != -1)
    {
      memory_service.sensor_info_lst[idx].ptr = ptr;
      memory_service.sensor_info_lst[idx].alloc_cnt = 1;
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "sns_memory_service: sensor_info_lst full, garbage collection "
                 "skipped for this allocation");
    }
  }

  sns_osa_lock_release(&memory_service.lock);
}

SNS_SECTION(".text.sns")
static void decrement_reference_count(void *ptr)
{
  sns_osa_lock_acquire(&memory_service.lock);

  int idx = find_alloc_cnt_idx(ptr);
  if(idx != -1)
  {
    memory_service.sensor_info_lst[idx].alloc_cnt--;
    if(memory_service.sensor_info_lst[idx].alloc_cnt == 0)
    {
      memory_service.sensor_info_lst[idx].ptr = NULL;
    }
  }

  sns_osa_lock_release(&memory_service.lock);
}

static void __attribute__((noinline)) sns_memory_service_cleanup_ni(int idx)
{
  void *ptr = NULL;
  uint16_t alloc_cnt = 0;
  uint16_t cleanup_cnt = 0;

  sns_osa_lock_acquire(&memory_service.lock);
  ptr = memory_service.sensor_info_lst[idx].ptr;
  alloc_cnt = memory_service.sensor_info_lst[idx].alloc_cnt;
  sns_osa_lock_release(&memory_service.lock);

  for(sns_mem_heap_id heap_id = SNS_HEAP_MAIN; heap_id < SNS_HEAP_MAX;
      heap_id++)
  {
    cleanup_cnt += sns_free_client_ptr_instances(heap_id, ptr, alloc_cnt);
    alloc_cnt -= cleanup_cnt;
  }

  sns_osa_lock_acquire(&memory_service.lock);
  memory_service.sensor_info_lst[idx].alloc_cnt -= cleanup_cnt;
  if(memory_service.sensor_info_lst[idx].alloc_cnt == 0)
  {
    memory_service.sensor_info_lst[idx].ptr = NULL;
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "sns_memory_service_cleanup: allocations remaining: %x %d",
               memory_service.sensor_info_lst[idx].ptr,
               memory_service.sensor_info_lst[idx].alloc_cnt);
    memory_service.sensor_info_lst[idx].ptr = NULL;
    memory_service.sensor_info_lst[idx].alloc_cnt = 0;
  }
  sns_osa_lock_release(&memory_service.lock);
}

SNS_SECTION(".text.sns")
void sns_memory_service_cleanup(void *ptr)
{
  sns_osa_lock_acquire(&memory_service.lock);
  int idx = find_alloc_cnt_idx(ptr);
  sns_osa_lock_release(&memory_service.lock);
  if(idx != -1)
  {
    SNS_ISLAND_EXIT();
    sns_memory_service_cleanup_ni(idx);
  }
}

static void sns_memory_service_deinit(void)
{
  sns_osa_lock_deinit(&memory_service.lock);
}

static void *instance_malloc(struct sns_sensor_instance *instance, size_t size)
{
  void *res = sns_malloc_debug(SNS_HEAP_BATCH, size, instance);

  if(res != NULL)
  {
    SNS_PRINTF(LOW, sns_fw_printf,
               "memservice allocated inst ptr: %x %x, size: %x", instance, res,
               size);
    if(instance != NULL)
    {
      increment_reference_count(instance);
    }
  }

  return res;
}

SNS_SECTION(".text.sns")
static void *instance_malloc_island(struct sns_sensor_instance *instance,
                                    size_t size)
{
  void *res;
  // allocate memory in island batch memory when instance is coming from island
  // memory
  if(sns_island_is_island_ptr((intptr_t)instance))
  {
    res = sns_malloc_debug(SNS_ISLAND_BATCH, size, instance);

    if(res != NULL)
    {
      SNS_PRINTF(LOW, sns_fw_printf,
                 "memservice allocated ptr: %x %x, size: %x", instance, res,
                 size);
      if(instance != NULL)
      {
        increment_reference_count(instance);
      }
    }
  }
  // instance is coming from big image so allocate memory in big image
  else
  {
    SNS_ISLAND_EXIT();
    res = instance_malloc(instance, size);
  }

  return res;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void instance_free(struct sns_sensor_instance *instance, void *ptr)
{
  if(sns_free(ptr) == SNS_RC_SUCCESS)
  {
    SNS_PRINTF(LOW, sns_fw_printf, "memservice freed inst ptr: %x %x", instance,
               ptr);
    if(instance != NULL)
    {
      decrement_reference_count(instance);
    }
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "memservice calling free on invalid ptr: %x %x", instance, ptr);
  }
}
/*----------------------------------------------------------------------------*/

static void register_island_memory_vote_cb(sns_sensor *const sensor,
                                           sns_island_memory_vote_cb cb)
{
  sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;

  if((true == sns_is_island_sensor_instance(fw_sensor->sensor.instance_api)) ||
     (true == sns_is_island_sensor(fw_sensor->sensor.sensor_api,
                                   fw_sensor->sensor.instance_api)))
  {
    fw_sensor->island_memory_vote_cb = cb;
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "Big image sensor(%x) tried to register cb", sensor);
  }
}
/*----------------------------------------------------------------------------*/

static sns_rc get_memory_pool_name(struct sns_sensor *sensor, char *dst_buffer,
                                   size_t dst_size)
{
  bool found_pool = false;
  size_t pool_name_size;
  sns_fw_sensor *fw_sensor;
  sns_sensor_library *library;
  sns_rc rc = SNS_RC_SUCCESS;

  if(sensor == NULL || dst_buffer == NULL)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "Got NULL pointer. sensor(%x), dst_buffer(%x)", sensor,
               dst_buffer);
    return SNS_RC_INVALID_VALUE;
  }

  fw_sensor = (sns_fw_sensor *)sensor;
  library = fw_sensor->library;

  SNS_PRINTF(HIGH, sns_fw_printf,
             "get_memory_pool_name: mem_seg(%d). sensor(%x)",
             library->mem_segment, sensor);

  for(int i = 0; i < ARR_SIZE(mem_seg_mem_pool_map); i++)
  {
    if(library->mem_segment == mem_seg_mem_pool_map[i].mem_segment)
    {
      found_pool = true;
      pool_name_size = strlen(mem_seg_mem_pool_map[i].pool_name);
      if(dst_size >= (pool_name_size + 1))
      {
        sns_strlcpy(dst_buffer, mem_seg_mem_pool_map[i].pool_name, dst_size);
      }
      else
      {
        rc = SNS_RC_INVALID_VALUE;
      }
      break;
    }
  }
  if(found_pool == false)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Unsupported mem_seg(%d), sensor(%x)",
               library->mem_segment, sensor);
    rc = SNS_RC_NOT_SUPPORTED;
  }
  return rc;
}

/*----------------------------------------------------------------------------*/

static void *sensor_malloc(struct sns_sensor *sensor, size_t size)
{
  void *res = sns_malloc_debug(SNS_HEAP_BATCH, size, sensor);

  if(res != NULL)
  {
    if(sensor != NULL)
    {
      increment_reference_count(sensor);
    }
  }
  else
  {
    SNS_PRINTF(HIGH, sns_fw_printf,
               "memservice failed to allocate memory in non-island: sensor:%x, "
               "size: %x",
               sensor, size);
  }

  return res;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void *sensor_malloc_island(struct sns_sensor *sensor, size_t size)
{
  void *res;

  // allocate memory in island memory when sensor is coming from island
  // memory
  if(sns_island_is_island_ptr((intptr_t)sensor))
  {
    sns_mem_heap_id heap_id;
    sns_fw_sensor *fw_sensor = (sns_fw_sensor *)sensor;
    sns_mem_segment mem_segment = fw_sensor->library->mem_segment;

    heap_id = sns_memmgr_get_sensor_heap_id(mem_segment);
    res = sns_malloc_debug(heap_id, size, sensor);

    if(res != NULL)
    {
      if(sensor != NULL)
      {
        increment_reference_count(sensor);
      }
    }
    else
    {
      SNS_PRINTF(
          HIGH, sns_fw_printf,
          "memservice failed to allocate memory in island: sensor:%x, size: %x",
          sensor, size);
    }
  }
  // sensor is coming from big image so allocate memory in big image
  else
  {
    SNS_ISLAND_EXIT();
    res = sensor_malloc(sensor, size);
  }

  return res;
}

/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
static void sensor_free(struct sns_sensor *sensor, void *ptr)
{
  if(sns_free(ptr) == SNS_RC_SUCCESS)
  {
    SNS_PRINTF(LOW, sns_fw_printf, "memservice freed sensor ptr: %x %x", sensor,
               ptr);
    if(sensor != NULL)
    {
      decrement_reference_count(sensor);
    }
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "memservice calling free on invalid ptr: %x %x", sensor, ptr);
  }
}

/*----------------------------------------------------------------------------*/

sns_fw_memory_service *sns_memory_service_init(void)
{
  sns_rc rc;
  sns_osa_lock_attr lock_attr;
  sns_fw_memory_service *svc = &memory_service;

  memory_service.service.api = &memory_service_api;
  memory_service.service.service.type = SNS_MEMORY_SERVICE;
  memory_service.service.service.version = SNS_MEMORY_SERVICE_VERSION;

  /** Initialize lock for memory service cleanup structure */
  rc = sns_osa_lock_attr_init(&lock_attr);
  rc |= sns_osa_lock_init(&lock_attr, &memory_service.lock);

  if(SNS_RC_SUCCESS != rc)
  {
    SNS_ULOG_INIT_SEQ_ERR("%s: rc=%u", __FUNCTION__, rc);
    sns_memory_service_deinit();
    svc = NULL;
  }
  return svc;
}

/*----------------------------------------------------------------------------*/
