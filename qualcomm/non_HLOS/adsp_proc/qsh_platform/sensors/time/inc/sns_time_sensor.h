#pragma once
/** ============================================================================
 * @file sns_time_sensor.h
 *
 * @brief sns_time sensor
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdint.h>

#include "pb_decode.h"
#include "pb_encode.h"

#include "sns_pb_util.h"
#include "sns_printf.h"
#include "sns_rc.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_sensor_util.h"
#include "sns_signal.h"
#include "sns_suid_util.h"
#include "sns_time.h"
#include "sns_types.h"

/*=============================================================================
  Macros and Constants
  ===========================================================================*/
#define TIME_SUID                                                              \
  {                                                                            \
    .sensor_uid = {                                                            \
      0xF2,                                                                    \
      0x2A,                                                                    \
      0xAF,                                                                    \
      0x0C,                                                                    \
      0xED,                                                                    \
      0xE6,                                                                    \
      0x48,                                                                    \
      0x11,                                                                    \
      0xAF,                                                                    \
      0xD6,                                                                    \
      0x05,                                                                    \
      0x4B,                                                                    \
      0x10,                                                                    \
      0x16,                                                                    \
      0x8B,                                                                    \
      0x95                                                                     \
    }                                                                          \
  }

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR   3600

/*=============================================================================
  Type definitions
  ===========================================================================*/
typedef struct
{
  SNS_SUID_LOOKUP_DATA(1) suid_lookup_data; // Registry
  sns_data_stream *registry_stream;
  bool published_available;

  uint64_t utc_time;
  sns_time utc_time_at_timetick;
  int32_t timezone_offset;
  sns_time timezone_at_timetick;

  sns_signal_handle *utc_time_signal_handle;
  sns_osa_thread_signal *utc_time_thread_signal;
  void *utc_time_rcevt_handle;

  sns_signal_handle *timezone_offset_signal_handle;
  sns_osa_thread_signal *timezone_offset_thread_signal;
  void *timezone_offset_rcevt_handle;
} sns_time_sensor_state;

void sns_time_publish_attributes(sns_sensor *const this);
void sns_time_publish_available(sns_sensor *const this);
sns_rc sns_time_send_utc_time_event(sns_sensor *const this);
sns_rc sns_time_send_time_zone_offset_event(sns_sensor *const this);

sns_rc sns_time_init(sns_sensor *const this);
sns_rc sns_time_deinit(sns_sensor *const this);
sns_rc sns_time_notify_event(sns_sensor *const this);
sns_sensor_uid const *sns_time_get_sensor_uid(sns_sensor const *this);
sns_sensor_instance *
sns_time_set_client_request(sns_sensor *const this,
                            struct sns_request const *exist_request,
                            struct sns_request const *new_request, bool remove);