/** ============================================================================
 * @file qsh_island_hexagon.c
 *
 * @brief Island control for Sensors.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/
#include <stdatomic.h>

#include "sns_common_island.h"
#include "sns_common_island_core_interface.h"
#include "pb_encode.h"
#include "qsh_island_hexagon.h"
#include "qsh_island_log.h"
#include "qsh_island_test.h"
#include "sns_assert.h"
#include "sns_diag.pb.h"
#include "sns_diag_service.h"
#include "sns_fw_diag_service.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_math_ops.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_pb_util.h"
#include "sns_printf_int.h"
#include "sns_time.h"
#include "sns_types.h"

/*============================================================================
  Macros
  ============================================================================*/

/*============================================================================
  Globals
  ============================================================================*/
/* Current OSA active threads count */
static _Atomic unsigned int *osa_active_threads_count SNS_ISLAND_DATA;
qsh_fw_island_state qsh_island SNS_ISLAND_DATA;

// External defined in sensors.lcs.template file
extern uint32_t __sensors_island_start;
extern uint32_t __sensors_island_end;
uintptr_t sns_island_size SNS_ISLAND_DATA = 0;

// Mappings from the 2018 island states to the legacy island states
const sns_island_state map_to_legacy_states[] SNS_ISLAND_RODATA = {
    SNS_ISLAND_STATE_ISLAND_DISABLED, // map_to_legacy_states[NOT_IN_ISLAND_BLOCKED_DISABLED]
    SNS_ISLAND_STATE_ISLAND_DISABLED, // Invalid state
                                      // NOT_IN_ISLAND_BLOCKED_ENABLED
    SNS_ISLAND_STATE_NOT_IN_ISLAND, // map_to_legacy_states[NOT_IN_ISLAND_UNBLOCKED_DISABLED]
    SNS_ISLAND_STATE_NOT_IN_ISLAND, // map_to_legacy_states[NOT_IN_ISLAND_UNBLOCKED_ENABLED]
    SNS_ISLAND_STATE_ISLAND_DISABLED, // Invalid state
                                      // IN_ISLAND_BLOCKED_DISABLED
    SNS_ISLAND_STATE_ISLAND_DISABLED, // Invalid state IN_ISLAND_BLOCKED_ENABLED
    SNS_ISLAND_STATE_ISLAND_DISABLED, // Invalid state
                                      // IN_ISLAND_UNBLOCKED_DISABLED
    SNS_ISLAND_STATE_IN_ISLAND // map_to_legacy_states[IN_ISLAND_UNBLOCKED_ENABLED]
};

/*============================================================================
  Static Functions
  ============================================================================*/

static bool qsh_island_restrict_silent(void)
{
  bool ret_val = false;
  sns_time now = sns_get_system_time();

  qsh_island_vote expected = QSH_ISLAND_ENABLE;
  if(atomic_compare_exchange_strong(&qsh_island.qsh_vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSH_ISLAND_DISABLE))
  {
    sns_island_vote_request(qsh_island.qsh_vote.client_handle,
                            SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_QSH_FW);
    qsh_island.qsh_vote.last_disable_ts = now;
    ret_val = true;
  }
  return ret_val;
}

/*----------------------------------------------------------------------------*/
static bool qsh_island_allow_silent(void)
{
  bool ret_val = false;
  sns_time now = sns_get_system_time();

  qsh_island_vote expected = QSH_ISLAND_DISABLE;
  if(atomic_compare_exchange_strong(&qsh_island.qsh_vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSH_ISLAND_ENABLE))
  {
    sns_island_vote_request(qsh_island.qsh_vote.client_handle,
                            SNS_ISLAND_VOTE_ENABLE, SNS_ISLAND_CLIENT_QSH_FW);
    qsh_island.qsh_vote.last_enable_ts = now;
    ret_val = true;
  }
  return ret_val;
}

/*----------------------------------------------------------------------------*/

//  qsh_island_state_transition
//
// Updates the qsh_island manager's internal state provided a triggering event.
// See the state machine below for more details.
//
// @param[i] this    pointer to the qsh_island manager
// @param[i] trigger Event that should cause a state change
//
// @return  true if the trigger produced a state change
//          false otherwise
//
//                                                      SYSTEM_STATE, SEE_STATE, QSH_VOTE
//
//  sns_island_init()
//      |
//      |         +-------------------------------------------------------------------------------------------------------------------------+
//      |         |                                                                                                                         |
//      |         |    +----------------------------------------------------+ISLAND_EXIT_CALLED                                             |
//      |         |    |                                                    |                                                               |
//      |         |    |                               +--------------------+----------------------+                                        |
//      |         |    |   +---------------------------> Not in Island, Island unblocked, Allow    +------------------------------+         |
//      |         |    |   |                           +----------------------+--------^-----------+ ISLAND_ENTER_CALLBACK        |         |ISLAND_EXIT_CALLED
//      |         |    |   |                                                  |        |                                          |         |
//      |         |    |   | NO_ACTIVE_THREADS                                |        |                                          |         |
//      |         |    |   |                                                  |        |                       +------------------v---------+---------+
//    +-v---------v----v---+-----------------------+                          |        +-----------------------+ In Island, Island unblocked, Allow   |
//    | Not in Island, Island unblocked,  Restrict +-----------------+        |        ISLAND_EXIT_CALLBACK    +------------------+-------------------+
//    +--------------------^-----------------------+                 |        |                                                   |
//                         |                                         |        |                                                   |
//                         |                    ISLAND_ENTRY_BLOCKED |        |ISLAND_ENTRY_BLOCKED                               |ISLAND_ENTRY_BLOCKED
//                         | ISLAND_ENTRY_UNBLOCKED                  |        |                                                   |
//                         |                           +-------------v--------v------------------+                                |
//                         +---------------------------+ Not in Island, Island blocked, Restrict <--------------------------------+
//                                                     +-----------------------------------------+
//
//
//  Invalid states:
//
//  In Island, Island unblocked, Restrict
//  In Island, Island blocked, Allow
//  In Island, Island blocked, Restrict
//  Not in Island, Island blocked, Allow
//
//
SNS_ISLAND_CODE static bool
qsh_island_state_transition(qsh_fw_island_state *this,
                            sns_island_state_transition_triggers trigger)
{
  unsigned int initial_state = this->current_state;
  sns_island_valid_island_states expected_NOT_IN_ISLAND_UNBLOCKED_ENABLED =
      NOT_IN_ISLAND_UNBLOCKED_ENABLED;
  sns_island_valid_island_states expected_IN_ISLAND_UNBLOCKED_ENABLED =
      IN_ISLAND_UNBLOCKED_ENABLED;
  sns_island_valid_island_states expected_NOT_IN_ISLAND_UNBLOCKED_DISABLED =
      NOT_IN_ISLAND_UNBLOCKED_DISABLED;
  sns_island_valid_island_states expected_NOT_IN_ISLAND_BLOCKED_DISABLED =
      NOT_IN_ISLAND_BLOCKED_DISABLED;
  sns_island_valid_island_states
      expected_TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED =
          TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED;
  switch(trigger)
  {
  case ISLAND_ENTER_CALLBACK:
  {
    atomic_compare_exchange_strong(
        &this->current_state,
        (unsigned int *)&expected_NOT_IN_ISLAND_UNBLOCKED_ENABLED,
        IN_ISLAND_UNBLOCKED_ENABLED);
    break;
  }
  case ISLAND_EXIT_CALLBACK:
  {
    atomic_compare_exchange_strong(
        &this->current_state,
        (unsigned int *)&expected_IN_ISLAND_UNBLOCKED_ENABLED,
        NOT_IN_ISLAND_UNBLOCKED_ENABLED);
    break;
  }
  case ISLAND_ENTRY_BLOCKED:
  {
    if(atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_IN_ISLAND_UNBLOCKED_ENABLED,
           TRANSITION_FROM_IN_ISLAND_UNBLOCKED_ENABLED) ||
       atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_NOT_IN_ISLAND_UNBLOCKED_ENABLED,
           TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_ENABLED) ||
       atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_NOT_IN_ISLAND_UNBLOCKED_DISABLED,
           TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED))
    {
      if(IS_IN_ISLAND(this->current_state))
      {
        sns_island_exit();
      }
      if(IS_ENABLED(this->current_state))
      {
        qsh_island_restrict_silent();
      }
      atomic_store(&this->current_state, NOT_IN_ISLAND_BLOCKED_DISABLED);
    }
    break;
  }
  case ISLAND_ENTRY_UNBLOCKED:
  {
    atomic_compare_exchange_strong(
        &this->current_state,
        (unsigned int *)&expected_NOT_IN_ISLAND_BLOCKED_DISABLED,
        NOT_IN_ISLAND_UNBLOCKED_DISABLED);
    break;
  }
  case NO_ACTIVE_THREADS:
  {
    if(atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_NOT_IN_ISLAND_UNBLOCKED_DISABLED,
           TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED))
    {
      if(!(*osa_active_threads_count) && qsh_island_allow_silent())
      {
        atomic_store(&this->current_state, NOT_IN_ISLAND_UNBLOCKED_ENABLED);
      }
      else
      {
        atomic_store(&this->current_state, NOT_IN_ISLAND_UNBLOCKED_DISABLED);
      }
    }
    break;
  }
  case ISLAND_EXIT_CALLED:
  {
    if(atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_IN_ISLAND_UNBLOCKED_ENABLED,
           TRANSITION_FROM_IN_ISLAND_UNBLOCKED_ENABLED) ||
       atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int *)&expected_NOT_IN_ISLAND_UNBLOCKED_ENABLED,
           TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_ENABLED) ||
       atomic_compare_exchange_strong(
           &this->current_state,
           (unsigned int
                *)&expected_TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED,
           TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED))
    {
      if(IS_IN_ISLAND(this->current_state))
      {
        sns_island_exit();
      }
      qsh_island_restrict_silent();
      atomic_store(&this->current_state, NOT_IN_ISLAND_UNBLOCKED_DISABLED);
    }
    else if(IS_IN_ISLAND(this->current_state))
    {
      // Another thread is currently busy pulling the system out of island mode.
      // Do not return till we are certain that we are out of island mode
      sns_island_exit();
    }
    break;
  }
  default:
    SNS_ASSERT(0);
  }
  qsh_island_transiton_debug(&qsh_island.qsh_debug, initial_state,
                             this->current_state);

  if((initial_state != this->current_state) && IS_IN_ISLAND(initial_state))
  {
    qsh_island_test_update_island_exit_count();
  }
  return (initial_state != this->current_state);
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE void
qsh_island_state_notification_cb(sns_island_notification_cb_state state)
{
  // Update internal island state
  if(SNS_ISLAND_STATE_ENTER == state)
  {
    qsh_island.qsh_stats.island_entry_timestamp = sns_get_system_time();
    qsh_island.qsh_stats.island_entry_count++;
    qsh_island_state_transition(&qsh_island, ISLAND_ENTER_CALLBACK);
  }
  else
  {
    uint64_t time_spent_us;
    qsh_island.qsh_stats.island_exit_timestamp = sns_get_system_time();
    qsh_island.qsh_stats.island_exit_count++;

    time_spent_us = (sns_get_time_tick_resolution_in_ps() *
                     (qsh_island.qsh_stats.island_exit_timestamp -
                      qsh_island.qsh_stats.island_entry_timestamp)) /
                    (1000000ULL);
    qsh_island.qsh_stats.total_island_time += time_spent_us;
    qsh_island_state_transition(&qsh_island, ISLAND_EXIT_CALLBACK);
  }
}

/*----------------------------------------------------------------------------*/

static sns_island_aggregator_client *
search_client_in_registered_clients_list(const char *client_name)
{
  sns_isafe_list_iter iter;
  sns_island_aggregator_client *found_client = NULL;

  // find client from the registered aggregator client list
  for(sns_isafe_list_iter_init(&iter, &qsh_island.client_list, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_island_aggregator_client *curr_client =
        (sns_island_aggregator_client *)sns_isafe_list_iter_get_curr_data(
            &iter);

    if(0 == strcmp(curr_client->client_name, client_name))
    {
      found_client = curr_client;
      break;
    }
  }
  return found_client;
}

static bool qsh_sensors_island_restrict(void)
{
  sns_osa_lock_acquire(&qsh_island.island_state_lock);
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  qsh_island_vote expected = QSH_ISLAND_ENABLE;

  if(atomic_compare_exchange_strong(&qsh_island.qsh_sensors_vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSH_ISLAND_DISABLE))
  {
    sns_island_vote_request(qsh_island.qsh_sensors_vote.client_handle,
                            SNS_ISLAND_VOTE_DISABLE,
                            SNS_ISLAND_CLIENT_QSH_SENSORS);
    qsh_island.qsh_sensors_vote.last_disable_ts = now;
    ret_val = true;
  }
  sns_osa_lock_release(&qsh_island.island_state_lock);
  return ret_val;
}
/*----------------------------------------------------------------------------*/

static bool qsh_sensors_island_allow(void)
{
  sns_osa_lock_acquire(&qsh_island.island_state_lock);
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  qsh_island_vote expected = QSH_ISLAND_DISABLE;

  if(atomic_compare_exchange_strong(&qsh_island.qsh_sensors_vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSH_ISLAND_ENABLE))
  {
    sns_island_vote_request(qsh_island.qsh_sensors_vote.client_handle,
                            SNS_ISLAND_VOTE_ENABLE,
                            SNS_ISLAND_CLIENT_QSH_SENSORS);
    qsh_island.qsh_sensors_vote.last_enable_ts = now;
    ret_val = true;
  }
  sns_osa_lock_release(&qsh_island.island_state_lock);
  return ret_val;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static sns_island_active_sensor_item *
search_sensor_in_active_sensors_list(sns_sensor *const sensor)
{
  sns_island_active_sensor_item *search_item = NULL;
  sns_osa_lock_acquire(&qsh_island.island_state_lock);
  search_item =
      sns_island_get_active_sensor(sensor, &qsh_island.active_sensors_list);
  sns_osa_lock_release(&qsh_island.island_state_lock);
  return search_item;
}

/*============================================================================
  Public Functions
  ============================================================================*/

SNS_ISLAND_CODE
sns_rc qsh_island_init(void)
{
  sns_time now = sns_get_system_time();
  osa_active_threads_count = NULL;

  sns_diag_island_log island_tx_logs1 = sns_diag_island_log_init_default;
  size_t size1;
  sns_diag_island_log island_tx_logs2 = sns_diag_island_log_init_default;
  uint8_t arr_index2 = 0;
  float callers2[3] = {0xABCD, 0xABCD, 0xABCD};
  pb_float_arr_arg arg2 = {.arr = (float *)callers2,
                           .arr_len = ARR_SIZE(callers2),
                           .arr_index = &arr_index2};
  size_t size2;
  sns_diag_island_debug_log island_tx_logs3 =
      sns_diag_island_debug_log_init_default;
  uint8_t arr_index3 = 0;
  float callers3[3] = {0xABCD, 0xABCD, 0xABCD};
  pb_float_arr_arg arg3 = {.arr = (float *)callers3,
                           .arr_len = ARR_SIZE(callers3),
                           .arr_index = &arr_index3};
  size_t size3;

  qsh_island.qsh_vote.client_handle =
      sns_island_vote_client_register("QSH_ISLAND_uSleep");
  SNS_ASSERT(NULL != qsh_island.qsh_vote.client_handle);

  qsh_island.qsh_sensors_vote.client_handle =
      sns_island_vote_client_register("QSH_SENSOR_ISLAND_uSleep");
  SNS_ASSERT(NULL != qsh_island.qsh_sensors_vote.client_handle);

  qsh_island.current_state = NOT_IN_ISLAND_UNBLOCKED_DISABLED;
  qsh_island.qsh_stats.island_entry_timestamp = 0;
  qsh_island.qsh_stats.island_exit_timestamp = now;
  qsh_island.qsh_stats.total_island_time = 0;
  sns_island_size =
      (uintptr_t)&__sensors_island_end - (uintptr_t)&__sensors_island_start;

  sns_island_notification_cb_handle *uSleep_handle =
      sns_island_notification_cb_client_register(
          qsh_island_state_notification_cb, SNS_ISLAND_CLIENT_QSH_FW);
  SNS_ASSERT(uSleep_handle != NULL);

  sns_osa_lock_attr attr;
  sns_osa_lock_attr_init(&attr);
  sns_osa_lock_init(&attr, &qsh_island.island_state_lock);
  sns_isafe_list_init(&qsh_island.island_blocks);

  qsh_island.qsh_vote.last_vote = QSH_ISLAND_DISABLE;

  sns_island_vote_request(qsh_island.qsh_vote.client_handle,
                          SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_QSH_FW);

  qsh_island.qsh_vote.last_disable_ts = now;

  qsh_island.qsh_stats.island_entry_timestamp = 0;
  qsh_island.qsh_stats.island_exit_timestamp = now;

  qsh_island.qsh_debug.island_debug = NULL;
  qsh_island.qsh_debug.enable_island_debug = false;

  qsh_island.qsh_sensors_vote.last_vote = QSH_ISLAND_DISABLE;
  sns_island_vote_request(qsh_island.qsh_sensors_vote.client_handle,
                          SNS_ISLAND_VOTE_DISABLE,
                          SNS_ISLAND_CLIENT_QSH_SENSORS);
  qsh_island.qsh_sensors_vote.last_disable_ts = now;

  sns_isafe_list_init(&qsh_island.active_sensors_list);
  sns_isafe_list_init(&qsh_island.client_list);

  island_tx_logs1.has_timestamp = true;
  island_tx_logs1.timestamp = sns_get_system_time();
  island_tx_logs1.which_sns_diag_island_log_payloads =
      sns_diag_island_log_log_payload_tag;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload.has_island_state =
      true;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload.island_state = 1;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload.has_cookie = true;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload.cookie = 0xABCD;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload
      .has_total_island_time = true;
  island_tx_logs1.sns_diag_island_log_payloads.log_payload.total_island_time =
      0xABCD;
  pb_get_encoded_size(&size1, sns_diag_island_log_fields, &island_tx_logs1);

  island_tx_logs2.has_timestamp = true;
  island_tx_logs2.timestamp = sns_get_system_time();
  island_tx_logs2.which_sns_diag_island_log_payloads =
      sns_diag_island_log_dbg_log_payload_tag;
  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload
      .has_previous_island_state = true;
  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload
      .previous_island_state = 0xABCD;
  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload
      .has_current_island_state = true;
  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload
      .current_island_state = 0xABCD;

  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload.callers.funcs
      .encode = &pb_encode_float_arr_cb;
  island_tx_logs2.sns_diag_island_log_payloads.dbg_log_payload.callers.arg =
      &arg2;

  pb_get_encoded_size(&size2, sns_diag_island_log_fields, &island_tx_logs2);

  if(size1 >= size2)
  {
    qsh_island.island_log_size = size1;
  }
  else
  {
    qsh_island.island_log_size = size2;
  }

  island_tx_logs3.has_previous_island_state = true;
  island_tx_logs3.previous_island_state = 0xABCD;
  island_tx_logs3.has_current_island_state = true;
  island_tx_logs3.current_island_state = 0xABCD;

  island_tx_logs3.callers.funcs.encode = &pb_encode_float_arr_cb;
  island_tx_logs3.callers.arg = &arg3;

  pb_get_encoded_size(&size3, sns_diag_island_debug_log_fields,
                      &island_tx_logs3);
  qsh_island.dbg_log_payload_size = size3;

  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
SNS_ISLAND_CODE void
sns_island_configure_island_transition_debug(bool enable_island_debug)
{
  if(true == enable_island_debug)
  {
    qsh_enable_island_debug(&qsh_island.qsh_debug);
  }
}
/*----------------------------------------------------------------------------*/
SNS_ISLAND_CODE sns_rc sns_island_exit_internal(void)
{
  // Skip qsh_island state transition,
  // if qsh_island is blocked or qsh_island is disabled.
  if(!((qsh_island.current_state == NOT_IN_ISLAND_BLOCKED_DISABLED) ||
       (qsh_island.current_state == NOT_IN_ISLAND_UNBLOCKED_DISABLED)))
  {
    sns_osa_lock_acquire(&qsh_island.island_state_lock);
    sns_island_valid_island_states initial_state = qsh_island.current_state;

    qsh_island_state_transition(&qsh_island, ISLAND_EXIT_CALLED);
    sns_osa_lock_release(&qsh_island.island_state_lock);

    if(IS_IN_ISLAND(initial_state))
    {
      SNS_PRINTF(HIGH, sns_fw_printf, "Island exit %p %p %p",
                 (void *)__builtin_return_address(1),
                 (void *)__builtin_return_address(2),
                 (void *)__builtin_return_address(3));
      return SNS_RC_SUCCESS;
    }
  }
  return SNS_RC_NOT_AVAILABLE;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_island_state sns_island_get_island_state(void)
{
  unsigned int qsh_island_current_state = atomic_load(&qsh_island.current_state);
  return map_to_legacy_states[0x07 & qsh_island_current_state];
}
/*----------------------------------------------------------------------------*/

sns_island_client_handle
sns_island_aggregator_register_client(const char *client_name)
{
  sns_island_aggregator_client *client = NULL;
  sns_isafe_list_iter iter;
  sns_osa_lock_attr attr;

  client = search_client_in_registered_clients_list(client_name);

  if(NULL == client)
  {
    client = sns_malloc_island_or_main(sizeof(sns_island_aggregator_client));
    SNS_ASSERT(NULL != client);
    sns_osa_lock_attr_init(&attr);

    sns_strlcpy(client->client_name, client_name, sizeof(client->client_name));
    sns_isafe_list_item_init(&client->list_entry_island_block, client);

    sns_isafe_list_item_init(&client->list_entry_client, client);
    sns_isafe_list_iter_init(&iter, &qsh_island.client_list, true);
    sns_isafe_list_iter_insert(&iter, &client->list_entry_client, false);
    sns_osa_lock_init(&attr, &client->lock);
  }

  return (sns_island_client_handle)client;
}
/*----------------------------------------------------------------------------*/

void sns_island_aggregator_deregister_client(
    sns_island_client_handle client_handle)
{
  sns_island_aggregator_client *client =
      (sns_island_aggregator_client *)client_handle;
  if(NULL != client)
  {
    sns_island_unblock(
        client); // Ensure the client is no longer blocking island entry
    sns_osa_lock_deinit(&client->lock);
    sns_isafe_list_item_remove(&client->list_entry_client);
    sns_free(client);
  }
}

SNS_ISLAND_CODE sns_rc sns_island_block(sns_island_client_handle client_handle)
{
  sns_island_aggregator_client *client =
      (sns_island_aggregator_client *)client_handle;
  sns_rc ret_val = SNS_RC_INVALID_TYPE;
  if(NULL != client)
  {
    SNS_ISLAND_EXIT();
    sns_osa_lock_acquire(&client->lock);
    if(NULL == client->list_entry_island_block.list)
    {
      sns_isafe_list_iter iter;
      sns_osa_lock_acquire(&qsh_island.island_state_lock);
      sns_isafe_list_iter_init(&iter, &qsh_island.island_blocks, true);
      sns_isafe_list_iter_insert(&iter, &client->list_entry_island_block, true);
      qsh_island_state_transition(&qsh_island, ISLAND_ENTRY_BLOCKED);
      sns_osa_lock_release(&qsh_island.island_state_lock);
    }

#ifdef SNS_ISLAND_ENABLE_DEBUG_PRINTS
    SNS_SPRINTF(LOW, sns_fw_printf, "Client %s has blocked, Final blocks: %d",
                client->client_name, qsh_island.island_blocks.cnt);
#endif

    // SNS_PRINTF(LOW, sns_fw_printf, "Client " SNS_DIAG_PTR " has blocked,
    // Final blocks: %d",
    //           client, qsh_island.island_blocks.cnt);
    ret_val = SNS_RC_SUCCESS;
    sns_osa_lock_release(&client->lock);
  }
  return ret_val;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc
sns_island_unblock(sns_island_client_handle client_handle)
{
  sns_rc ret_val = SNS_RC_INVALID_TYPE;

  sns_island_aggregator_client *client =
      (sns_island_aggregator_client *)client_handle;
  if(NULL != client)
  {
    sns_osa_lock_acquire(&client->lock);
    if(&qsh_island.island_blocks == client->list_entry_island_block.list)
    {
      bool island_reenabled = false;
      sns_osa_lock_acquire(&qsh_island.island_state_lock);
      sns_isafe_list_item_remove(&client->list_entry_island_block);
      island_reenabled = (0 == qsh_island.island_blocks.cnt);
      if(island_reenabled)
      {
        qsh_island_state_transition(&qsh_island, ISLAND_ENTRY_UNBLOCKED);
      }
      sns_osa_lock_release(&qsh_island.island_state_lock);

#ifdef SNS_ISLAND_ENABLE_DEBUG_PRINTS
      SNS_SPRINTF(LOW, sns_fw_printf,
                  "Client %s has unblocked, Final blocks: %d",
                  client->client_name, qsh_island.island_blocks.cnt);
#endif

      // SNS_PRINTF(LOW, sns_fw_printf, "Client " SNS_DIAG_PTR " has unblocked,
      // Final blocks: %d",
      //             client, qsh_island.island_blocks.cnt);
      ret_val = SNS_RC_SUCCESS;
    }
    else if(NULL != client->list_entry_island_block.list)
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "Island aggregator client belongs to unrecognized list");
      ret_val = SNS_RC_SUCCESS;
    }
    sns_osa_lock_release(&client->lock);
  }
  return ret_val;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE bool
qsh_island_allow(_Atomic unsigned int *active_threads_count)
{
  bool ret_val = false;

  sns_osa_lock_acquire(&qsh_island.island_state_lock);
  osa_active_threads_count = active_threads_count;
  if(0 == qsh_island.island_blocks.cnt)
  {
    ret_val = qsh_island_state_transition(&qsh_island, NO_ACTIVE_THREADS);
  }
  sns_osa_lock_release(&qsh_island.island_state_lock);
  return ret_val;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc qsh_island_memory_vote(sns_sensor *const sensor,
                                              bool enable)
{
  sns_isafe_list_iter iter;
  sns_island_active_sensor_item *sensor_item = NULL;
  bool free_sensor_item = false;
  bool is_island_item = true;

  // search sensor in active sensors list
  sensor_item = search_sensor_in_active_sensors_list(sensor);

  if(true == enable)
  {
    if(NULL == sensor_item)
    {
      // allocate memory for sensor item
      sensor_item =
          sns_malloc(SNS_HEAP_ISLAND, sizeof(sns_island_active_sensor_item));
      if(NULL == sensor_item)
      {
        SNS_ISLAND_EXIT();
        sensor_item =
            sns_malloc(SNS_HEAP_MAIN, sizeof(sns_island_active_sensor_item));
        is_island_item = false;
      }
      if(NULL != sensor_item)
      {
        sns_osa_lock_acquire(&qsh_island.island_state_lock);

        // insert sensor item to the active sensor list
        sns_isafe_list_iter_init(&iter, &qsh_island.active_sensors_list,
                                 is_island_item);
        sns_isafe_list_item_init(&sensor_item->list_entry, sensor_item);
        sns_isafe_list_iter_insert(&iter, &sensor_item->list_entry,
                                   !is_island_item);

        sensor_item->sensor = sensor;
        sns_osa_lock_release(&qsh_island.island_state_lock);
      }
    }
  }
  else
  {
    // remove sensor from the active sensor list
    if(NULL != sensor_item)
    {
      sns_osa_lock_acquire(&qsh_island.island_state_lock);
      if(sns_isafe_list_item_present(&sensor_item->list_entry))
      {
        sns_isafe_list_item_remove(&sensor_item->list_entry);
        free_sensor_item = true;
      }
      sns_osa_lock_release(&qsh_island.island_state_lock);
    }
  }
  sns_osa_lock_acquire(&qsh_island.island_state_lock);
  // vote for qsh island pool if there is atleast one island sensor active
  // remove qsh island vote if there are no island sensors active.
  if(qsh_island.active_sensors_list.cnt > 0)
  {
    if(QSH_ISLAND_DISABLE == qsh_island.qsh_sensors_vote.last_vote)
    {
      SNS_ISLAND_EXIT();
      qsh_sensors_island_allow();
      qsh_island_log_use_case_activated();
    }
  }
  else
  {
    if(QSH_ISLAND_ENABLE == qsh_island.qsh_sensors_vote.last_vote)
    {
      SNS_ISLAND_EXIT();
      qsh_sensors_island_restrict();
      qsh_island_log_use_case_deactivated();
    }
  }
  sns_osa_lock_release(&qsh_island.island_state_lock);
  if(true == free_sensor_item)
  {
    sns_free(sensor_item);
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
