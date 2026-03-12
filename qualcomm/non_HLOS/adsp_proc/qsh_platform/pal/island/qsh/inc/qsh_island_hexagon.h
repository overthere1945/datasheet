#pragma once
/*=============================================================================
  @file qsh_island_hexagon.h

  Header file for the island hexagon utility

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#include "pb_encode.h"
#include "qsh_island_debug.h"
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

/*============================================================================
  Types
  ============================================================================*/

typedef enum
{
  ISLAND_ENTER_CALLBACK = 0,
  ISLAND_EXIT_CALLBACK,
  ISLAND_ENTRY_BLOCKED,
  ISLAND_ENTRY_UNBLOCKED,
  NO_ACTIVE_THREADS,
  ISLAND_EXIT_CALLED
} sns_island_state_transition_triggers;

// System state.
#define SET_IN_ISLAND(state)     atomic_fetch_or(&state, 0x04)
#define SET_NOT_IN_ISLAND(state) atomic_fetch_and(&state, 0x03)
#define IS_IN_ISLAND(state)      (state & 0x04)

// SEE state
#define SET_ISLAND_UNBLOCKED(state) atomic_fetch_or(&state, 0x02)
#define SET_ISLAND_BLOCKED(state)   atomic_fetch_and(&state, 0x05)
#define IS_UNBLOCKED(state)         (state & 0x02)

// SEE vote
#define SET_ISLAND_ENABLED(state)  atomic_fetch_or(&state, 0x01)
#define SET_ISLAND_DISABLED(state) atomic_fetch_and(&state, 0x06)
#define IS_ENABLED(state)          (state & 0x01)

typedef enum
{
  NOT_IN_ISLAND_BLOCKED_DISABLED = 0x00,                   // 0000 0000
  NOT_IN_ISLAND_UNBLOCKED_DISABLED = 0x02,                 // 0000 0010
  NOT_IN_ISLAND_UNBLOCKED_ENABLED = 0x03,                  // 0000 0011
  IN_ISLAND_UNBLOCKED_ENABLED = 0x07,                      // 0000 0111
  TRANSITION_FROM_NOT_IN_ISLAND_BLOCKED_DISABLED = 0x10,   // 0001 0000
  TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_DISABLED = 0x12, // 0001 0010
  TRANSITION_FROM_NOT_IN_ISLAND_UNBLOCKED_ENABLED = 0x13,  // 0001 0011
  TRANSITION_FROM_IN_ISLAND_UNBLOCKED_ENABLED = 0x17       // 0001 0111
} sns_island_valid_island_states;

typedef struct
{
  sns_isafe_list_item list_entry_island_block;
  sns_isafe_list_item list_entry_client;
  char client_name[16];
  sns_osa_lock lock;
} sns_island_aggregator_client;

typedef struct
{
  /* Current island state */
  _Atomic unsigned int current_state;

  /* Mutex protecting current_state */
  sns_osa_lock island_state_lock;

  /* Island blocks */
  sns_isafe_list island_blocks;

  sns_island_vote qsh_vote;

  sns_island_vote qsh_sensors_vote;

  sns_island_transition_stats qsh_stats;

  /*Island transition debug structure */
  sns_island_transition_debug qsh_debug;

  /*List of active island sensors */
  sns_isafe_list active_sensors_list;

  /*List of registered aggregator clients */
  sns_isafe_list client_list;

  /* island transition log size calculated at bootup*/
  size_t island_log_size;

  /* debug island transition log size calculated at bootup*/
  size_t dbg_log_payload_size;
} qsh_fw_island_state;
