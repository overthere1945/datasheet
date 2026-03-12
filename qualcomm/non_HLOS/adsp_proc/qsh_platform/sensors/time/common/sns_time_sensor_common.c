/** ============================================================================
 * @file sns_time_sensor.c
 *
 * @brief sns_time sensor implementation
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include "sns_attribute_util.h"
#include "sns_time.pb.h"
#include "sns_time_sensor.h"

/*=============================================================================
  Macros and Constants
  ===========================================================================*/

/*=============================================================================
  Type definitions
  ===========================================================================*/

/*=============================================================================
  Globals
  ===========================================================================*/
static const sns_sensor_uid sensor_uid = TIME_SUID;

/*------------------------------------------------------------------------------
  Public Function Definitions
  * ---------------------------------------------------------------------------*/
void sns_time_publish_attributes(sns_sensor *const this)
{
  {
    char const name[] = "sns_time";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg){.buf = name, .buf_len = sizeof(name)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
  }
  {
    char const type[] = "time";
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
    char const proto_files[] = "sns_time.proto";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg =
        &((pb_buffer_arg){.buf = proto_files, .buf_len = sizeof(proto_files)});
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API, &value, 1, false);
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
    value.sint = SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_STREAM_TYPE, &value, 1,
                          true);
  }
}

/*----------------------------------------------------------------------------*/
sns_rc sns_time_send_utc_time_event(sns_sensor *const this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_sensor_instance *instance = this->cb->get_sensor_instance(this, true);
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  sns_time_event sns_time_utc_time_event = sns_time_event_init_default;

  if(instance != NULL)
  {
    sns_time_utc_time_event.has_utc_time = true;
    sns_time_utc_time_event.utc_time = state->utc_time;

    if(!pb_send_event(instance, sns_time_event_fields, &sns_time_utc_time_event,
                      state->utc_time_at_timetick,
                      SNS_TIME_MSGID_SNS_TIME_EVENT, NULL))
    {
      SNS_INST_PRINTF(ERROR, this, "Error sending UTC time event");
      rc = SNS_RC_FAILED;
    }
  }
  return rc;
}

/*----------------------------------------------------------------------------*/
sns_rc sns_time_send_time_zone_offset_event(sns_sensor *const this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_sensor_instance *instance = this->cb->get_sensor_instance(this, true);
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  sns_time_event sns_time_time_zone_event = sns_time_event_init_default;

  if(instance != NULL)
  {
    sns_time_time_zone_event.has_time_zone_offset = true;
    sns_time_time_zone_event.time_zone_offset = state->timezone_offset;

    if(!pb_send_event(instance, sns_time_event_fields,
                      &sns_time_time_zone_event, state->timezone_at_timetick,
                      SNS_TIME_MSGID_SNS_TIME_EVENT, NULL))
    {
      SNS_INST_PRINTF(ERROR, this, "Error sending time zone event");
      rc = SNS_RC_FAILED;
    }
  }
  return rc;
}

/*----------------------------------------------------------------------------*/
void sns_time_publish_available(sns_sensor *const this)
{
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  if(!state->published_available && state->utc_time_at_timetick &&
     state->timezone_at_timetick)
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = true;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1,
                          true);

    state->published_available = true;
    SNS_PRINTF(LOW, this, "Publish available");
  }
}

/*----------------------------------------------------------------------------*/
sns_sensor_uid const *sns_time_get_sensor_uid(sns_sensor const *this)
{
  UNUSED_VAR(this);
  return &sensor_uid;
}

/*----------------------------------------------------------------------------*/
sns_sensor_instance *
sns_time_set_client_request(sns_sensor *const this,
                            sns_request const *exist_request,
                            sns_request const *new_request, bool remove)

{
  bool flush_req_handled = false;
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;
  sns_sensor_instance *instance = this->cb->get_sensor_instance(this, true);

  if(remove)
  {
    if(NULL != instance)
    {
      instance->cb->remove_client_request(instance, exist_request);
    }
  }
  else if(NULL != new_request)
  {
    SNS_PRINTF(LOW, this, "sns_time set_client_req: msg_id=(%d)",
               new_request->message_id);

    if(SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG == new_request->message_id)
    {
      if(NULL == instance)
      {
        instance = this->cb->create_instance(this, 0);
      }

      if(NULL != instance && NULL == exist_request)
      {
        instance->cb->add_client_request(instance, new_request);
        // Send event that will have the earliest timestamp first
        if(state->utc_time_at_timetick < state->timezone_at_timetick)
        {
          sns_time_send_utc_time_event(this);
          sns_time_send_time_zone_offset_event(this);
        }
        else
        {
          sns_time_send_time_zone_offset_event(this);
          sns_time_send_utc_time_event(this);
        }
      }
    }
    else if(SNS_STD_MSGID_SNS_STD_FLUSH_REQ == new_request->message_id &&
            NULL != instance && NULL != exist_request)
    {
      sns_sensor_util_send_flush_event(this->sensor_api->get_sensor_uid(this),
                                       instance);
      flush_req_handled = true;
    }
    else
    {
      instance = NULL;
    }
  }
  else
  {
    instance = NULL;
  }

  if(NULL != instance &&
     NULL == instance->cb->get_client_request(
                 instance, this->sensor_api->get_sensor_uid(this), true))
  {
    this->cb->remove_instance(instance);
  }

  if(flush_req_handled)
  {
    instance = &sns_instance_no_error;
  }

  return instance;
}
