/**============================================================================
  @file sns_suid_sensor.c

  The Framework Sensor manages all SUID and attribute requests.  It is
  technically part of the Framework, but it modelled separately in order
  to avoid special cases within the event-passing code.

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include <string.h>
#include "pb_decode.h"
#include "pb_encode.h"
#include "sns_assert.h"
#include "sns_attribute_service.h"
#include "sns_attribute_util.h"
#include "sns_event_service.h"
#include "sns_fw_attribute_service.h"
#include "sns_fw_sensor.h"
#include "sns_fw_sensor_instance.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_pb_util.h"
#include "sns_printf_int.h"
#include "sns_registry.pb.h"
#include "sns_router.h"
#include "sns_sensor.h"
#include "sns_sensor_event.h"
#include "sns_sensor_util.h"
#include "sns_service.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_suid.pb.h"
#include "sns_suid_sensor.h"
#include "sns_suid_util.h"
#include "sns_timer.h"
#include "sns_types.h"

/*=============================================================================
  Macro Definitions
  ===========================================================================*/

#define MAX_DATATYPE_LEN 32
// Timeout of 120s used to cleanup unsupported requests
#define SUID_TIMER_TIMEOUT_NSEC 120000000000ULL // nanoseconds

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * Callback structure used when encoding the list of SUIDs in response
 * to an SUID lookup request.
 */
typedef struct sns_encode_suid_arg
{
  struct pb_ostream_s *stream;
  pb_field_t const *field;
  /* If an empty string, encode all sensors */
  char const *data_type;
  uint32_t *data_type_len;
  /* Client request only default SUID to be encoded */
  bool default_only;
  /* request is only for SUID in local HUB */
  bool native_only;
  /* If the default sensor has been found and encoded */
  bool default_encoded;
  /* No default sensor found; instead use "first" matching SUID */
  bool use_matching_against_default;
} sns_encode_suid_arg;

/**
 * Information regarding a single sensor attribute in sns_def_sensor_info.
 */
typedef struct sns_def_sensor_attr
{
  /* entry for the attr_list */
  sns_isafe_list_item list_entry;
  /* attribute id to match */
  sns_attribute_id attr_id;
  /* size of attribute value */
  size_t attr_val_size;
  /* attribute value */
  void *attr_val;
} sns_def_sensor_attr;

/**
 * Information regarding a default sensor of a given datatype; this is
 * populated from registry config.
 */
typedef struct sns_def_sensor_info
{
  /* entry for the def_sensor_list */
  sns_isafe_list_item list_entry;
  /* attribute list */
  sns_isafe_list attr_list;
  /* sensor datatype */
  char datatype[MAX_DATATYPE_LEN];
  /* match flag */
  bool match_found;
  /* registry response has group info */
  bool has_group;
  /* registry response has item info */
  bool has_subgroup;
} sns_def_sensor_info;

/*=============================================================================
  Static Data Definitions
  ===========================================================================*/
static sns_fw_sensor *suid_sensor = NULL;

static const sns_sensor_uid registry_suid = {
    .sensor_uid = {0x75, 0x22, 0x1e, 0x70, 0xb4, 0x41, 0x25, 0x5e, 0x59, 0x27,
                   0x7f, 0x00, 0xa7, 0x54, 0x27, 0xe1}};

static char const *reg_default_sensors = "default_sensors";

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/* Decodes a single attribute value */
static bool decode_attr_value(pb_istream_t *stream, const pb_field_t *field,
                              void **arg)
{
  UNUSED_VAR(field);
  pb_buffer_arg str_data;
  const void *data_ptr = NULL;
  size_t data_len = 0;
  int rc = false;

  sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
  value.str.funcs.decode = &pb_decode_string_cb;
  value.str.arg = &str_data;

  if(!pb_decode_noinit(stream, sns_std_attr_value_data_fields, &value))
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "decode_attr_value, decode failed");
  }
  else
  {
    if(value.has_sint)
    {
      data_ptr = &value.sint;
      data_len = sizeof(value.sint);
      rc = true;
    }
    else if(value.has_flt)
    {
      data_ptr = &value.flt;
      data_len = sizeof(value.flt);
      rc = true;
    }
    else if(str_data.buf_len > 0)
    {
      data_ptr = str_data.buf;
      data_len = str_data.buf_len;
      rc = true;
    }
  }

  /* sns_free for this allocation will be called in
   * default_sensor_match_attr_cb() */
  *arg = sns_malloc(SNS_HEAP_MAIN, data_len);
  SNS_ASSERT(data_len == 0 || *arg != NULL);
  sns_memscpy(*arg, data_len, data_ptr, data_len);

  return rc;
}

/* Callback function used with sns_attr_info_process_attribute */
static bool default_sensor_process_attribute_cb(void *buf, uint32_t buf_len,
                                                void *arg)
{
  bool rc = true;
  void **attr_val = arg;
  pb_istream_t stream = pb_istream_from_buffer(buf, buf_len);

  sns_std_attr attribute = sns_std_attr_init_default;
  attribute.value.values.funcs.decode = decode_attr_value;

  rc = pb_decode_noinit(&stream, sns_std_attr_fields, &attribute);
  if(!rc)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "default_sensor_process_attribute_cb, pb_decode() failed");
  }
  else
  {
    *attr_val = attribute.value.values.arg;
  }

  return rc;
}

static bool default_sensor_match_attr_cb(struct sns_attribute_info *attr_info,
                                         void *arg)
{
  SNS_ASSERT(arg != NULL);
  sns_def_sensor_info *item = (sns_def_sensor_info *)arg;
  bool rc = true;

  if(!item->match_found && sns_attr_info_get_available(attr_info))
  {
    char datatype[MAX_DATATYPE_LEN];
    sns_attr_info_get_data_type(attr_info, datatype, sizeof(datatype));

    if(0 == strcmp(datatype, item->datatype))
    {
      bool attr_mismatch = false;
      sns_isafe_list_iter iter;
      for(sns_isafe_list_iter_init(&iter, &item->attr_list, true);
          NULL != sns_isafe_list_iter_curr(&iter);
          sns_isafe_list_iter_advance(&iter))
      {
        void *attr_val = NULL;
        sns_def_sensor_attr *attr =
            (sns_def_sensor_attr *)sns_isafe_list_iter_get_curr_data(&iter);
        rc = sns_attr_info_process_attribute(
            attr_info, attr->attr_id, &default_sensor_process_attribute_cb,
            &attr_val);
        SNS_ASSERT(NULL != attr->attr_val);
        if(!rc || NULL == attr_val)
        {
          SNS_PRINTF(ERROR, sns_fw_printf,
                     "sns_attr_info_process_attribute() failed");
        }
        else
        {
          if(sns_memcmp(attr_val, attr->attr_val, attr->attr_val_size))
          {
            SNS_SPRINTF(MED, sns_fw_printf,
                        "default_sensor_match_attr_cb: attribute id %d "
                        "mismatch found %s",
                        attr->attr_id, item->datatype);
            attr_mismatch |= true;
          }
        }
        /* free memory allocated in decode_attr_value */
        sns_free(attr_val);
      }
      if(!attr_mismatch)
      {
        SNS_SPRINTF(MED, sns_fw_printf,
                    "default_sensor_match_attr_cb: match found %s",
                    item->datatype);
        item->match_found = true;
        sns_attr_info_set_default(attr_info, true);
      }
    }
  }
  return true;
}

/* Updates the def_sensor_list with default attribute info */
static void default_sensor_match_attributes(char const *data_type)
{
  sns_suid_sensor_state *state =
      (sns_suid_sensor_state *)suid_sensor->sensor.state->state;
  sns_isafe_list_iter iter;
  bool rc;

  for(sns_isafe_list_iter_init(&iter, &state->def_sensor_list, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_def_sensor_info *item =
        (sns_def_sensor_info *)sns_isafe_list_iter_get_curr_data(&iter);

    if(!item->match_found && 0 == strcmp(item->datatype, data_type))
    {
      rc = sns_attr_svc_sensor_foreach(&default_sensor_match_attr_cb,
                                       (void *)item);
      if(!rc)
        SNS_PRINTF(ERROR, sns_fw_printf, "error while matching attributes");
    }
  }
}

/* callback function to decode one item from the default_sensor config */
static bool default_sensor_item_decode_cb(pb_istream_t *stream,
                                          const pb_field_t *field, void **arg)
{
  UNUSED_VAR(field);
  sns_def_sensor_info *sensor_info = (sns_def_sensor_info *)*arg;
  sns_registry_data_item reg_item = sns_registry_data_item_init_default;
  pb_buffer_arg item_name;
  pb_buffer_arg item_str_data;
  sns_isafe_list_iter iter;
  bool rc, rv = true;

  reg_item.name.funcs.decode = &pb_decode_string_cb;
  reg_item.name.arg = &item_name;
  reg_item.str.funcs.decode = &pb_decode_string_cb;
  reg_item.str.arg = &item_str_data;

  sns_isafe_list_iter_init(&iter, &sensor_info->attr_list, true);
  sns_def_sensor_attr *sensor_attr = sns_isafe_list_iter_get_curr_data(&iter);

  rc = pb_decode_noinit(stream, sns_registry_data_item_fields, &reg_item);
  if(!rc || 0 == item_name.buf_len)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "pb_decode() failed");
    rv = false;
  }
  else if(NULL == sensor_attr)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "empty sensor attributes in default_sensor_item_decode_cb()");
    rv = false;
  }
  else if(0 == strcmp((char *)item_name.buf, "id"))
  {
    if(!reg_item.has_sint)
      rv = false;
    else
      sensor_attr->attr_id = reg_item.sint;
  }
  else if(0 == strcmp((char *)item_name.buf, "val"))
  {
    const void *data_ptr;

    if(reg_item.has_sint)
    {
      data_ptr = &reg_item.sint;
      sensor_attr->attr_val_size = sizeof(reg_item.sint);
    }
    else if(reg_item.has_flt)
    {
      data_ptr = &reg_item.flt;
      sensor_attr->attr_val_size = sizeof(reg_item.flt);
    }
    else if(item_str_data.buf_len > 0)
    {
      data_ptr = item_str_data.buf;
      sensor_attr->attr_val_size = item_str_data.buf_len;
    }
    else
    {
      data_ptr = NULL;
      sensor_attr->attr_val_size = 0;
      rv = false;
    }

    sensor_attr->attr_val =
        sns_malloc(SNS_HEAP_MAIN, sensor_attr->attr_val_size);
    SNS_ASSERT(sensor_attr->attr_val_size == 0 ||
               sensor_attr->attr_val != NULL);
    sns_memscpy(sensor_attr->attr_val, sensor_attr->attr_val_size, data_ptr,
                sensor_attr->attr_val_size);
  }

  if(rv)
    sensor_info->has_subgroup = true;

  return rv;
}

static bool default_sensor_attr_group_decode_cb(pb_istream_t *stream,
                                                const pb_field_t *field,
                                                void **arg)
{
  UNUSED_VAR(field);
  sns_def_sensor_info *sensor_info = (sns_def_sensor_info *)*arg;
  sns_registry_data_item reg_item = sns_registry_data_item_init_default;
  sns_def_sensor_attr *sensor_attr = NULL;
  sns_isafe_list_iter iter;
  pb_buffer_arg group_name;
  bool rc, rv = true;

  sensor_attr = sns_malloc(SNS_HEAP_MAIN, sizeof(*sensor_attr));
  SNS_ASSERT(sensor_attr != NULL);

  sns_isafe_list_iter_init(&iter, &sensor_info->attr_list, true);
  sns_isafe_list_item_init(&sensor_attr->list_entry, sensor_attr);
  sns_isafe_list_iter_insert(&iter, &sensor_attr->list_entry, false);

  reg_item.name.funcs.decode = &pb_decode_string_cb;
  reg_item.name.arg = &group_name;
  reg_item.subgroup.items.funcs.decode = default_sensor_item_decode_cb;
  reg_item.subgroup.items.arg = sensor_info;

  rc = pb_decode_noinit(stream, sns_registry_data_item_fields, &reg_item);
  if(!rc || 0 == group_name.buf_len)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "pb_decode() failed, buf_len=%i",
               group_name.buf_len);
    rv = false;
  }
  else if(!sensor_info->has_subgroup)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "subgroup not found, decode failed");
    rv = false;
  }

  if(!rv)
  {
    sns_isafe_list_iter_return(&iter);
    sns_isafe_list_iter_remove(&iter);
    sns_free(sensor_attr);
  }
  else
    sensor_info->has_group = true;

  return rv;
}

/**
 * Decode a registry item; expected contents are a subgroup containing the
 * attributes which the default sensor must hold.
 */
static bool default_sensor_group_decode_cb(pb_istream_t *stream,
                                           const pb_field_t *field, void **arg)
{
  UNUSED_VAR(field);
  UNUSED_VAR(arg);
  sns_suid_sensor_state *state =
      (sns_suid_sensor_state *)suid_sensor->sensor.state->state;
  sns_registry_data_item reg_item = sns_registry_data_item_init_default;
  pb_buffer_arg group_name;
  sns_def_sensor_info *sensor_info;
  bool rc, rv = true;

  sensor_info = sns_malloc(SNS_HEAP_MAIN, sizeof(*sensor_info));
  SNS_ASSERT(NULL != sensor_info);

  reg_item.name.funcs.decode = &pb_decode_string_cb;
  reg_item.name.arg = &group_name;
  reg_item.subgroup.items.funcs.decode = default_sensor_attr_group_decode_cb;
  reg_item.subgroup.items.arg = sensor_info;

  sns_isafe_list_init(&sensor_info->attr_list);
  rc = pb_decode_noinit(stream, sns_registry_data_item_fields, &reg_item);
  if(!rc || 0 == group_name.buf_len || !sensor_info->has_group)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "pb_decode() failed, buf_len=%i has_group=%i",
               group_name.buf_len, sensor_info->has_group);
    sns_free(sensor_info);
    rv = false;
  }
  else
  {
    bool info_exists = false;
    sns_isafe_list_iter iter;

    sns_strlcpy(sensor_info->datatype, group_name.buf, MAX_DATATYPE_LEN - 1);
    sensor_info->datatype[MAX_DATATYPE_LEN - 1] = '\0';

    SNS_SPRINTF(MED, sns_fw_printf, "default_sensor_group_decode_cb, dt=%s",
                sensor_info->datatype);

    /* check if the datatype exists in the def_sensor_list */
    for(sns_isafe_list_iter_init(&iter, &state->def_sensor_list, true);
        NULL != sns_isafe_list_iter_curr(&iter);
        sns_isafe_list_iter_advance(&iter))
    {
      sns_def_sensor_info *temp =
          (sns_def_sensor_info *)sns_isafe_list_iter_get_curr_data(&iter);

      if(0 == strcmp(temp->datatype, sensor_info->datatype))
      {
        info_exists = true;
        break;
      }
    }

    if(!info_exists)
    {
      sns_isafe_list_iter_init(&iter, &state->def_sensor_list, true);
      sns_isafe_list_item_init(&sensor_info->list_entry, sensor_info);
      sns_isafe_list_iter_insert(&iter, &sensor_info->list_entry, true);
      /* match this default sensor config to a sensor attribute_info */
      default_sensor_match_attributes(sensor_info->datatype);
    }
    else
    {
      sns_free(sensor_info);
    }
  }
  return rv;
}

/**
 * Decode all pending events from the Registry Sensor.
 */
static void suid_process_registry_events(sns_sensor *this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  sns_data_stream *reg_stream = state->reg_stream;

  for(sns_sensor_event *event = reg_stream->api->peek_input(reg_stream);
      NULL != event; event = reg_stream->api->get_next_input(reg_stream))
  {
    if(SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_EVENT == event->message_id)
    {
      pb_istream_t stream =
          pb_istream_from_buffer((void *)event->event, event->event_len);
      sns_registry_read_event read_event = sns_registry_read_event_init_default;
      pb_buffer_arg group_name;

      read_event.name.arg = &group_name;
      read_event.name.funcs.decode = pb_decode_string_cb;

      read_event.data.items.funcs.decode = &default_sensor_group_decode_cb;
      read_event.data.items.arg = NULL;

      if(!pb_decode_noinit(&stream, sns_registry_read_event_fields,
                           &read_event))
        SNS_PRINTF(ERROR, this, "Error decoding registry event");
    }
  }
}

/* See sns_sensor::event_notify */
static sns_rc notify_event(sns_sensor *const this)
{
  UNUSED_VAR(this);
  suid_process_registry_events(this);
  return SNS_RC_SUCCESS;
}

/**
 * Generate and send an error indication
 */
static sns_rc suid_send_error_event(sns_sensor_instance *const instance)
{
  sns_rc rc = SNS_RC_SUCCESS;

  sns_std_error_event error_event;
  error_event.error = SNS_STD_ERROR_NOT_AVAILABLE;

  if(!pb_send_event(instance, sns_std_error_event_fields, &error_event,
                    sns_get_system_time(), SNS_STD_MSGID_SNS_STD_ERROR_EVENT,
                    NULL))
  {
    rc = SNS_RC_FAILED;
  }

  return rc;
}

static void suid_timer_cb(void *args)
{
  sns_suid_sensor_state *state =
      (sns_suid_sensor_state *)suid_sensor->sensor.state->state;

  UNUSED_VAR(args);
  SNS_PRINTF(HIGH, &suid_sensor->sensor,
             "Suid timer fired, set signal to "
             "cleanup instances");
  if(NULL != state->timer_sig_handle)
  {
    sns_signal_set(state->timer_sig_handle);
  }
}

static void stop_timer(sns_sensor *const this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  if(state->timer_active)
  {
    sns_timer_stop(state->timer, NULL);
    state->timer_active = false;
  }
}

static void disable_timer(sns_sensor *const this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  stop_timer(this);
  state->timer_disable = true;
}

static void start_timer(sns_sensor *const this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;

  stop_timer(this);

  if(!state->timer_disable)
  {
#ifndef SNS_SUID_TIMER_DISABLE
    sns_timer_start_relative(state->timer,
                             sns_convert_ns_to_ticks(SUID_TIMER_TIMEOUT_NSEC));
#endif
    state->timer_active = true;
    SNS_PRINTF(LOW, this, "Suid timer started");
  }
}

static void create_timer(sns_sensor *const this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;

  sns_timer_attr timer_attr;
  sns_timer_attr_init(&timer_attr);
  sns_timer_attr_set_memory_partition(&timer_attr, SNS_OSA_MEM_TYPE_NORMAL);
  sns_timer_attr_set_periodic(&timer_attr, false);
  sns_timer_attr_set_deferrable(&timer_attr, false);

  sns_timer_create(suid_timer_cb, NULL, &timer_attr, &state->timer);
}

static void delete_timer(sns_sensor *const this)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  stop_timer(this);
  sns_timer_delete(state->timer);
}

void sns_suid_handle_timer_signal(sns_sensor *sensor,
                                  sns_signal_handle *sig_handle, void *args)
{
  UNUSED_VAR(args);
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)sensor->state->state;

  if(sig_handle == state->timer_sig_handle)
  {
    SNS_PRINTF(
        HIGH, sensor,
        "Handling suid timer sig, cleanup instances and physical sensors");

    sns_sensor_instance *instance = NULL;

    for(instance = sensor->cb->get_sensor_instance(sensor, true);
        NULL != instance;
        instance = sensor->cb->get_sensor_instance(sensor, false))
    {
      bool request_removed = false;
      sns_isafe_list_iter iter;
      sns_suid_sensor_instance_config *state =
          (sns_suid_sensor_instance_config *)instance->state->state;

      sns_isafe_list_iter_init(&iter, &state->requests, true);

      while(NULL != sns_isafe_list_iter_curr(&iter))
      {
        sns_suid_lookup_request *lookup_req =
            (sns_suid_lookup_request *)sns_isafe_list_iter_get_curr_data(&iter);

        if(false == lookup_req->register_updates)
        {
          SNS_SPRINTF(LOW, sensor,
                      "Removed request for data_type \'%s\' "
                      "(register %i), default_only=%i",
                      lookup_req->data_type, lookup_req->register_updates,
                      lookup_req->default_only);

          request_removed = true;
          sns_isafe_list_iter_remove(&iter);
          sns_free(lookup_req);
        }
        else
        {
          sns_isafe_list_iter_advance(&iter);
        }
      }

      if(request_removed)
      {
        suid_send_error_event(instance);
      }

      if(0 == sns_isafe_list_iter_len(&iter))
      {
        sensor->cb->remove_instance(instance);
      }
    }
    // Delete unavailable physical sensors
    sns_framework_delete_unavailable_phy_sensors();
  }
}

static sns_rc register_timer_sig(sns_sensor *const sensor)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)sensor->state->state;
  sns_signal_attr signal_attr;

  sns_signal_attr_init(&signal_attr);

  sns_signal_attr_set(&signal_attr, sensor, &sns_suid_handle_timer_signal,
                      NULL);

  rc = sns_signal_register(&signal_attr, &state->timer_sig_handle);

  return rc;
}

static sns_rc unregister_timer_sig(sns_sensor *const sensor)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)sensor->state->state;

  rc = sns_signal_unregister(&state->timer_sig_handle);

  return rc;
}

static sns_rc init_sns_router(void)
{
  sns_rc rc = SNS_RC_FAILED;
  rc = sns_router_init();
  if(SNS_RC_SUCCESS != rc)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "sns_router_init failed, rc = %d", rc);
  }
  else
  {
    SNS_PRINTF(HIGH, sns_fw_printf, "sns_router_init, rc = %d", rc);
  }
  return rc;
}

/* See sns_sensor::init */
static sns_rc init(sns_sensor *const this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  suid_sensor = (sns_fw_sensor *)this;
  {
    char const name[] = "suid";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg){.buf = name, .buf_len = sizeof(name)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
  }
  {
    char const type[] = "suid";
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
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1, false);
  }
  {
    char const proto_files[] = "sns_suid.proto";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg =
        &((pb_buffer_arg){.buf = proto_files, .buf_len = sizeof(proto_files)});
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

  sns_isafe_list_init(&state->def_sensor_list);
  state->reg_stream = NULL;

  create_timer(this);

  register_timer_sig(this);

  init_sns_router();

  return rc;
}

/* See sns_sensor::init */
static sns_rc deinit(sns_sensor *const this)
{
  delete_timer(this);
  unregister_timer_sig(this);
  return SNS_RC_SUCCESS;
}

/* See sns_sensor::get_sensor_uid */
static sns_sensor_uid const *get_sensor_uid(sns_sensor const *const this)
{
  UNUSED_VAR(this);
  static sns_sensor_uid suid;
  suid = sns_get_suid_lookup();
  return &suid;
}

/**
 * Decode callback for the SUID request message.
 */
static bool pb_suid_request_decode_cb(pb_istream_t *stream,
                                      const pb_field_t *field, void **arg)
{
  UNUSED_VAR(field);
  sns_suid_lookup_request **lookup_request = (sns_suid_lookup_request **)*arg;
  pb_buffer_arg data;
  sns_suid_req suid_req = sns_suid_req_init_default;

  suid_req.data_type.funcs.decode = &pb_decode_string_cb;
  suid_req.data_type.arg = &data;

  if(pb_decode_noinit(stream, sns_suid_req_fields, &suid_req))
  {
    sns_suid_lookup_request *lookup;

    lookup = sns_malloc(SNS_HEAP_MAIN, sizeof(*lookup) + data.buf_len + 1);
    SNS_ASSERT(NULL != lookup);

    sns_isafe_list_item_init(&lookup->list_entry, lookup);
    lookup->register_updates =
        suid_req.has_register_updates && suid_req.register_updates;
    lookup->default_only =
        suid_req.has_default_only ? suid_req.default_only : true;

    sns_memscpy(lookup->data_type, data.buf_len, data.buf, data.buf_len);
    lookup->data_type_len = data.buf_len + 1;
    // Ensure data_type is always null-terminated (may be redundant)
    lookup->data_type[data.buf_len] = '\0';
    *lookup_request = lookup;

    SNS_SPRINTF(
        LOW, &suid_sensor->sensor,
        "Received request for data_type \'%s\' (register %i), default_only=%i",
        lookup->data_type, lookup->register_updates, lookup->default_only);
    return true;
  }
  else
  {
    SNS_SPRINTF(ERROR, &suid_sensor->sensor, "Error decoding SUID Request: %s",
                PB_GET_ERROR(stream));
    return false;
  }
}

/**
 * Parse an incoming lookup request, and store the results in a dynamicallly
 * allocated object.
 *
 * @param[i] request Incoming request from client
 * @param[o] lookup_request Parsed version of the request message
 */
static bool parse_suid_request(sns_request const *request,
                               sns_suid_lookup_request **lookup_request)
{
  sns_std_request req = sns_std_request_init_default;
  pb_istream_t stream =
      pb_istream_from_buffer(request->request, request->request_len);

  req.payload.funcs.decode = &pb_suid_request_decode_cb;
  req.payload.arg = lookup_request;

  if(!pb_decode_noinit(&stream, sns_std_request_fields, &req))
  {
    SNS_SPRINTF(ERROR, &suid_sensor->sensor, "Error decoding request: %s",
                PB_GET_ERROR(&stream));

    if(NULL != *lookup_request)
    {
      sns_free(*lookup_request);
      *lookup_request = NULL;
    }
    return false;
  }

  return true;
}

/**
 * Encode a single SUID within a SUID Event message.
 *
 * @param[i] stream Active stream within an encode function
 * @param[i] field The SUID field within sns_suid_event
 * @param[i] suid SUID to encode
 *
 * @return false if encoding was unsuccessful
 */
static bool encode_suid_in_event(struct pb_ostream_s *stream,
                                 pb_field_t const *field,
                                 sns_sensor_uid const *suid)
{
  sns_std_suid uid;
  sns_memscpy(&uid, sizeof(uid), suid, sizeof(uid));

  if(!pb_encode_tag_for_field(stream, field))
    return false;
  if(!pb_encode_submessage(stream, sns_std_suid_fields, &uid))
    return false;

  return true;
}

/**
 * Given a Sensor, determine whether it meets the search criteria, and if so,
 * encode its SUID on the stream.
 *
 * @param[i] remote sensor
 * @param[i] arg of type sns_encode_suid_arg, set below in encode_suid_cb
 *
 * @return True to iterate over next suid, false otherwise
 */
static bool encode_remote_sensor_suid(struct sns_remote_sensor *remote_sensor,
                                      void *arg)
{
  bool rv = true;
  sns_encode_suid_arg *suid_arg = (sns_encode_suid_arg *)arg;
  if(sns_router_get_remote_sensor_available(remote_sensor))
  {
    bool encode = false;
    sns_sensor_uid suid;
    char data_type[MAX_DATATYPE_LEN];
    // if data type matches then encode
    sns_router_get_remote_sensor_data_type(remote_sensor, data_type,
                                           sizeof(data_type));
    if(0 == strcmp(data_type, suid_arg->data_type))
      encode = true;

    if(encode)
    {
      suid = sns_router_get_remote_sensor_suid(remote_sensor);
      rv = encode_suid_in_event(suid_arg->stream, suid_arg->field, &suid);

      if(rv && suid_arg->default_only)
        suid_arg->default_encoded = true;
    }
  }

  return rv;
}

/**
 * Given a Sensor, determine whether it meets the search criteria, and if so,
 * encode its SUID on the stream.
 *
 * @param[i] sensor
 * @param[i] arg Of type sns_encode_suid_arg, set below in encode_suid_cb
 *
 * @return True if encoding was successful, false otherwise
 */
static bool encode_sensor_suid(struct sns_attribute_info *attr_info, void *arg)
{
  bool rv = true;
  sns_encode_suid_arg *suid_arg = (sns_encode_suid_arg *)arg;

  if(!suid_arg->default_only || !suid_arg->default_encoded)
  {
    if(sns_attr_info_get_available(attr_info))
    {
      bool encode = false;
      sns_sensor_uid suid;

      if('\0' == suid_arg->data_type[0])
      {
        char platform_sensor_vendor[] = "QTI_internal";
        char vendor[sizeof(platform_sensor_vendor) + 1];
        sns_attr_info_get_vendor(attr_info, vendor, sizeof(vendor));
        if(0 != strcmp(platform_sensor_vendor, vendor))
          encode = true;
      }
      else
      {
        char data_type[MAX_DATATYPE_LEN];

        sns_attr_info_get_data_type(attr_info, data_type, sizeof(data_type));
        if(0 == strcmp(data_type, suid_arg->data_type))
          encode = true;
      }

      if(suid_arg->default_only)
      {
        if(!suid_arg->use_matching_against_default &&
           !sns_attr_info_get_default(attr_info))
          encode = false;
      }

      if(encode)
      {
        suid = sns_attr_info_get_suid(attr_info);
        rv = encode_suid_in_event(suid_arg->stream, suid_arg->field, &suid);

        if(rv && suid_arg->default_only)
          suid_arg->default_encoded = true;
      }
    }
  }

  return true;
}

/**
 * Given a Sensor type, determine whether it is present in the default sensors
 * list.
 *
 * @param[i] suid_arg Of type sns_encode_suid_arg for the lookup type
 *
 * @return True if Sensor type in default sensors list, false otherwise
 */
static bool is_sensor_in_default_list(sns_encode_suid_arg *suid_arg)
{
  sns_suid_sensor_state *state =
      (sns_suid_sensor_state *)suid_sensor->sensor.state->state;
  sns_isafe_list_iter iter;

  for(sns_isafe_list_iter_init(&iter, &state->def_sensor_list, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_def_sensor_info *item =
        (sns_def_sensor_info *)sns_isafe_list_iter_get_curr_data(&iter);
    if(0 == strcmp(item->datatype, suid_arg->data_type))
    {
      return true;
    }
  }

  return false;
}

/**
 * Encode all SUIDs which match the client-specified query.
 *
 * If the default sensor was requested, but not found, instead encode the
 * first matching SUID only if there is no default specified for the type
 * through registry.
 *
 * @param[i] arg Data Type to search for (sns_encode_suid_arg)
 *
 * @return True if encoding was successful, false otherwise
 */
static bool encode_suid_cb(struct pb_ostream_s *stream, pb_field_t const *field,
                           void *const *arg)
{
  sns_encode_suid_arg *suid_arg = (sns_encode_suid_arg *)*arg;

  suid_arg->stream = stream;
  suid_arg->field = field;
  suid_arg->default_encoded = false;

  /* If Sensor is present in default_sensor.json from registry do not use the
   * first */
  suid_arg->use_matching_against_default =
      is_sensor_in_default_list(suid_arg) ? false : true;
  sns_attr_svc_sensor_foreach(&encode_sensor_suid, suid_arg);

  if(!suid_arg->native_only)
  {
    sns_remote_sensor_foreach(&encode_remote_sensor_suid, suid_arg);
  }
  // PEND: Consider returning false if encode failed (SNS_RC_SUCCESS != rc)
  return true;
}

/**
 * Form and send an SUID event based on the given request.
 *
 * data_type string will be an exact copy of what was sent by the client.
 *
 * @param[i] instance Instance on which this request was assigned
 * @param[i] request Perform the request search and return publish results
 */
static void send_suid_event(sns_sensor_instance *instance,
                            sns_suid_lookup_request const *request)
{
  sns_suid_event suid_event = sns_suid_event_init_default;
  sns_encode_suid_arg arg =
      (sns_encode_suid_arg){.data_type = request->data_type,
                            .default_only = request->default_only,
                            .native_only = request->native_only,
                            .default_encoded = false,
                            .use_matching_against_default = false};

  SNS_SPRINTF(LOW, sns_fw_printf, "send_suid_event dt=%s, default=%i",
              request->data_type, request->default_only);

  pb_buffer_arg data = (pb_buffer_arg){.buf = request->data_type,
                                       .buf_len = request->data_type_len - 1};

  suid_event.data_type.funcs.encode = &pb_encode_string_cb;
  suid_event.data_type.arg = &data;
  suid_event.suid.funcs.encode = &encode_suid_cb;
  suid_event.suid.arg = &arg;

  pb_send_event(instance, sns_suid_event_fields, &suid_event,
                sns_get_system_time(), SNS_SUID_MSGID_SNS_SUID_EVENT, NULL);
}

/**
 * Add a new lookup request to the state of an existing SUID Sensor Instance.
 *
 * @param[i] state Sensor Instance state
 * @param[i] new_request Parsed request from a client
 *
 * @return True if active requests remain for this client
 */
static bool add_req_to_client(sns_suid_sensor_instance_config *state,
                              sns_suid_lookup_request *new_request)
{
  sns_isafe_list_iter lookups;

  for(sns_isafe_list_iter_init(&lookups, &state->requests, true);
      NULL != sns_isafe_list_iter_curr(&lookups);)
  {
    sns_suid_lookup_request *request =
        (sns_suid_lookup_request *)sns_isafe_list_iter_get_curr_data(&lookups);

    uint32_t len = (request->data_type_len < new_request->data_type_len
                        ? request->data_type_len
                        : new_request->data_type_len);

    if(0 == sns_memcmp(request->data_type, new_request->data_type, len))
    {
      sns_isafe_list_iter_remove(&lookups);
      sns_free(request);
    }
    else
    {
      sns_isafe_list_iter_advance(&lookups);
    }
  }

  sns_isafe_list_iter_insert(&lookups, &new_request->list_entry, false);

  return 0 != sns_isafe_list_iter_len(&lookups);
}

/**
 * Lookup among all SUID client instances for a one-shot update request
 *
 * @param[i] this The SUID Lookup Sensor
 *
 * @return True if no one-shot update request is found
 */
static bool is_stop_timer_required(sns_sensor *const this)
{
  bool rv = true;
  for(sns_sensor_instance *instance = this->cb->get_sensor_instance(this, true);
      NULL != instance; instance = this->cb->get_sensor_instance(this, false))
  {
    sns_isafe_list_iter iter;
    sns_suid_sensor_instance_config *state =
        (sns_suid_sensor_instance_config *)instance->state->state;

    sns_isafe_list_iter_init(&iter, &state->requests, true);

    while(NULL != sns_isafe_list_iter_curr(&iter))
    {
      sns_suid_lookup_request *lookup_req =
          (sns_suid_lookup_request *)sns_isafe_list_iter_get_curr_data(&iter);

      if(!lookup_req->register_updates)
      {
        rv = false;
        return rv;
      }
      else
      {
        sns_isafe_list_iter_advance(&iter);
      }
    }
  }
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
  sns_sensor_instance *curr_inst =
      sns_sensor_util_find_instance(this, curr_req, get_sensor_uid(this));
  sns_sensor_instance *rv_inst = NULL;

  // Remove is called when a client has closed the data stream.  Since an
  // instance only serves a single client, we indicate to the Framework that it
  // should be removed, by removing all associated client requests (which
  // should be exactly one).
  if(remove)
  {
    if(NULL != curr_inst)
    {
      curr_inst->cb->remove_client_request(curr_inst, curr_req);
      this->cb->remove_instance(curr_inst);
      // stop the timer only when no remaining clients have a one-shot update
      // request
      if(is_stop_timer_required(this))
      {
        stop_timer(this);
      }
    }
  }
  else if((NULL != curr_inst) &&
          (new_req->message_id == SNS_STD_MSGID_SNS_STD_FLUSH_REQ))
  {
    sns_fw_sensor *fw_sensor = (sns_fw_sensor *)this;
    sns_sensor_util_send_flush_event(fw_sensor->diag_config.suid, curr_inst);
    rv_inst = curr_inst;
  }
  else if(SNS_SUID_MSGID_SNS_SUID_REQ == new_req->message_id ||
          SNS_SUID_V2_MSGID_SNS_SUID_V2_REQ == new_req->message_id)
  {
    sns_suid_lookup_request *lookup_request = NULL;

    if(parse_suid_request(new_req, &lookup_request))
    {
      lookup_request->native_only =
          (SNS_SUID_MSGID_SNS_SUID_REQ == new_req->message_id) ? true : false;
      // start the timer only when the client is not requesting continuous
      // updates
      if(!lookup_request->register_updates)
      {
        start_timer(this);
      }

      // For this Sensor, Instances are created effectively only to hold state
      // Hence, we don't bother calling set_client_config
      rv_inst = (NULL != curr_inst)
                    ? curr_inst
                    : this->cb->create_instance(
                          this, sizeof(sns_suid_sensor_instance_config));

      // The Framework does book-keeping based on client-requests, so we still
      // need to ensure that we remove the "replaced" client request, and add
      // the new one
      if(NULL != curr_inst && NULL != curr_req)
        curr_inst->cb->remove_client_request(curr_inst, curr_req);

      // Sending an event requires that the client be added to the queue first
      rv_inst->cb->add_client_request(rv_inst, new_req);
      send_suid_event(rv_inst, lookup_request);

      if(!add_req_to_client(
             (sns_suid_sensor_instance_config *)rv_inst->state->state,
             lookup_request))
      {
        rv_inst->cb->remove_client_request(rv_inst, new_req);
        this->cb->remove_instance(rv_inst);
        rv_inst = &sns_instance_no_error;
      }
    }
  }
  else
  {
    rv_inst = NULL;
  }

  return rv_inst;
}

static void suid_request_registry_group(sns_sensor *this, char const *grp_name)
{
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;

  pb_buffer_arg data =
      (pb_buffer_arg){.buf = grp_name, .buf_len = strlen(grp_name) + 1};

  uint8_t buffer[100];
  int32_t encoded_len = 0;
  sns_registry_read_req read_request;

  read_request.name.arg = &data;
  read_request.name.funcs.encode = pb_encode_string_cb;

  encoded_len = pb_encode_request(buffer, sizeof(buffer), &read_request,
                                  sns_registry_read_req_fields, NULL);
  if(encoded_len <= 0)
  {
    SNS_PRINTF(ERROR, this, "encoding failed");
  }
  else
  {
    sns_request request =
        (sns_request){.request_len = encoded_len,
                      .request = buffer,
                      .message_id = SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_REQ};

    state->reg_stream->api->send_request(state->reg_stream, &request);
  }
}

/**
 * Send requests to the registry sensor, for the list of default
 * sensor attributes.
 *
 * @param[i] this The SUID Lookup Sensor
 */
static void suid_send_registry_request(sns_sensor *this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_suid_sensor_state *state = (sns_suid_sensor_state *)this->state->state;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_svc =
      (sns_stream_service *)service_mgr->get_service(service_mgr,
                                                     SNS_STREAM_SERVICE);

  rc = stream_svc->api->create_sensor_stream(stream_svc, this, registry_suid,
                                             &state->reg_stream);

  if(SNS_RC_SUCCESS == rc)
  {
    suid_request_registry_group(this, reg_default_sensors);
  }
}

static void task_func_suid_sensor_apprise(char *data_type)
{
  /* when registry sensor become available, send registry requests */
  if(0 == strcmp(data_type, "registry"))
  {
    suid_send_registry_request((sns_sensor *)suid_sensor);
  }
  else if(0 == strcmp(data_type, "sim"))
  {
    disable_timer((sns_sensor *)suid_sensor);
  }

  default_sensor_match_attributes(data_type);

  for(sns_sensor_instance *instance =
          suid_sensor->sensor.cb->get_sensor_instance(&suid_sensor->sensor,
                                                      true);
      NULL != instance; instance = suid_sensor->sensor.cb->get_sensor_instance(
                            &suid_sensor->sensor, false))
  {
    sns_isafe_list_iter iter;
    sns_suid_sensor_instance_config *state =
        (sns_suid_sensor_instance_config *)instance->state->state;

    for(sns_isafe_list_iter_init(&iter, &state->requests, true);
        NULL != sns_isafe_list_iter_curr(&iter);
        sns_isafe_list_iter_advance(&iter))
    {
      sns_suid_lookup_request *lookup_req =
          (sns_suid_lookup_request *)sns_isafe_list_iter_get_curr_data(&iter);

      if(0 == strcmp(lookup_req->data_type, data_type) ||
         '\0' == lookup_req->data_type[0])
      {
        send_suid_event(instance, lookup_req);
      }
    }
  }
}

static void __attribute__((noinline)) task_func_suid_sensor_bi(void *arg)
{
  char *data_type = (char *)arg;
  task_func_suid_sensor_apprise(data_type);
  sns_free(data_type);
}

SNS_SECTION(".text.sns") static sns_rc task_func_suid_sensor(void *arg)
{
  SNS_ISLAND_EXIT();
  task_func_suid_sensor_bi(arg);
  return SNS_RC_SUCCESS;
}

/*===========================================================================
  Public Function Definitions
  ===========================================================================*/

/**
 * Add task to handle availability attribute of this data type
 */
void sns_suid_sensor_add_task(char const *data_type)
{
  size_t data_type_arg_size = strlen(data_type) + 1;
  char *data_type_arg = sns_malloc(SNS_HEAP_MAIN, data_type_arg_size);
  if(NULL != data_type_arg)
  {
    sns_tmgr_task_args task_args;
    task_args.instance = NULL;
    task_args.task_lock = suid_sensor->library->library_lock;
    sns_strlcpy(data_type_arg, data_type, data_type_arg_size);
    sns_thread_manager_add(suid_sensor->library, task_func_suid_sensor,
                           data_type_arg, SNS_TMGR_TASK_PRIO_MED, &task_args);
  }
}

/*===========================================================================
  Public Data Definitions
  ===========================================================================*/

const sns_sensor_api suid_sensor_api = {.struct_len = sizeof(sns_sensor_api),
                                        .init = &init,
                                        .deinit = &deinit,
                                        .get_sensor_uid = &get_sensor_uid,
                                        .set_client_request =
                                            &set_client_request,
                                        .notify_event = &notify_event};
