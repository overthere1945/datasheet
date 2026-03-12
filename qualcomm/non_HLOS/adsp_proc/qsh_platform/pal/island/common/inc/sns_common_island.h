#pragma once
/** ============================================================================
 *  @file
 *
 *  @brief Common island utility APIs
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.\n 
 *  All rights reserved.\n 
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*=============================================================================
  Includes
  ===========================================================================*/

#include "sns_isafe_list.h"
#include "sns_sensor.h"
#include "sns_time.h"

/*============================================================================
  Macros
  ============================================================================*/

#define ISLAND_CODE   SNS_SECTION(".text.SNS")
#define ISLAND_DATA   SNS_SECTION(".data.SNS")
#define ISLAND_RODATA SNS_SECTION(".rodata.SNS")

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Structure for maintaining island vote details
 */
typedef struct
{
  /* Last enable vote */
  sns_time last_enable_ts;

  /* Last disable vote */
  sns_time last_disable_ts;

  /* Client handle for island vote */
  struct sns_island_vote_handle *client_handle;

  /* Last island vote*/
  _Atomic unsigned int last_vote;
} sns_island_vote;

/**
 * @brief Structure for maintaining island transition statistics
 */
typedef struct
{
  /* Timestamp of last entry into island mode*/
  sns_time island_entry_timestamp;

  /* Timestamp of last exit out of island mode*/
  sns_time island_exit_timestamp;

  /*Total time(us) spent in island since boot*/
  uint64_t total_island_time;

  /*count from userpd_init*/
  uint32_t island_entry_count;

  /*count from userpd_init*/
  uint32_t island_exit_count;
} sns_island_transition_stats;

/**
 * @brief Structure for an item in the active sensors list for a specific island
 * memory pool
 */
typedef struct
{
  sns_isafe_list_item list_entry;
  sns_sensor *sensor;
} sns_island_active_sensor_item;

/*=============================================================================
  Functions
  ===========================================================================*/

/**
 * @brief The API to get an active sensor item from the specified active sensor
 * list for an island memory pool
 *
 * @param[in] sensor               Target sensor for search
 * @param[in] active_list          Active sensors list for an island memory pool
 *
 * @return                         Non-NULL pointer if sensor item is found.
 *                                 NULL otherwise
 */
sns_island_active_sensor_item *
sns_island_get_active_sensor(sns_sensor *const sensor,
                             sns_isafe_list *active_list);
