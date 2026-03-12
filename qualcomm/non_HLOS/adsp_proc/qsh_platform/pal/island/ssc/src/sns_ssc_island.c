/** ============================================================================
 * @file
 *
 * @brief Island control for Sensors.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/
#ifdef SNS_ISLAND_REMOVE_PROXY_VOTE
#include "island_user.h"
#endif

#include "pb_encode.h"
#include "sns_assert.h"
#include "sns_common_island_core_interface.h"
#include "sns_common_island.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_fw_diag_service.h"
#include "sns_fw_sensor_instance.h"
#include "sns_fw_sensor.h"
#include "sns_isafe_list.h"
#include "sns_island_util.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_pwr_sleep_mgr.h"
#include "sns_ssc_island.h"
#include "sns_thread_manager.h"
#include "sns_time.h"
#include "sns_types.h"

#include <stdatomic.h>

/*============================================================================
  Macros
  ============================================================================*/

/*============================================================================
  Globals
  ============================================================================*/

static sns_fw_island_state sns_island SNS_ISLAND_DATA;

/*============================================================================
  Static Functions
  ============================================================================*/

static bool sns_island_restrict_internal(void)
{
  sns_osa_lock_acquire(&sns_island.island_state_lock);
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  sns_ssc_island_vote expected = SNS_ISLAND_ENABLE;
  if(atomic_compare_exchange_strong(&sns_island.sns_vote.last_vote,
                                    (unsigned int *)&expected,
                                    SNS_ISLAND_DISABLE))
  {
    sns_island_vote_request(sns_island.sns_vote.client_handle,
                            SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_SSC);
    sns_island.sns_vote.last_disable_ts = now;
    ret_val = true;
  }
  sns_osa_lock_release(&sns_island.island_state_lock);
  return ret_val;
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static bool sns_island_allow_internal(void)
{
  sns_osa_lock_acquire(&sns_island.island_state_lock);
  bool ret_val = false;
  sns_time now = sns_get_system_time();
  sns_ssc_island_vote expected_SNS_ISLAND_DISABLE = SNS_ISLAND_DISABLE;
  if(atomic_compare_exchange_strong(
         &sns_island.sns_vote.last_vote,
         (unsigned int *)&expected_SNS_ISLAND_DISABLE, SNS_ISLAND_ENABLE))
  {
    sns_island_vote_request(sns_island.sns_vote.client_handle,
                            SNS_ISLAND_VOTE_ENABLE, SNS_ISLAND_CLIENT_SSC);
    sns_island.sns_vote.last_enable_ts = now;
    ret_val = true;
  }
  sns_osa_lock_release(&sns_island.island_state_lock);
  return ret_val;
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE void
sns_island_state_notification_cb(sns_island_notification_cb_state state)
{
  if(SNS_ISLAND_STATE_ENTER == state)
  {
    sns_island.current_state = SNS_ISLAND_STATE_IN_ISLAND;
    sns_island.sns_stats.island_entry_timestamp = sns_get_system_time();
    sns_island.sns_stats.island_entry_count++;
  }
  else
  {
    uint64_t time_spent_us = 0;
    sns_island.current_state = SNS_ISLAND_STATE_NOT_IN_ISLAND;
    sns_island.sns_stats.island_exit_timestamp = sns_get_system_time();
    sns_island.sns_stats.island_exit_count++;
    time_spent_us = (sns_get_time_tick_resolution_in_ps() *
                     (sns_island.sns_stats.island_exit_timestamp -
                      sns_island.sns_stats.island_entry_timestamp)) /
                    (1000000ULL);
    sns_island.sns_stats.total_island_time += time_spent_us;
  }
}

/*----------------------------------------------------------------------------*/

/**
 * This function removes the proxy voting done by island manager to
 * restrict sensors and sensors+audio island spec
 */
SNS_ISLAND_CODE static void sns_island_remove_island_mgr_proxy_voting(void)
{
#ifdef SNS_ISLAND_REMOVE_PROXY_VOTE
  // remove proxy voting from island_mgr
  island_mgr_remove_proxy(USLEEP_ISLAND_SNS);
#endif
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static sns_island_active_sensor_item *
search_sensor_in_active_sensors_list(sns_sensor *const sensor)
{
  sns_island_active_sensor_item *search_item = NULL;
  sns_osa_lock_acquire(&sns_island.island_state_lock);
  search_item =
      sns_island_get_active_sensor(sensor, &sns_island.active_sensors_list);
  sns_osa_lock_release(&sns_island.island_state_lock);
  return search_item;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static bool sns_island_allow(void)
{
  bool ret_val = false;
  if(true == sns_island.is_sensors_usecase_active)
  {
    ret_val = sns_island_allow_internal();
  }
  return ret_val;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE static bool sns_island_restrict(void)
{
  SNS_ISLAND_EXIT();
  bool ret_val = sns_island_restrict_internal();
  return ret_val;
}

/*============================================================================
  Public Functions
  ============================================================================*/

SNS_ISLAND_CODE sns_rc sns_island_init(void)
{
  sns_time now = sns_get_system_time();

  sns_island_notification_cb_handle *uSleep_handle =
      sns_island_notification_cb_client_register(
          sns_island_state_notification_cb, SNS_ISLAND_CLIENT_SSC);
  SNS_ASSERT(uSleep_handle != NULL);

  sns_osa_lock_attr attr;
  sns_osa_lock_attr_init(&attr);
  sns_osa_lock_init(&attr, &sns_island.island_state_lock);

  sns_island.sns_vote.client_handle =
      sns_island_vote_client_register("SNS_ISLAND_uSleep");
  SNS_ASSERT(NULL != sns_island.sns_vote.client_handle);

  sns_island.sns_stats.island_entry_count = 0;
  sns_island.sns_stats.island_exit_count = 0;
  sns_island.sns_stats.total_island_time = 0;
  sns_island.sns_stats.island_entry_timestamp = 0;
  sns_island.sns_stats.island_exit_timestamp = now;

  sns_island_vote_request(sns_island.sns_vote.client_handle,
                          SNS_ISLAND_VOTE_DISABLE, SNS_ISLAND_CLIENT_SSC);
  sns_island.sns_vote.last_vote = SNS_ISLAND_DISABLE;
  sns_island.sns_vote.last_disable_ts = now;
  sns_island.current_state = SNS_ISLAND_STATE_NOT_IN_ISLAND;
  sns_island.is_sensors_usecase_active = false;

  sns_island_remove_island_mgr_proxy_voting();

  sns_isafe_list_init(&sns_island.active_sensors_list);

  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc ssc_island_memory_vote(sns_sensor *const sensor,
                                              bool enable)
{
  // For Testing
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
      // add sensor to the active sensors list.
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
        sns_osa_lock_acquire(&sns_island.island_state_lock);

        // insert sensor item to the active sensor list
        sns_isafe_list_iter_init(&iter, &sns_island.active_sensors_list,
                                 is_island_item);
        sns_isafe_list_item_init(&sensor_item->list_entry, sensor_item);
        sns_isafe_list_iter_insert(&iter, &sensor_item->list_entry,
                                   !is_island_item);

        // update sensor entry
        sensor_item->sensor = sensor;

        sns_osa_lock_release(&sns_island.island_state_lock);
      }
    }
  }
  else
  {
    // if sensor found, remove sensor from the list
    if(NULL != sensor_item)
    {
      sns_osa_lock_acquire(&sns_island.island_state_lock);
      if(sns_isafe_list_item_present(&sensor_item->list_entry))
      {
        sns_isafe_list_item_remove(&sensor_item->list_entry);
        free_sensor_item = true;
      }
      sns_osa_lock_release(&sns_island.island_state_lock);
    }
  }
  sns_osa_lock_acquire(&sns_island.island_state_lock);
  // Vote for ssc memory pool if there is atleast one ssc island sensor active
  if(sns_island.active_sensors_list.cnt > 0)
  {
    // add ssc island vote
    if(false == sns_island.is_sensors_usecase_active)
    {
      SNS_ISLAND_EXIT();
      sns_island.is_sensors_usecase_active = true;
      sns_island_allow();
    }
  }
  else
  {
    if(true == sns_island.is_sensors_usecase_active)
    {
      SNS_ISLAND_EXIT();
      // remove ssc island vote
      sns_island.is_sensors_usecase_active = false;
      sns_island_restrict();
    }
  }
  sns_osa_lock_release(&sns_island.island_state_lock);

  if(true == free_sensor_item)
  {
    sns_free(sensor_item);
  }
  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE bool is_ssc_island_usecase_active(void)
{
  return sns_island.is_sensors_usecase_active;
}

/*----------------------------------------------------------------------------*/
