/** ============================================================================
 * @file sns_qshtech_island.c
 *
 * @brief Qshtech island vote utility.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#include "sns_common_island.h"
#include "sns_common_island_core_interface.h"
#include "sns_assert.h"
#include "sns_diag.pb.h"
#include "sns_diag_service.h"
#include "sns_island.h"
#include "sns_island_config.h"
#include "sns_island_util.h"
#include "sns_fw_diag_service.h"
#include "sns_isafe_list.h"
#include "sns_island_service.h"
#include "sns_island_util.h"
#include "sns_memmgr.h"
#include "sns_qshtech_island.h"
#include "sns_qshtech_island_internal.h"
#include "sns_time.h"
#include "sns_types.h"

#include <stdatomic.h>

/*============================================================================
  Macros
  ============================================================================*/

/*============================================================================
  typedefs
  ============================================================================*/

/**
 * Island mode vote
 */
typedef enum sns_qshtech_island_vote
{
  QSHTECH_ISLAND_DISABLE = 0,
  QSHTECH_ISLAND_ENABLE = 1,

} sns_qshtech_island_vote;

/**
 * Island mode states
 */
typedef enum sns_qshtech_island_state
{
  QSHTECH_ISLAND_STATE_IN_ISLAND = 0,
  QSHTECH_ISLAND_STATE_NOT_IN_ISLAND = 1

} sns_qshtech_island_state;

typedef struct sns_qshtech_active_sensor_item
{
  sns_sensor *sensor;
  sns_isafe_list_item list_entry;

} sns_qshtech_active_sensor_item;

typedef struct sns_qshtech_island_npa_vote
{
  sns_time last_enable_ts;  // Last enable vote
  sns_time last_disable_ts; // Last disable vote
  sns_island_vote_handle
      *client_handle;             // Client handle for QSH provided by the NPA
  _Atomic unsigned int last_vote; // Last island vote

} sns_qshtech_island_npa_vote;

typedef struct sns_qshtech_island_stats
{

  sns_time island_entry_timestamp; // Timestamp of last entry into island mode
  sns_time island_exit_timestamp;  // Timestamp of last exit out of island mode
  uint64_t total_island_time;      // Total time(us) spent in island since boot
  uint32_t island_entry_count;     // count from userpd_init
  uint32_t island_exit_count;      // count from userpd_init

} sns_qshtech_island_stats;

typedef struct sns_qshtech_state
{
  sns_qshtech_island_npa_vote vote;
  sns_qshtech_island_stats stats;
  sns_qshtech_island_state current_state;
  sns_isafe_list active_sensors_list;
  sns_osa_lock lock;
  bool is_qshtech_sensor_active;

} sns_qshtech_state;

/*=============================================================================
  Static data
  ===========================================================================*/

static sns_qshtech_state qshtech_island;

/*============================================================================
  Static Functions
  ============================================================================*/

void sns_qshtech_island_state_notification_cb(
    sns_island_notification_cb_state state)
{
  if(SNS_ISLAND_STATE_ENTER == state)
  {
    qshtech_island.current_state = QSHTECH_ISLAND_STATE_IN_ISLAND;
    qshtech_island.stats.island_entry_timestamp = sns_get_system_time();
    qshtech_island.stats.island_entry_count++;
  }
  else
  {
    uint64_t time_spent_us = 0;
    qshtech_island.current_state = QSHTECH_ISLAND_STATE_NOT_IN_ISLAND;
    qshtech_island.stats.island_exit_timestamp = sns_get_system_time();
    qshtech_island.stats.island_exit_count++;
    time_spent_us = (sns_get_time_tick_resolution_in_ps() *
                     (qshtech_island.stats.island_exit_timestamp -
                      qshtech_island.stats.island_entry_timestamp)) /
                    (1000000ULL);
    qshtech_island.stats.total_island_time += time_spent_us;
  }
}
/*----------------------------------------------------------------------------*/
static bool qshtech_island_allow(void)
{
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  sns_qshtech_island_vote expected = QSHTECH_ISLAND_DISABLE;
  if(atomic_compare_exchange_strong(&qshtech_island.vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSHTECH_ISLAND_ENABLE))
  {
    sns_island_vote_request(qshtech_island.vote.client_handle,
                            SNS_ISLAND_VOTE_ENABLE, SNS_ISLAND_CLIENT_QSHTECH);
    qshtech_island.vote.last_enable_ts = now;
    ret_val = true;
  }

  return ret_val;
}
/*----------------------------------------------------------------------------*/
static bool qshtech_island_restrict(void)
{
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  sns_qshtech_island_vote expected = QSHTECH_ISLAND_ENABLE;
  if(atomic_compare_exchange_strong(&qshtech_island.vote.last_vote,
                                    (unsigned int *)&expected,
                                    QSHTECH_ISLAND_DISABLE))
  {
    sns_island_vote_request(qshtech_island.vote.client_handle,
                            SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_QSHTECH);

    qshtech_island.vote.last_disable_ts = now;
    ret_val = true;
  }
  return ret_val;
}

/*----------------------------------------------------------------------------*/
static sns_island_active_sensor_item *
search_sensor_in_active_sensors_list(sns_sensor *const sensor)
{
  sns_island_active_sensor_item *search_item = NULL;
  sns_osa_lock_acquire(&qshtech_island.lock);
  search_item =
      sns_island_get_active_sensor(sensor, &qshtech_island.active_sensors_list);
  sns_osa_lock_release(&qshtech_island.lock);
  return search_item;
}
/*============================================================================
  Public Functions
  ============================================================================*/
sns_rc sns_qshtech_island_init(void)
{
  sns_time now = sns_get_system_time();

  sns_island_notification_cb_handle *uSleep_handle =
      sns_island_notification_cb_client_register(
          sns_qshtech_island_state_notification_cb, SNS_ISLAND_CLIENT_QSHTECH);
  SNS_ASSERT(uSleep_handle != NULL);

  sns_osa_lock_attr attr;
  sns_osa_lock_attr_init(&attr);
  sns_osa_lock_init(&attr, &qshtech_island.lock);

  qshtech_island.vote.client_handle =
      sns_island_vote_client_register("QSHTECH_ISLAND_uSleep");
  SNS_ASSERT(NULL != qshtech_island.vote.client_handle);

  qshtech_island.stats.island_entry_count = 0;
  qshtech_island.stats.island_exit_count = 0;
  qshtech_island.stats.total_island_time = 0;
  qshtech_island.stats.island_entry_timestamp = 0;
  qshtech_island.stats.island_exit_timestamp = now;

  sns_island_vote_request(qshtech_island.vote.client_handle,
                          SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_QSHTECH);

  qshtech_island.vote.last_vote = QSHTECH_ISLAND_DISABLE;
  qshtech_island.vote.last_disable_ts = now;
  qshtech_island.current_state = QSHTECH_ISLAND_STATE_NOT_IN_ISLAND;
  qshtech_island.is_qshtech_sensor_active = false;

  sns_isafe_list_init(&qshtech_island.active_sensors_list);

  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/

sns_rc sns_qshtech_island_memory_vote(sns_sensor *const sensor, bool enable)
{
  sns_isafe_list_iter iter;
  sns_island_active_sensor_item *sensor_item = NULL;
  bool free_sensor_item = false;

  // search sensor in active sensors list
  sensor_item = search_sensor_in_active_sensors_list(sensor);

  if(true == enable)
  {
    if(NULL == sensor_item)
    {
      // allocate memory for sensor item
      sensor_item =
          sns_malloc(SNS_HEAP_MAIN, sizeof(sns_island_active_sensor_item));

      if(NULL != sensor_item)
      {
        sns_osa_lock_acquire(&qshtech_island.lock);

        // insert sensor item to the active sensor list
        sns_isafe_list_iter_init(&iter, &qshtech_island.active_sensors_list,
                                 false);
        sns_isafe_list_item_init(&sensor_item->list_entry, sensor_item);
        sns_isafe_list_iter_insert(&iter, &sensor_item->list_entry, true);

        // update sensor entry
        sensor_item->sensor = sensor;
        sns_osa_lock_release(&qshtech_island.lock);
      }
    }
  }
  else
  {
    // if sensor found, remove sensor from the list
    if(NULL != sensor_item)
    {
      sns_osa_lock_acquire(&qshtech_island.lock);
      if(sns_isafe_list_item_present(&sensor_item->list_entry))
      {
        sns_isafe_list_item_remove(&sensor_item->list_entry);
        free_sensor_item = true;
      }
      sns_osa_lock_release(&qshtech_island.lock);
    }
  }
  sns_osa_lock_acquire(&qshtech_island.lock);
  // Vote for qshtech island if there is atleast one qshtech island sensor
  // active
  if(qshtech_island.active_sensors_list.cnt > 0)
  {
    // add qshtech island vote
    if(false == qshtech_island.is_qshtech_sensor_active)
    {
      qshtech_island.is_qshtech_sensor_active = true;
      qshtech_island_allow();
    }
  }
  else
  {
    if(true == qshtech_island.is_qshtech_sensor_active)
    {
      // remove qshtech island vote
      qshtech_island.is_qshtech_sensor_active = false;
      qshtech_island_restrict();
    }
  }
  sns_osa_lock_release(&qshtech_island.lock);

  if(true == free_sensor_item)
  {
    sns_free(sensor_item);
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
