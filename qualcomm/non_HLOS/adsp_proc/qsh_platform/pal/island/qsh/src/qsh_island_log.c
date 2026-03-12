/*============================================================================
  @file qsh_island_log.c

  @brief
  API definitions to submit island transition logs

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#include <stdbool.h>
#include <stdatomic.h>

#include "qsh_island_hexagon.h"
#include "qsh_island_test.h"
#include "qsh_island_log.h"
#include "sns_assert.h"
#include "sns_diag.pb.h"
#include "sns_fw_diag_types.h"
#include "sns_fw_diag_service.h"
#include "sns_island_util.h"
#include "sns_memmgr.h"
#include "sns_pb_util.h"
#include "sns_printf_int.h"
#include "sns_time.h"

/*============================================================================
  type definitions
  ============================================================================*/

typedef struct qsh_island_log_t
{
  sns_time island_entry_ts;
  sns_time island_exit_ts;
  sns_island_notification_cb_handle *uSleep_handle;
  uint64_t total_island_us;
  uint32_t island_entry_count;
  uint32_t island_exit_count;
  sns_island_state current_state;

} qsh_island_log_t;

/*============================================================================
  Static Data
  ============================================================================*/

qsh_island_log_t qsh_island_log SNS_ISLAND_DATA;

/*============================================================================
  Static Functions
  ============================================================================*/

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static sns_rc qsh_island_log_transition_encode_payload(
    void *log, size_t log_size, size_t encoded_log_size, void *encoded_log,
    size_t *bytes_written)
{
  UNUSED_VAR(log_size);
  sns_rc rc = SNS_RC_SUCCESS;
  if(NULL == log || 0 == log_size || NULL == encoded_log ||
     0 == encoded_log_size || NULL == bytes_written)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
    void *log_payload;
    pb_ostream_t stream = pb_ostream_from_buffer(encoded_log, encoded_log_size);

    sns_diag_island_tx_log *log_payload_internal = &(
        ((sns_diag_island_log *)log)->sns_diag_island_log_payloads.log_payload);
    log_payload = (void *)log_payload_internal;

    if(!pb_encode(&stream, sns_diag_island_tx_log_fields, log_payload))
    {
      SNS_SPRINTF(ERROR, sns_fw_printf,
                  "Error encoding encoded_log_internal : %s",
                  PB_GET_ERROR(&stream));
      rc = SNS_RC_FAILED;
    }

    if(SNS_RC_SUCCESS == rc)
    {
      *bytes_written = stream.bytes_written;
    }
  }

  return rc;
}

SNS_ISLAND_CODE static sns_rc
qsh_island_log_transition_encode_cb(void *log, size_t log_size,
                                    size_t encoded_log_size, void *encoded_log,
                                    size_t *bytes_written)
{
  UNUSED_VAR(log_size);
  sns_rc rc = SNS_RC_SUCCESS;
  sns_rc rc1 = SNS_RC_SUCCESS;
  if(NULL == log || 0 == log_size || NULL == encoded_log ||
     0 == encoded_log_size || NULL == bytes_written)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
    uint32_t sizeof_oneof_payload = sns_diag_island_tx_log_size;
    void *encoded_log_internal = NULL;
    size_t bytes_written_internal = 0;
    sns_diag_island_log header = sns_diag_island_log_init_default;

    // encoding the oneof_payload sns_diag_island_log_payloads
    encoded_log_internal = sns_malloc(SNS_DIAG_HEAP_TYPE, sizeof_oneof_payload);

    rc1 = qsh_island_log_transition_encode_payload(
        log, log_size, sizeof_oneof_payload, encoded_log_internal,
        &bytes_written_internal);
    // encoding the oneof_payload sns_diag_island_log_payloads

    if(rc1 != SNS_RC_SUCCESS)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "Error encoding encoded_log_internal");
    }

    // Getting the stream
    pb_ostream_t stream = pb_ostream_from_buffer(encoded_log, encoded_log_size);
    // Getting the stream

    // encoding the header
    header.has_timestamp = true;
    header.timestamp = ((sns_diag_island_log *)log)->timestamp;
    if(!pb_encode(&stream, sns_diag_island_log_fields, &header))
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "Error encoding sns_diag_island_log header");
    }
    // encoding the header

    else
    {
      // encoding the tag
      if(!pb_encode_tag(&stream, PB_WT_STRING,
                        sns_diag_island_log_log_payload_tag))
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "Error encoding sns_diag_island_log tag");
      }
      // encoding the tag

      // encoding the oneof_payload_as_string
      else if(!pb_encode_string(&stream, (pb_byte_t *)encoded_log_internal,
                                bytes_written_internal))
      {
        SNS_SPRINTF(
            ERROR, sns_fw_printf,
            "Error encoding sns_diag_island_log oneof payload as string %s",
            PB_GET_ERROR(&stream));
      }
      // encoding the oneof_payload_as_string

      // writing the bytes_written
      else
      {
        *bytes_written = stream.bytes_written;
      }
    }
    sns_free(encoded_log_internal);
    encoded_log_internal = NULL;
  }

  return rc;
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static void
sns_island_log_transition(sns_island_state current_state, uint64_t timestamp,
                          bool has_user_cookie, uint64_t user_cookie,
                          bool submit_asynchronously)
{
  sns_diag_async_log_item async_log;
  sns_diag_island_log *island_log;

  _Static_assert(sizeof(sns_diag_island_log) < sizeof(async_log.payload),
                 "sizeof(sns_diag_island_log)< sizeof(async_log.payload)");

  if(submit_asynchronously)
  {
    island_log = (sns_diag_island_log *)&async_log.payload[0];
  }
  else
  {
    island_log = sns_diag_log_alloc(sizeof(sns_diag_island_log),
                                    SNS_LOG_ISLAND_TRANSITION);
  }

  if(NULL != island_log)
  {
    island_log->has_timestamp = true;
    island_log->which_sns_diag_island_log_payloads =
        sns_diag_island_log_log_payload_tag;
    island_log->sns_diag_island_log_payloads.log_payload.has_island_state =
        true;
    if(SNS_ISLAND_STATE_IN_ISLAND == current_state)
    {
      island_log->sns_diag_island_log_payloads.log_payload.island_state =
          SNS_DIAG_ISLAND_STATE_IN_ISLAND_MODE;
      island_log->timestamp = qsh_island_log.island_entry_ts;
    }
    else if(SNS_ISLAND_STATE_NOT_IN_ISLAND == current_state)
    {
      island_log->sns_diag_island_log_payloads.log_payload.island_state =
          SNS_DIAG_ISLAND_STATE_NOT_IN_ISLAND_MODE;
      island_log->timestamp = qsh_island_log.island_exit_ts;
    }
    else
    {
      island_log->sns_diag_island_log_payloads.log_payload.island_state =
          SNS_DIAG_ISLAND_STATE_ISLAND_DISABLED;
      island_log->timestamp = timestamp;
    }
    if(true == has_user_cookie)
    {
      island_log->timestamp = timestamp;
    }
    island_log->sns_diag_island_log_payloads.log_payload.has_cookie =
        has_user_cookie;
    island_log->sns_diag_island_log_payloads.log_payload.cookie = user_cookie;
    island_log->sns_diag_island_log_payloads.log_payload.has_total_island_time =
        true;
    island_log->sns_diag_island_log_payloads.log_payload.total_island_time =
        qsh_island_log.total_island_us;

    if(submit_asynchronously)
    {
      async_log.encoded_size = qsh_island.island_log_size;
      async_log.log_id = SNS_LOG_ISLAND_TRANSITION;
      async_log.payload_size = sizeof(sns_diag_island_log);
      async_log.payload_encode_cb = qsh_island_log_transition_encode_cb;
      async_log.timestamp = sns_get_system_time();

      sns_diag_submit_async_log(async_log);
    }
    else
    {
      // First consume all the previously recorded transitions
      sns_diag_consume_async_logs(true);
      qsh_island_transition_logging(true);

      // publish this log
      sns_diag_publish_fw_log(
          SNS_LOG_ISLAND_TRANSITION, sizeof(sns_diag_island_log), island_log,
          qsh_island.island_log_size, qsh_island_log_transition_encode_cb);
    }
  }
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE void
qsh_island_log_notification_cb(sns_island_notification_cb_state state)
{
  if(SNS_ISLAND_STATE_ENTER == state)
  {
    qsh_island_log.current_state = SNS_ISLAND_STATE_IN_ISLAND;
    qsh_island_log.island_entry_ts = sns_get_system_time();
    qsh_island_log.island_entry_count++;
  }
  else
  {
    qsh_island_log.current_state = SNS_ISLAND_STATE_NOT_IN_ISLAND;
    qsh_island_log.island_exit_ts = sns_get_system_time();
    qsh_island_log.island_exit_count++;

    uint64_t island_time_ticks = 0;
    uint64_t island_time_us = 0;

    island_time_ticks =
        (qsh_island_log.island_exit_ts - qsh_island_log.island_entry_ts);

    island_time_us =
        (sns_get_time_tick_resolution_in_ps() * island_time_ticks) /
        (1000000ULL);

    qsh_island_log.total_island_us += island_time_us;

    if(0 != qsh_island_log.island_entry_ts)
    {
      sns_island_log_transition(SNS_ISLAND_STATE_IN_ISLAND,
                                qsh_island_log.island_entry_ts, false, 0, true);
    }
    sns_island_log_transition(SNS_ISLAND_STATE_NOT_IN_ISLAND,
                              qsh_island_log.island_exit_ts, false, 0, true);
  }
}

/*============================================================================
  public Functions
  ============================================================================*/

void qsh_island_log_use_case_activated(void)
{
  qsh_island_log.uSleep_handle = sns_island_notification_cb_client_register(
      qsh_island_log_notification_cb, SNS_ISLAND_CLIENT_QSH_LOG);
}
/*----------------------------------------------------------------------------*/

void qsh_island_log_use_case_deactivated(void)
{
  sns_island_notification_cb_client_deregister(qsh_island_log.uSleep_handle,
                                               SNS_ISLAND_CLIENT_QSH_LOG);
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc sns_island_generate_and_commit_state_log(uint64_t req_id)
{
  sns_island_log_transition(qsh_island_log.current_state, sns_get_system_time(),
                            true, req_id, false);

  if(0 != req_id)
  {
    // island validation is valid only for non-zero cookie.
    qsh_island_test_validate(req_id, qsh_island_log.total_island_us);
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
SNS_ISLAND_CODE static sns_rc qsh_island_dbg_log_transition_encode_payload(
    void *log, size_t log_size, size_t encoded_log_size, void *encoded_log,
    size_t *bytes_written)
{
  UNUSED_VAR(log_size);
  sns_rc rc = SNS_RC_SUCCESS;
  if(NULL == log || 0 == log_size || NULL == encoded_log ||
     0 == encoded_log_size || NULL == bytes_written)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
    void *log_payload;
    pb_ostream_t stream = pb_ostream_from_buffer(encoded_log, encoded_log_size);

    sns_diag_island_debug_log *log_payload_internal =
        &(((sns_diag_island_log *)log)
              ->sns_diag_island_log_payloads.dbg_log_payload);
    log_payload = (void *)log_payload_internal;

    if(!pb_encode(&stream, sns_diag_island_debug_log_fields, log_payload))
    {
      SNS_SPRINTF(ERROR, sns_fw_printf,
                  "Error encoding encoded_log_internal : %s",
                  PB_GET_ERROR(&stream));
      rc = SNS_RC_FAILED;
    }

    if(SNS_RC_SUCCESS == rc)
    {
      *bytes_written = stream.bytes_written;
    }
  }

  return rc;
}

SNS_ISLAND_CODE static sns_rc qsh_island_dbg_log_transition_encode_cb(
    void *log, size_t log_size, size_t encoded_log_size, void *encoded_log,
    size_t *bytes_written)
{
  UNUSED_VAR(log_size);
  sns_rc rc = SNS_RC_SUCCESS;
  sns_rc rc1 = SNS_RC_SUCCESS;
  if(NULL == log || 0 == log_size || NULL == encoded_log ||
     0 == encoded_log_size || NULL == bytes_written)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
    qsh_island_transition_debug_state *log_info =
        (qsh_island_transition_debug_state *)log;
    sns_diag_island_log dbg_log = sns_diag_island_log_init_default;
    uint8_t arr_index = 0;
    uint32_t sizeof_oneof_payload = 0;
    void *encoded_log_internal = NULL;
    size_t bytes_written_internal = 0;
    sns_diag_island_log header = sns_diag_island_log_init_default;

    // get the info from debug_state and populate in actual dbg_log
    dbg_log.has_timestamp = true;
    dbg_log.timestamp = log_info->island_transition_ts;
    dbg_log.which_sns_diag_island_log_payloads =
        sns_diag_island_log_dbg_log_payload_tag;
    dbg_log.sns_diag_island_log_payloads.dbg_log_payload
        .has_previous_island_state = true;
    dbg_log.sns_diag_island_log_payloads.dbg_log_payload.previous_island_state =
        log_info->island_transition_states[0];
    dbg_log.sns_diag_island_log_payloads.dbg_log_payload
        .has_current_island_state = true;
    dbg_log.sns_diag_island_log_payloads.dbg_log_payload.current_island_state =
        log_info->island_transition_states[1];

    dbg_log.sns_diag_island_log_payloads.dbg_log_payload.callers.funcs.encode =
        &pb_encode_float_arr_cb;
    void *callers[3] = {log_info->caller0, log_info->caller1,
                        log_info->caller2};
    pb_float_arr_arg arg = {.arr = (float *)callers,
                            .arr_len = ARR_SIZE(callers),
                            .arr_index = &arr_index};
    dbg_log.sns_diag_island_log_payloads.dbg_log_payload.callers.arg = &arg;
    // get the info from debug_state and populate in actual dbg_log

    // get the info about which oneof payload is used. Use this info to get the
    // info used in the rest of the function (like encoded payload size etc)
    sizeof_oneof_payload = qsh_island.dbg_log_payload_size;
    // get the info about which oneof payload is used

    // encoding the oneof_payload sns_diag_island_log_payloads
    encoded_log_internal = sns_malloc(SNS_DIAG_HEAP_TYPE, sizeof_oneof_payload);
    if(encoded_log_internal == NULL)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "encoded_log_internal is NULL");
    }

    rc1 = qsh_island_dbg_log_transition_encode_payload(
        &dbg_log, sizeof(sns_diag_island_log), sizeof_oneof_payload,
        encoded_log_internal, &bytes_written_internal);
    // encoding the oneof_payload sns_diag_island_log_payloads

    if(rc1 != SNS_RC_SUCCESS)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "Error encoding encoded_log_internal");
    }

    // Getting the stream
    pb_ostream_t stream =
        pb_ostream_from_buffer(encoded_log, qsh_island.island_log_size);
    // Getting the stream

    // encoding the header
    header.has_timestamp = true;
    header.timestamp = dbg_log.timestamp;
    if(!pb_encode(&stream, sns_diag_island_log_fields, &header))
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "Error encoding sns_diag_island_log header");
    }
    // encoding the header

    else
    {
      // encoding the tag
      if(!pb_encode_tag(&stream, PB_WT_STRING,
                        sns_diag_island_log_dbg_log_payload_tag))
      {
        SNS_PRINTF(ERROR, sns_fw_printf,
                   "Error encoding sns_diag_island_log tag");
      }
      // encoding the tag

      // encoding the oneof_payload_as_string
      else if(!pb_encode_string(&stream, (pb_byte_t *)encoded_log_internal,
                                bytes_written_internal))
      {
        SNS_SPRINTF(
            ERROR, sns_fw_printf,
            "Error encoding sns_diag_island_log oneof payload as string %s",
            PB_GET_ERROR(&stream));
      }
      // encoding the oneof_payload_as_string

      // writing the bytes_written
      else
      {
        *bytes_written = stream.bytes_written;
      }
    }
    sns_free(encoded_log_internal);
    encoded_log_internal = NULL;
  }

  return rc;
}

SNS_ISLAND_CODE
void qsh_island_transition_logging(bool force)
{
  if(qsh_island.qsh_debug.enable_island_debug == true)
  {
    bool done = false;
    bool consume_logs =
        (force) ||
        (qsh_island.qsh_debug.island_debug->island_transition_pending_count >=
         ISLAND_TRANSITION_LOGGING_THRESHOLD);
    uint8_t write_index;
    unsigned int read_index;
    qsh_island_transition_debug_state *debug_ptr =
        qsh_island.qsh_debug.island_debug->island_transition_debug;

    sns_osa_lock_acquire(
        &qsh_island.qsh_debug.island_debug->island_transition_write_index_lock);
    write_index =
        qsh_island.qsh_debug.island_debug->island_transition_write_index;
    read_index =
        qsh_island.qsh_debug.island_debug->island_transition_read_index;
    sns_osa_lock_release(
        &qsh_island.qsh_debug.island_debug->island_transition_write_index_lock);

    do
    {
      qsh_island_transition_debug_state *debug_info =
          (qsh_island_transition_debug_state *)sns_diag_log_alloc(
              sizeof(qsh_island_transition_debug_state),
              SNS_LOG_ISLAND_TRANSITION);

      if(NULL != debug_info)
      {
        bool process_log = false;
        if((read_index != write_index) && consume_logs)
        {
          debug_info->island_transition_ts =
              debug_ptr[read_index].island_transition_ts;
          debug_info->caller0 = debug_ptr[read_index].caller0;
          debug_info->caller1 = debug_ptr[read_index].caller1;
          debug_info->caller2 = debug_ptr[read_index].caller2;
          debug_info->island_transition_states[0] =
              debug_ptr[read_index].island_transition_states[0];
          debug_info->island_transition_states[1] =
              debug_ptr[read_index].island_transition_states[1];

          read_index = (read_index + 1) % DEBUG_ARRAY_LENGTH;

          if(qsh_island.qsh_debug.island_debug
                 ->island_transition_pending_count > 0)
          {
            atomic_fetch_sub(&(qsh_island.qsh_debug.island_debug
                                   ->island_transition_pending_count),
                             1);
          }

          process_log = true;
          done = false;
        }
        else
        {
          done = true;
        }

        if(process_log)
        {
          sns_diag_publish_fw_log(SNS_LOG_ISLAND_TRANSITION,
                                  sizeof(qsh_island_transition_debug_state),
                                  debug_info, qsh_island.island_log_size,
                                  qsh_island_dbg_log_transition_encode_cb);
        }
        else
        {
          sns_diag_log_info *temp = sns_diag_get_log_info((void *)debug_info);
          sns_diag_log_free(temp);
        }
      }
      else
      {
        break;
      }
    } while(!done);

    sns_osa_lock_acquire(
        &qsh_island.qsh_debug.island_debug->island_transition_write_index_lock);
    qsh_island.qsh_debug.island_debug->island_transition_read_index =
        read_index;
    sns_osa_lock_release(
        &qsh_island.qsh_debug.island_debug->island_transition_write_index_lock);
  }
}
/*----------------------------------------------------------------------------*/