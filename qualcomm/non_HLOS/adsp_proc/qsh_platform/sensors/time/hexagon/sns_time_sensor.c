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
#include "sns_time_sensor.h"
#include "sns_time_events.h"
#include "time_svc.h"

/*=============================================================================
  Macros and Constants
  ===========================================================================*/
/**
 * @brief This value is to prevent the sensor from publishing available on the
 * very first bootup of the device before any network is camped.
 */
#define TEN_YEARS_IN_SECONDS 315532800

/*=============================================================================
  Static Functions
  ===========================================================================*/
static void sns_time_query_utc_time(sns_sensor *const this)
{
  time_network_time_type network_time = {.unit = TIME_SECS};
  uint64_t *utc_time = (uint64_t *)network_time.time;
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  if(time_get_network_time(&network_time) == T_SUCCESS)
  {
    SNS_PRINTF(LOW, this, "UTC time: 0x%X%X", (uint32_t)(network_time.time[1]),
               (uint32_t)network_time.time[0]);
    if(likely(*utc_time > TEN_YEARS_IN_SECONDS))
    {
      state->utc_time = *utc_time;
      state->utc_time_at_timetick = network_time.at_timetick;
    }
  }
  else
  {
    SNS_PRINTF(HIGH, this, "Failed to get UTC time");
  }
}

/*----------------------------------------------------------------------------*/
static void sns_time_utc_time_cb(sns_sensor *this, sns_signal_handle *handle,
                                 void *args)
{
  UNUSED_VAR(args);
  UNUSED_VAR(handle);
  SNS_PRINTF(LOW, this, "UTC time change CB");
  sns_time_query_utc_time(this);
  sns_time_publish_available(this);
  sns_time_send_utc_time_event(this);
}

/*----------------------------------------------------------------------------*/
static void sns_time_query_timezone(sns_sensor *const this)
{
  time_network_timezone_type network_timezone = {0};
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  if(time_get_network_timezone(&network_timezone) == T_SUCCESS)
  {
    SNS_PRINTF(LOW, this, "Timezone offset: %d", network_timezone.timezone);

    state->timezone_offset = SECONDS_PER_MINUTE * network_timezone.timezone;
    state->timezone_at_timetick = network_timezone.at_timetick_timezone;
  }
  else
  {
    SNS_PRINTF(HIGH, this, "Failed to get timezone offset");
  }
}

/*----------------------------------------------------------------------------*/
static void sns_time_time_zone_cb(sns_sensor *this, sns_signal_handle *handle,
                                  void *args)
{
  UNUSED_VAR(args);
  UNUSED_VAR(handle);
  SNS_PRINTF(LOW, this, "Time zone offset change CB");
  sns_time_query_timezone(this);
  sns_time_publish_available(this);
  sns_time_send_time_zone_offset_event(this);
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/
sns_rc sns_time_init(sns_sensor *const this)
{
  sns_rc rc = SNS_RC_SUCCESS;
  uint32_t mask = 0;
  sns_signal_attr signal_attr;
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  sns_time_publish_attributes(this);

  sns_signal_attr_init(&signal_attr);
  sns_signal_attr_set(&signal_attr, this, &sns_time_utc_time_cb, NULL);
  sns_signal_register(&signal_attr, &state->utc_time_signal_handle);

  sns_signal_get_thread_signal(state->utc_time_signal_handle,
                               &state->utc_time_thread_signal);
  sns_signal_get_mask(state->utc_time_signal_handle, &mask);

  state->utc_time_rcevt_handle = sns_time_events_register(
      TIME_SVC_NETWORK_TIME_AVAL, state->utc_time_thread_signal, mask);

  sns_signal_attr_init(&signal_attr);
  sns_signal_attr_set(&signal_attr, this, &sns_time_time_zone_cb, NULL);
  sns_signal_register(&signal_attr, &state->timezone_offset_signal_handle);

  sns_signal_get_thread_signal(state->timezone_offset_signal_handle,
                               &state->timezone_offset_thread_signal);
  sns_signal_get_mask(state->timezone_offset_signal_handle, &mask);

  state->timezone_offset_rcevt_handle =
      sns_time_events_register(TIME_SVC_NETWORK_TIMEZONE_AVAL,
                         state->timezone_offset_thread_signal, mask);

  if(state->utc_time_rcevt_handle == NULL)
  {
    SNS_PRINTF(HIGH, this, "Failed to register for UTC time change");
  }

  if(state->timezone_offset_rcevt_handle == NULL)
  {
    SNS_PRINTF(HIGH, this, "Failed to register for time zone offset change");
  }

  if(state->utc_time_rcevt_handle == NULL ||
     state->timezone_offset_rcevt_handle == NULL)
  {
    rc = SNS_RC_INVALID_STATE;
  }
  else
  {
    sns_time_query_utc_time(this);
    sns_time_query_timezone(this);
    sns_time_publish_available(this);
  }

  return rc;
}

/*----------------------------------------------------------------------------*/
sns_rc sns_time_deinit(sns_sensor *const this)
{
  uint32_t mask = 0;
  sns_time_sensor_state *state = (sns_time_sensor_state *)this->state->state;

  sns_signal_get_mask(state->utc_time_signal_handle, &mask);
  sns_time_events_unregister(state->utc_time_rcevt_handle,
                       state->utc_time_thread_signal, mask);
  sns_signal_unregister(&state->utc_time_signal_handle);

  sns_signal_get_mask(state->timezone_offset_signal_handle, &mask);
  sns_time_events_unregister(state->timezone_offset_rcevt_handle,
                       state->timezone_offset_thread_signal, mask);
  sns_signal_unregister(&state->timezone_offset_signal_handle);

  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/
sns_rc sns_time_notify_event(sns_sensor *const this)
{
  UNUSED_VAR(this);
  return SNS_RC_SUCCESS;
}

/*------------------------------------------------------------------------------
  Public Data Definitions
  * ---------------------------------------------------------------------------*/
const sns_sensor_api sns_time_api = {
    .struct_len = sizeof(sns_sensor_api),
    .init = &sns_time_init,
    .deinit = &sns_time_deinit,
    .get_sensor_uid = &sns_time_get_sensor_uid,
    .set_client_request = &sns_time_set_client_request,
    .notify_event = &sns_time_notify_event,
};
