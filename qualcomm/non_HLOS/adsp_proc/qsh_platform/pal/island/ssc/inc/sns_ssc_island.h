#pragma once
/*=============================================================================
  @file

  Header file for the island hexagon utility

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#include "pb_encode.h"
#include "sns_common_island_core_interface.h"
#include "sns_common_island.h"
#include "sns_isafe_list.h"
#include "sns_island_service.h"
#include "sns_island.h"
#include "sns_osa_lock.h"
#include "sns_time.h"
#include "sns_types.h"

/*============================================================================
  Macros
  ============================================================================*/

#define SNS_ISLAND_CODE   SNS_SECTION(".text.SNS")
#define SNS_ISLAND_DATA   SNS_SECTION(".data.SNS")
#define SNS_ISLAND_RODATA SNS_SECTION(".rodata.SNS")

#define SNS_ISLAND_EXIT_NOTIFY_ARR_SIZE    5
#define SNS_ISLAND_TRANSITION_LOG_ARR_SIZE 5
#define SNS_ISLAND_MAX_BLOCKED_THREADS     15

/*============================================================================
  Functions
  ============================================================================*/

/* This function counts the number of island exits for SNS island. This is used
 * for internal island validation
 */
void sns_island_exit_count_update(void);

/*============================================================================
  Types
  ============================================================================*/

typedef struct
{
  sns_island_vote sns_vote;

  sns_island_transition_stats sns_stats;

  /* Island service pointer */
  sns_island_service service;

  /* Mutex protecting access to this state */
  sns_osa_lock island_state_lock;

  sns_island_state current_state;

  bool is_sensors_usecase_active;

  /*List of active ssc island sensors */
  sns_isafe_list active_sensors_list;

} sns_fw_island_state;
