/**============================================================================
  @file sns_registry_sensor.c

  @note Registry write requests are considered atomic actions.  That is to
  say, either all items from a registry request are written successfully, or
  none of them are.

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <string.h>
#include <stdio.h>

#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_assert.h"
#include "sns_attribute_util.h"
#include "sns_dl_utils.h"
#include "sns_fw_basic_services.h"
#include "sns_fw_file_service_internal.h"
#include "sns_fw_sensor.h"
#include "sns_latency.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_pb_util.h"
#include "sns_power_mgr_service.h"
#include "sns_printf_int.h"
#include "sns_pwr_sleep_mgr.h"
#include "sns_registry.pb.h"
#include "sns_registry_parser.h"
#include "sns_registry_sensor.h"
#include "sns_registry_util.h"
#include "sns_secure.h"
#include "sns_sensor_util.h"
#include "sns_service_manager.h"
#include "sns_suid_util.h"
#include "sns_thread_table.h"
#include "sns_types.h"

/*=============================================================================
  Macros
  ===========================================================================*/

/*=============================================================================
 * Static Data Definitions
 ============================================================================*/
#define REGISTRY_SUID                                                          \
  0x75, 0x22, 0x1e, 0x70, 0xb4, 0x41, 0x25, 0x5e, 0x59, 0x27, 0x7f, 0x00,      \
      0xa7, 0x54, 0x27, 0xe1

/* This latency vote is added to keep APPS active till registry parsing is done.
 * This is added to reduce sensor discovery latency.
 */
#define SNS_REGISTRY_APPS_AWAKE_LATENCY_USEC 1000

#define MAX_PENDING_GROUP_WRITES 64

/* local handle */
volatile sns_osa_thread *registry_thread = NULL;

sns_registry_sensor_state *registry_sensor_state = NULL;

#define SNS_REGISTRY_MCPS_MAX (1000)

/*Number of items to be processed at a time from the write list*/
#define DEQUEUE_ITEM_COUNT 10

#ifdef SNS_REG_ASSERT_ENABLE
#define REG_ASSERT(check, fmt, ...) SNS_VERBOSE_ASSERT(check, fmt, __VA_ARGS__)
#else
#define REG_ASSERT(check, fmt, ...)                                            \
  if(!(check))                                                                 \
  SNS_PRINTF(ERROR, sns_fw_printf, fmt, __VA_ARGS__)
#endif

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/* See sns_sensor::event_notify */
static sns_rc notify_event(sns_sensor *const this)
{
  UNUSED_VAR(this);
  // This Sensor receives no events
  return SNS_RC_SUCCESS;
}

// Enqueue the group to be saved in a list
// for the registry thread to save it asynchronously
sns_rc sns_registry_group_enqueue(char const *restrict source,
                                  char const *restrict name,
                                  char *restrict json, uint32_t length,
                                  void *sensor)
{
  sns_rc rv = SNS_RC_FAILED;
  sns_registry_sensor_state *state =
      (sns_registry_sensor_state *)((sns_sensor *)sensor)->state->state;

  if(state->write_list.cnt < MAX_PENDING_GROUP_WRITES)
  {
    size_t name_len =
        strlen(name) + 1 + ((NULL != source) ? (strlen(source) + 1) : 0);
    /* Allocate memory for cfg + name + payload together */
    sns_registry_group_write_cfg *json_cfg =
        (sns_registry_group_write_cfg *)sns_malloc(
            SNS_HEAP_MAIN,
            sizeof(sns_registry_group_write_cfg) + name_len + length);
    sns_list_item *list_item =
        (sns_list_item *)sns_malloc(SNS_HEAP_MAIN, sizeof(sns_list_item));

    if(NULL != json_cfg && NULL != list_item)
    {
      sns_list_iter iter;
      /* Set name and payload offsets */
      json_cfg->name = (char *)json_cfg + sizeof(sns_registry_group_write_cfg);
      json_cfg->json =
          (char *)json_cfg + sizeof(sns_registry_group_write_cfg) + name_len;
      if(NULL != source)
      {
        sns_strlcpy(json_cfg->name, source, name_len);
        sns_strlcat(json_cfg->name, ".", name_len);
      }
      sns_strlcat(json_cfg->name, name, name_len);
      sns_memscpy(json_cfg->json, length, json, length);
      json_cfg->length = length;

      SNS_REG_SPRINTF(HIGH, sensor, "Enqueue group %s to queue : Success",
                      name);
      sns_list_iter_init(&iter, &state->write_list, false);
      sns_list_item_init(list_item, json_cfg);
      sns_list_iter_insert(&iter, list_item, true);

      sns_osa_thread_sigs_set(state->thread, SNS_REGISTRY_WRITE_SIGNAL);
      rv = SNS_RC_SUCCESS;
    }
    else
    {
      rv = SNS_RC_NOT_AVAILABLE;
    }

    if(SNS_RC_SUCCESS != rv)
    {
      sns_free(json_cfg);
      sns_free(list_item);
    }
  }

  if(SNS_RC_SUCCESS != rv)
  {
    SNS_REG_SPRINTF(ERROR, sensor, "group_enqueue: '%s' cnt=%u rv=%u", name,
                    state->write_list.cnt, rv);
  }
  return rv;
}

static void sns_registry_group_dequeue(sns_sensor *const this,
                                       sns_registry_sensor_state *state)
{
  sns_rc rv;
  sns_list_iter iter;
  sns_list_item *next_item, *curr_item;
  sns_registry_group_write_cfg *json_cfg;
  uint16_t item_count = 0;

  do
  {
    sns_osa_lock_acquire(((sns_fw_sensor *)this)->library->library_lock);
    sns_list_iter_init(&iter, &state->write_list, true);
    json_cfg =
        (sns_registry_group_write_cfg *)sns_list_iter_get_curr_data(&iter);

    /* Make sure we have a valid write to process
     * as the previous signal may have processed all pending writes */
    if(NULL != json_cfg)
    {
      curr_item = sns_list_iter_remove(&iter);
      next_item = sns_list_iter_curr(&iter);
    }
    else
    {
      next_item = NULL;
    }
    sns_osa_lock_release(((sns_fw_sensor *)this)->library->library_lock);

    if(NULL != json_cfg)
    {
      rv = sns_registry_group_write(json_cfg->name, json_cfg->json,
                                    json_cfg->length);
      if(rv == SNS_RC_SUCCESS)
      {
        SNS_REG_SPRINTF(HIGH, this, "Processed group %s from write queue",
                        json_cfg->name);
      }
      else if(false == sns_fs_is_up())
      {
        SNS_REG_SPRINTF(HIGH, this,
                        "Link failure writing group %s to registry %i",
                        json_cfg->name, rv);
        /* Add it back to queue and exit */
        sns_osa_lock_acquire(((sns_fw_sensor *)this)->library->library_lock);
        sns_list_iter_insert(&iter, curr_item, false);
        sns_osa_lock_release(((sns_fw_sensor *)this)->library->library_lock);
        break;
      }
      else
      {
        SNS_REG_SPRINTF(ERROR, this, "Error writing group %s to registry %i",
                        json_cfg->name, rv);
      }
      sns_free(curr_item);
      sns_free(json_cfg);
      item_count += 1;
    }
  } while(NULL != next_item && DEQUEUE_ITEM_COUNT > item_count);
  if(0 != state->write_list.cnt)
  {
    sns_osa_thread_sigs_set(state->thread, SNS_REGISTRY_WRITE_SIGNAL);
  }
}

/* Function to trigger Parsing of Registry by calling
 * initialization funcs and then publishes available if successful */
static bool sns_registry_publish(sns_sensor *const this)
{
  bool published = false;
  sns_power_mgr_handle *power_mgr_client = NULL;

  /* Bumps up clock until registry init is complete;
   * avoid the boot time regression */
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_power_mgr_service *power_mgr_svc =
      (sns_power_mgr_service *)service_mgr->get_service(service_mgr,
                                                        SNS_POWER_MGR_SERVICE);
  power_mgr_svc->api->sns_register_client("REGISTRY", &power_mgr_client);
  power_mgr_svc->api->sns_update_mcps_vote(power_mgr_client,
                                           SNS_REGISTRY_MCPS_MAX);

  sns_latency_handle *latency_handle =
      sns_latency_register("sns_registry_latency_0");
  sns_latency_set(latency_handle, SNS_REGISTRY_APPS_AWAKE_LATENCY_USEC);
  sns_rc rc = sns_registry_init(this);
  if(SNS_RC_SUCCESS == rc && sns_fs_is_up())
  {
    sns_osa_lock_acquire(((sns_fw_sensor *)this)->library->library_lock);

    {
      char const name[] = "registry";
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.str.funcs.encode = pb_encode_string_cb;
      value.str.arg = &((pb_buffer_arg){.buf = name, .buf_len = sizeof(name)});
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
    }
    {
      char const type[] = "registry";
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.str.funcs.encode = pb_encode_string_cb;
      value.str.arg = &((pb_buffer_arg){.buf = type, .buf_len = sizeof(type)});
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
    }
    {
      char const vendor[] = "QTI";
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.str.funcs.encode = pb_encode_string_cb;
      value.str.arg =
          &((pb_buffer_arg){.buf = vendor, .buf_len = sizeof(vendor)});
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1,
                            false);
    }
    {
      char const proto_files[] = "sns_registry.proto";
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.str.funcs.encode = pb_encode_string_cb;
      value.str.arg = &(
          (pb_buffer_arg){.buf = proto_files, .buf_len = sizeof(proto_files)});
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API, &value, 1, false);
    }
    {
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.has_boolean = true;
      value.boolean = true;
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1,
                            false);
    }
    {
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.has_sint = true;
      value.sint = 1;
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VERSION, &value, 1,
                            false);
    }
    {
      sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
      value.has_sint = true;
      value.sint = SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT;
      sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_STREAM_TYPE, &value, 1,
                            true);
    }

    sns_osa_lock_release(((sns_fw_sensor *)this)->library->library_lock);

    SNS_REG_PRINTF(HIGH, this, "Successfully initialized registry");
    published = true;
  }
  else if(sns_fs_is_up())
  { /* If INIT did not fail due to Daemon Failure we either ASSERT or gracefully
       handle error */
    REG_ASSERT(SNS_RC_SUCCESS == rc, "Error initializing registry rc=%d", rc);
  }

  sns_latency_deregister(latency_handle);
  power_mgr_svc->api->sns_deregister_client(&power_mgr_client);

  return published;
}

/**
 * Registry thread task function.
 *
 * File access function calls block until the file service is available on
 * the HLOS.  Until this service is available, we want to ensure
 * the rest of the SSC initialization can still be completed.
 */
static void sns_reg_task(void *arg)
{
  sns_osa_thread_sigs sig_rcvd;
  bool published = false;
  sns_sensor *const this = (sns_sensor *)arg;
  const uint32_t reg_task_sig =
      (SNS_REGISTRY_WRITE_SIGNAL | SNS_REGISTRY_RPCD_UP_SIGNAL |
       SNS_REGISTRY_RPCD_DN_SIGNAL | SNS_REGISTRY_DL_LOAD_SIGNAL |
       SNS_REGISTRY_LIB_DELETE_SIGNAL | SNS_REGISTRY_PWR_RAIL_OFF_SIGNAL |
       SNS_OSA_THREAD_EXIT_SIG | SNS_REGISTRY_SECURE_FILE_WRITE_SIGNAL);
  sns_registry_sensor_state *state =
      (sns_registry_sensor_state *)this->state->state;
  struct sns_watchdog_voter *wd_voter =
      sns_osa_thread_get_wd_voter(state->thread);
  state->island_client =
      sns_island_aggregator_register_client(SNS_REGISTRY_THREAD);

  sns_osa_thread_active(wd_voter);

  while(1)
  {
    sns_island_unblock(state->island_client);
    sig_rcvd = 0;
    sns_osa_thread_sigs_wait_wrapper(reg_task_sig, &sig_rcvd, state->thread);
    sns_island_block(state->island_client);

    if(sig_rcvd & SNS_OSA_THREAD_EXIT_SIG)
    {
      SNS_REG_PRINTF(HIGH, this, "THREAD_EXIT Signal");
      break;
    }

    if(sig_rcvd & SNS_REGISTRY_RPCD_DN_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "SSCRPCD DN Signal");
    }

    if(sig_rcvd & SNS_REGISTRY_RPCD_UP_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "Received : SSCRPCD UP Signal");
      SNS_ULOG_BOOTUP_PROFILE("Avail: %u, RPCD_UP",
                              (uint32_t)sns_get_system_time());
      if(published)
      {
        sns_registry_group_dequeue(this, state);
      }
      else
      {
        sns_rc rc = SNS_RC_FAILED;
        sns_file_service_internal *file_svc = sns_fs_get_internal_service();
        if(NULL != file_svc)
        {
          rc = file_svc->api->init();
        }
        if(SNS_RC_SUCCESS == rc)
        {
          published = sns_registry_publish(this);
        }
      }
      SNS_REG_PRINTF(HIGH, this, "Processed : SSCRPCD UP Signal");
    }

    if(sig_rcvd & SNS_REGISTRY_WRITE_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "Received : WRITE Signal");
      sns_registry_group_dequeue(this, state);
      SNS_REG_PRINTF(HIGH, this, "Processed : WRITE Signal");
      sns_reg_debug_buf_dump();
    }

    if(sig_rcvd & SNS_REGISTRY_DL_LOAD_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "Received : DL_LOAD Signal");
      sns_load_shared_object(this);
      SNS_REG_PRINTF(HIGH, this, "Processed : DL_LOAD Signal");
    }

    if(sig_rcvd & SNS_REGISTRY_PWR_RAIL_OFF_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "Received : PWR_RAIL_OFF_SIGNAL Signal");
      sns_pwr_rail_service_devote();
      SNS_REG_PRINTF(HIGH, this, "Processed : PWR_RAIL_OFF_SIGNAL Signal");
    }

    if(sig_rcvd & SNS_REGISTRY_LIB_DELETE_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this, "Received : LIB_DELETE Signal");
      sns_framework_delete_empty_libraries();
      SNS_REG_PRINTF(HIGH, this, "Processed : LIB_DELETE Signal");
    }

    if(sig_rcvd & SNS_REGISTRY_SECURE_FILE_WRITE_SIGNAL)
    {
      SNS_REG_PRINTF(HIGH, this,
                     "Received : SNS_REGISTRY_SECURE_FILE_WRITE_SIGNAL Signal");
      sns_flush_secure_db();
      SNS_REG_PRINTF(HIGH, this, "Processed : SECURE FILE WRITE Signal");
    }
  }
}

static void service_status_update_cb(sns_service_status_func_arg arg,
                                     bool available)
{
  sns_sensor *this = (sns_sensor *)arg;
  sns_registry_sensor_state *state =
      (sns_registry_sensor_state *)this->state->state;

  sns_osa_thread_sigs_set(
      state->thread, available ? SNS_REGISTRY_RPCD_UP_SIGNAL
                               : SNS_REGISTRY_RPCD_DN_SIGNAL);
}

/* See sns_sensor::init */
static sns_rc init(sns_sensor *const this)
{
  sns_rc rc;
  sns_registry_sensor_state *state =
      (sns_registry_sensor_state *)this->state->state;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  if(NULL == file_svc)
  {
    return SNS_RC_POLICY;
  }

  sns_list_init(&state->write_list);

  rc = sns_osa_thread_create_v2(SNS_REGISTRY_THREAD, &sns_reg_task, this,
                                &state->thread);
  if(SNS_RC_SUCCESS == rc && NULL != state->thread)
  {
    registry_thread = state->thread;
    registry_sensor_state = state;
    file_svc->api->register_status_callback(&service_status_update_cb,
                                            (void *)this);
  }
  else
  {
    SNS_REG_PRINTF(FATAL, this, "Error creating registry thread");
    rc = SNS_RC_POLICY;
  }

  return rc;
}

/* See sns_sensor::deinit */
static sns_rc deinit(sns_sensor *const this)
{
  sns_registry_sensor_state *state =
      (sns_registry_sensor_state *)this->state->state;
  sns_island_aggregator_deregister_client(state->island_client);
  sns_osa_thread_sigs_set(state->thread, SNS_OSA_THREAD_EXIT_SIG);
  sns_osa_thread_delete(state->thread);
  registry_thread = state->thread = NULL;

  return SNS_RC_SUCCESS;
}

/* See sns_sensor::get_sensor_uid */
static sns_sensor_uid const *get_sensor_uid(sns_sensor const *this)
{
  UNUSED_VAR(this);
  static const sns_sensor_uid registry_suid = {.sensor_uid = {REGISTRY_SUID}};
  return &registry_suid;
}

/**
 * Parse a read request message to get the group name.
 *
 * @param[i] req Incoming request message
 *
 * @return Parsed name (pointer into the request message)
 */
static char const *get_read_req_name(pb_buffer_arg const *payload)
{
  sns_registry_read_req read_req = sns_registry_read_req_init_default;
  pb_buffer_arg data;
  pb_istream_t stream = pb_istream_from_buffer(payload->buf, payload->buf_len);

  read_req.name.funcs.decode = &pb_decode_string_cb;
  read_req.name.arg = &data;

  if(pb_decode_noinit(&stream, sns_registry_read_req_fields, &read_req))
    return (char *)data.buf;

  SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding read req: %s",
                  PB_GET_ERROR(&stream));
  return NULL;
}

/**
 * Encode all registry items for the given registry group.
 */
static bool encode_registry_items_cb(struct pb_ostream_s *stream,
                                     pb_field_t const *field, void *const *arg)
{
  sns_registry_group *group = (sns_registry_group *)*arg;
  sns_list_iter iter;

  for(sns_list_iter_init(&iter, &group->items, true);
      NULL != sns_list_iter_curr(&iter); sns_list_iter_advance(&iter))
  {
    sns_registry_item *item =
        (sns_registry_item *)sns_list_iter_get_curr_data(&iter);
    pb_buffer_arg str_data;
    pb_buffer_arg name_data =
        (pb_buffer_arg){.buf = item->name, .buf_len = strlen(item->name) + 1};
    sns_registry_data_item pb_item = sns_registry_data_item_init_default;

    pb_item.name.funcs.encode = &pb_encode_string_cb;
    pb_item.name.arg = &name_data;

    pb_item.has_version = true;
    pb_item.version = item->version;

    if(SNS_REGISTRY_TYPE_GROUP == item->type)
    {
      pb_item.has_subgroup = true;
      pb_item.subgroup.items.funcs.encode = &encode_registry_items_cb;
      pb_item.subgroup.items.arg = (void *)item->data.group;
    }
    else if(SNS_REGISTRY_TYPE_INTEGER == item->type)
    {
      pb_item.has_sint = true;
      pb_item.sint = item->data.sint;
    }
    else if(SNS_REGISTRY_TYPE_FLOAT == item->type)
    {
      pb_item.has_flt = true;
      pb_item.flt = item->data.flt;
    }
    else if(SNS_REGISTRY_TYPE_STRING == item->type)
    {
      str_data = (pb_buffer_arg){
          .buf = item->data.str,
          .buf_len = NULL != item->data.str ? (strlen(item->data.str) + 1) : 0};
      pb_item.str.funcs.encode = &pb_encode_string_cb;
      pb_item.str.arg = &str_data;
    }

    if(!pb_encode_tag_for_field(stream, field))
      return false;

    if(!pb_encode_submessage(stream, sns_registry_data_item_fields, &pb_item))
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error encoding item: %s",
                      PB_GET_ERROR(stream));
      return false;
    }
  }

  return true;
}

/**
 * Search for the requested group/item, create the appropriate registry read
 * event, and send back to client.
 *
 * Search criteria will be in form: "group_name[.sub_group_name][.item_name]"
 * There may be zero or more levels of subgroups specified, and including an
 * item name is optional.  If item name is included, event message will contain
 * all items within the containing group.
 *
 * @param[i] instance Client-specific instance who made the request
 * @param[i] name Full registry group/item name to read
 */
static void handle_read_req(sns_sensor_instance *instance, char const *name)
{
  pb_buffer_arg name_data =
      (pb_buffer_arg){.buf = name, .buf_len = strlen(name) + 1};
  sns_registry_read_event read_event = sns_registry_read_event_init_default;
  sns_registry_group const *group = sns_registry_group_find(name);
  char group_name[SNS_REGISTRY_STR_MAX + 1];
  char *item_name;

  if(NULL == group)
  {
    sns_strlcpy(group_name, name, sizeof(group_name));
    item_name = strrchr(group_name, '.');
    if(NULL != item_name)
      item_name[0] = '\0';

    group = sns_registry_group_find(group_name);
    if(NULL == group || NULL == item_name ||
       NULL == sns_registry_item_find(group, &item_name[1]))
      group = NULL;
    else
      name_data =
          (pb_buffer_arg){.buf = group_name, .buf_len = strlen(group_name) + 1};
  }

  read_event.name.funcs.encode = &pb_encode_string_cb;
  read_event.name.arg = &name_data;

  if(NULL == group)
  {
    SNS_REG_SPRINTF(HIGH, sns_fw_printf, "Group not found: %s", name);
  }
  else
  {
    read_event.data.items.funcs.encode = &encode_registry_items_cb;
    read_event.data.items.arg = (void *)group;
  }

  if(!pb_send_event(instance, sns_registry_read_event_fields, &read_event,
                    sns_get_system_time(),
                    SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_EVENT, NULL))
  {
    sns_std_error_event error_event;
    error_event.error = SNS_STD_ERROR_FAILED;

    pb_send_event(instance, sns_std_error_event_fields, &error_event,
                  sns_get_system_time(), SNS_STD_MSGID_SNS_STD_ERROR_EVENT,
                  NULL);
    SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Unable to send registry read event");
  }
}

/**
 * Parse a write request message to get the group name.
 *
 * @param[i] req Incoming request message
 *
 * @return Parsed name (pointer into the request message)
 */
static char const *get_write_req_name(pb_buffer_arg const *payload)
{
  sns_registry_write_req write_req = sns_registry_write_req_init_default;
  pb_buffer_arg data;
  pb_istream_t stream = pb_istream_from_buffer(payload->buf, payload->buf_len);

  write_req.name.funcs.decode = &pb_decode_string_cb;
  write_req.name.arg = &data;

  if(pb_decode_noinit(&stream, sns_registry_write_req_fields, &write_req))
    return (char *)data.buf;

  SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding write req: %s",
                  PB_GET_ERROR(&stream));
  return NULL;
}

/**
 * Decode callback for a single registry item from sns_registry_data.
 */
static bool registry_item_decode_cb(pb_istream_t *stream,
                                    const pb_field_t *field, void **arg)
{
  UNUSED_VAR(field);
  bool rv = false;
  pb_istream_t stream_cpy = *stream;
  pb_buffer_arg str_data = {0, 0}, name_data = {0, 0};
  sns_registry_group *group = (sns_registry_group *)*arg;
  sns_registry_data_item pb_item = sns_registry_data_item_init_default;

  pb_item.name.funcs.decode = &pb_decode_string_cb;
  pb_item.name.arg = &name_data;
  pb_item.str.funcs.decode = &pb_decode_string_cb;
  pb_item.str.arg = &str_data;

  if(!pb_decode_noinit(stream, sns_registry_data_item_fields, &pb_item))
    SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding item: %s",
                    PB_GET_ERROR(stream));
  else if(0 == name_data.buf_len)
    SNS_REG_PRINTF(ERROR, sns_fw_printf, "Missing item name");
  else
  {
    sns_registry_item item;
    bool failed = false;

    item.name = (char *)name_data.buf;
    item.version = pb_item.has_version ? pb_item.version : 0;

    if(pb_item.has_flt)
    {
      item.data.flt = pb_item.flt;
      item.type = SNS_REGISTRY_TYPE_FLOAT;
    }
    else if(pb_item.has_sint)
    {
      item.data.sint = pb_item.sint;
      item.type = SNS_REGISTRY_TYPE_INTEGER;
    }
    else if(0 != str_data.buf_len)
    {
      item.data.str = (char *)str_data.buf;
      item.type = SNS_REGISTRY_TYPE_STRING;
    }
    else if(0 != pb_item.has_subgroup)
    {
      sns_registry_group *subgroup;

      STR_CONCAT(full_name, total_size, group->name, ".", item.name);

      subgroup = sns_registry_group_create(full_name);
      if(NULL == subgroup)
      {
        SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error creating subgroup: %s",
                        full_name);
        failed = true;
      }
      else
      {
        sns_registry_data_item temp_item = sns_registry_data_item_init_default;
        temp_item.subgroup.items.funcs.decode = &registry_item_decode_cb;
        temp_item.subgroup.items.arg = subgroup;

        if(!pb_decode_noinit(&stream_cpy, sns_registry_data_item_fields,
                             &temp_item))
        {
          SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding item: %s",
                          PB_GET_ERROR(stream));
          failed = true;
          sns_registry_group_free(subgroup);
          subgroup = NULL;
        }

        item.data.group = subgroup;
        item.type = SNS_REGISTRY_TYPE_GROUP;
      }
    }
    else
    {
      SNS_REG_PRINTF(ERROR, sns_fw_printf, "Unknown item type");
      failed = true;
    }

    if(!failed)
    {
      sns_registry_item const *new_item =
          sns_registry_item_create(group, &item);
      if(NULL == new_item)
        SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error creating new item: %s",
                        item.name);
      else
        rv = true;
    }
  }

  return rv;
}

/**
 * Handle a registry write request
 *
 * @param[i] Sensor handle for enqueue of writes
 * @param[i] Client-specific instance who made the request
 * @param[i] Parsed null-terminated registry group name
 * @param[i] Incoming request message
 *
 * @return
 *  SNS_RC_SUCCESS
 *  SNS_RC_INVALID_VALUE Unable to create registry group
 *  SNS_RC_NOT_AVAILABLE File system error
 */
static sns_rc handle_write_req(sns_sensor *const sensor,
                               sns_sensor_instance *instance, char const *name,
                               pb_buffer_arg const *payload)
{
  sns_rc rv = SNS_RC_SUCCESS;
  sns_registry_group *group = sns_registry_group_create(name);
  sns_registry_write_event write_event = sns_registry_write_event_init_default;

  if(NULL == group)
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf, "Failure creating registry group");
    rv = SNS_RC_INVALID_VALUE;
  }
  else
  {
    sns_registry_write_req write_req = sns_registry_write_req_init_default;
    pb_istream_t stream =
        pb_istream_from_buffer(payload->buf, payload->buf_len);

    write_req.data.items.funcs.decode = &registry_item_decode_cb;
    write_req.data.items.arg = group;

    if(!pb_decode_noinit(&stream, sns_registry_write_req_fields, &write_req))
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding write req: %s",
                      PB_GET_ERROR(&stream));
      rv = SNS_RC_FAILED;
    }
    else
    {
      rv = sns_registry_group_merge(group, sensor);
      if(write_req.has_cookie)
      {
        write_event.has_cookie = true;
        write_event.cookie = write_req.cookie;
      }
    }
    sns_registry_group_free(group);
  }

  write_event.status = SNS_RC_SUCCESS == rv
                           ? SNS_REGISTRY_WRITE_STATUS_SUCCESS
                           : SNS_REGISTRY_WRITE_STATUS_ERROR_OTHER;

  pb_send_event(instance, sns_registry_write_event_fields, &write_event,
                sns_get_system_time(),
                SNS_REGISTRY_MSGID_SNS_REGISTRY_WRITE_EVENT, NULL);
  return rv;
}

/*
 * See sns_sensor::set_client_request
 *
 * This Sensor combines all requests from the same client to a single Instance,
 * regardless of the request.  Instances are never shared amongst clients.
 */
static sns_sensor_instance *set_client_request(sns_sensor *const this,
                                               sns_request const *curr_req,
                                               sns_request const *new_req,
                                               bool remove)
{
  UNUSED_VAR(curr_req);
  sns_sensor_instance *inst = this->cb->get_sensor_instance(this, true);

  if(NULL == inst)
    inst = this->cb->create_instance(this, 0);

  if(!remove)
  {
    pb_istream_t stream =
        pb_istream_from_buffer(new_req->request, new_req->request_len);
    pb_buffer_arg payload = {0, 0};
    sns_std_request request = sns_std_request_init_default;

    request.payload.funcs.decode = &pb_decode_string_cb;
    request.payload.arg = &payload;

    if(pb_decode_noinit(&stream, sns_std_request_fields, &request))
    {
      inst->cb->add_client_request(inst, new_req);
      if(SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_REQ == new_req->message_id)
      {
        char const *name = get_read_req_name(&payload);
        if(NULL == name)
          SNS_REG_PRINTF(ERROR, sns_fw_printf,
                         "Error parsing registry read request");
        else
          handle_read_req(inst, name);
      }
      else if(SNS_REGISTRY_MSGID_SNS_REGISTRY_WRITE_REQ == new_req->message_id)
      {
        char const *name = get_write_req_name(&payload);
        if(NULL == name)
          SNS_REG_PRINTF(ERROR, sns_fw_printf,
                         "Error parsing registry write request");
        else
          handle_write_req(this, inst, name, &payload);
      }
    }
    else
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error decoding std req: %s",
                      PB_GET_ERROR(&stream));

    inst->cb->remove_client_request(inst, new_req);
  }

  if(NULL != inst &&
     NULL == inst->cb->get_client_request(inst, get_sensor_uid(this), true))
  {
    this->cb->remove_instance(inst);
  }

  return &sns_instance_no_error;
}

/*===========================================================================
  Public Data Definitions
  ===========================================================================*/
/**
 * Sends Registry task the given signal
 */
void sns_sensor_signal_registry_task(unsigned int sig)
{
#ifdef SNS_DISABLE_REGISTRY
  UNUSED_VAR(sig);
#else

  if(NULL != registry_thread)
  {
    sns_osa_thread_sigs_set((sns_osa_thread*)registry_thread, sig);
  }
  else
  {
    SNS_FW_DBG_LOG("signal_registry_task: can't signal Registry task");
  }

#endif
}

const sns_sensor_api sns_registry_sensor_api = {
    .struct_len = sizeof(sns_sensor_api),
    .init = &init,
    .deinit = &deinit,
    .get_sensor_uid = &get_sensor_uid,
    .set_client_request = &set_client_request,
    .notify_event = &notify_event,
};
