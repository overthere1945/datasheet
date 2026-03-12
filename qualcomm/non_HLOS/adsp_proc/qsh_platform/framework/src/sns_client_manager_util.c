/*==============================================================================
  @file sns_client_manager_util.c

  Utility functions related to creating encoded request

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "pb_encode.h"
#include "sns_assert.h"
#include "sns_client_manager_util.h"
#include "sns_memmgr.h"
#include "sns_mem_util.h"
#include "sns_pb_util.h"
#include "sns_printf.h"
#include "sns_request.h"
#include "sns_resampler.pb.h"
#include "sns_std.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_threshold.pb.h"
#include "sns_island.h"

/*=============================================================================
  Static data
  ============================================================================*/

static sns_sensor_uid resampler_suid;
static sns_sensor_uid threshold_suid;
static bool resampler_suid_populated = false;
static bool threshold_suid_populated = false;

/*=============================================================================
  Static Function Definitions
  ============================================================================*/

/*=============================================================================
  Public Function Definitions
  ============================================================================*/

bool sns_cm_is_valid_resampler_suid(void)
{
  return resampler_suid_populated;
}

bool sns_cm_is_valid_threshold_suid(void)
{
  return threshold_suid_populated;
}

void sns_cm_store_resampler_suid(sns_sensor_uid *suid)
{
  sns_memscpy(&resampler_suid, sizeof(resampler_suid), suid,
              sizeof(sns_sensor_uid));
  resampler_suid_populated = true;
  SNS_PRINTF(HIGH, sns_fw_printf, "Stored resampler SUID");
}

void sns_cm_get_resampler_suid(sns_sensor_uid *suid)
{
  sns_memscpy(suid, sizeof(sns_sensor_uid), &resampler_suid,
              sizeof(resampler_suid));
}

void sns_cm_store_threshold_suid(sns_sensor_uid *suid)
{
  sns_memscpy(&threshold_suid, sizeof(threshold_suid), suid,
              sizeof(sns_sensor_uid));
  threshold_suid_populated = true;
  SNS_PRINTF(HIGH, sns_fw_printf, "Stored threshold SUID");
}

void sns_cm_get_threshold_suid(sns_sensor_uid *suid)
{
  sns_memscpy(suid, sizeof(sns_sensor_uid), &threshold_suid,
              sizeof(threshold_suid));
}

uint32_t sns_cm_util_get_resampler_msg_id(void)
{
  return (uint32_t)SNS_RESAMPLER_MSGID_SNS_RESAMPLER_CONFIG;
}

uint32_t sns_cm_util_get_threshold_msg_id(void)
{
  return (uint32_t)SNS_THRESHOLD_MSGID_SNS_THRESHOLD_CONFIG;
}

size_t sns_cm_enc_resamp_req(void *buffer, size_t buffer_len,
                             sns_sensor_uid suid,
                             sns_cm_resampler_param *resamp_param,
                             sns_std_request *request)
{
  size_t encoded_len = 0;

  sns_resampler_config resampler_config = sns_resampler_config_init_default;

  // Encode resampler request
  sns_memscpy(&resampler_config.sensor_uid, sizeof(resampler_config.sensor_uid),
              &suid, sizeof(suid));

  resampler_config.resampled_rate = resamp_param->resampled_rate;
  resampler_config.rate_type = (sns_resampler_rate)resamp_param->rate_type;
  resampler_config.filter = resamp_param->filter;
  resampler_config.has_event_gated = resamp_param->event_gated;
  resampler_config.event_gated = resamp_param->event_gated;

  encoded_len = pb_encode_request(buffer, buffer_len, &resampler_config,
                                  sns_resampler_config_fields, request);

  return encoded_len;
}

size_t sns_cm_enc_thresh_req(void *buffer, size_t buffer_len,
                             sns_sensor_uid suid,
                             sns_cm_threshold_param *threshold_param,
                             sns_std_request *request)
{
  size_t encoded_len = 0;

  sns_threshold_config threshold_config = sns_threshold_config_init_default;

  // populate threshold request configuration
  sns_memscpy(&threshold_config.sensor_uid, sizeof(threshold_config.sensor_uid),
              &suid, sizeof(suid));

  threshold_config.threshold_type =
      (sns_threshold_type)threshold_param->threshold_type;
  threshold_config.threshold_val.funcs.encode = &pb_encode_float_arr_cb;
  threshold_config.threshold_val.arg = threshold_param->threshold_val_arg;
  threshold_config.payload_cfg_msg_id = threshold_param->payload_cfg_msg_id;
  threshold_config.payload.funcs.encode = &pb_encode_string_cb;
  threshold_config.payload.arg = threshold_param->payload;

  // Encode Threshold config request
  encoded_len = pb_encode_request(buffer, buffer_len, &threshold_config,
                                  sns_threshold_config_fields, request);

  return encoded_len;
}

size_t sns_cm_enc_thresh_resamp_req(void *buffer, size_t buffer_len,
                                    sns_sensor_uid suid,
                                    sns_cm_resampler_param *resamp_param,
                                    sns_cm_threshold_param *threshold_param,
                                    sns_std_request *request)
{
  uint8_t resampler_buffer[100];
  size_t encoded_len = 0;

  sns_threshold_config threshold_config = sns_threshold_config_init_default;
  sns_resampler_config resampler_config = sns_resampler_config_init_default;

  // populate resampler request configuration
  sns_memscpy(&resampler_config.sensor_uid, sizeof(resampler_config.sensor_uid),
              &suid, sizeof(suid));

  resampler_config.resampled_rate = resamp_param->resampled_rate;
  resampler_config.rate_type = (sns_resampler_rate)resamp_param->rate_type;
  resampler_config.filter = resamp_param->filter;
  resampler_config.has_event_gated = resamp_param->event_gated;
  resampler_config.event_gated = resamp_param->event_gated;

  // Encode threshold config request
  pb_ostream_t stream =
      pb_ostream_from_buffer(resampler_buffer, sizeof(resampler_buffer));

  pb_buffer_arg resampler_arg = (pb_buffer_arg){.buf = NULL, .buf_len = 0};

  if(!pb_encode(&stream, sns_resampler_config_fields, &resampler_config))
  {
    return 0;
  }
  else
  {
    if(0 < stream.bytes_written)
    {
      resampler_arg.buf = resampler_buffer;
      resampler_arg.buf_len = stream.bytes_written;
    }
  }
  // Populate threshold request configuration
  sns_memscpy(&threshold_config.sensor_uid, sizeof(threshold_config.sensor_uid),
              &resampler_suid, sizeof(resampler_suid));

  threshold_config.threshold_type =
      (sns_threshold_type)threshold_param->threshold_type;
  threshold_config.threshold_val.funcs.encode = &pb_encode_float_arr_cb;
  threshold_config.threshold_val.arg = threshold_param->threshold_val_arg;
  threshold_config.payload_cfg_msg_id =
      SNS_RESAMPLER_MSGID_SNS_RESAMPLER_CONFIG;

  // Encode Threshold config request
  threshold_config.payload.funcs.encode = &pb_encode_string_cb;
  threshold_config.payload.arg = &resampler_arg;

  encoded_len = pb_encode_request(buffer, buffer_len, &threshold_config,
                                  sns_threshold_config_fields, request);

  return encoded_len;
}
