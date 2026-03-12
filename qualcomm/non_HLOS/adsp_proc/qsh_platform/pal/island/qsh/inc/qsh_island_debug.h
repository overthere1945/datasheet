#pragma once
/*=============================================================================
  @file qsh_island_debug.h

  Header file for the island debug utility

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#include <stdbool.h>
#include "sns_osa_lock.h"
#include "sns_time.h"

/*============================================================================
  Macros
  ============================================================================*/

#define DEBUG_ARRAY_LENGTH 50

// This threshold is compared against island_transition_pending_count
// to determine if sufficient number of entries in the array
// 'island_transition_debug' is collected to be logged
#define ISLAND_TRANSITION_LOGGING_THRESHOLD 40

/*============================================================================
  Types
  ============================================================================*/

typedef struct
{
  /* Time stamp of island state update */
  sns_time island_transition_ts;

  /* Store 3 levels of caller pointers */
  void *caller0;
  void *caller1;
  void *caller2;

  /* Initial and final state after island state update */
  unsigned int island_transition_states[2];

} qsh_island_transition_debug_state;

typedef struct
{
  /* Circular buffer for island transition debug */
  qsh_island_transition_debug_state island_transition_debug[DEBUG_ARRAY_LENGTH];

  /* Index used to write into island transition debug circular buffer */
  uint8_t island_transition_write_index;

  /* Lock to protect update of island_transition_write_index */
  sns_osa_lock island_transition_write_index_lock;

  /* Index used to read from island transition debug circular buffer for logging
   */
  _Atomic unsigned int island_transition_read_index;

  /* Pending count used with island transition debug circular buffer for logging
   */
  _Atomic unsigned int island_transition_pending_count;
} sns_island_debug_state;

typedef struct
{
  bool enable_island_debug;
  sns_island_debug_state *island_debug;
} sns_island_transition_debug;

/*============================================================================
  Functions
  ============================================================================*/
/**
 * This function initialize island debug structure
 *
 * @param[i] debug      island debug structure pointer
 */
void qsh_enable_island_debug(sns_island_transition_debug *debug);

/**
 * This function logs island tranistionsfor debug
 *
 * @param[i] *debug         island debug structure pointer
 * @param[i] initial state  island intial state
 * @param[i] current state  island current state
 *
 */
void qsh_island_transiton_debug(sns_island_transition_debug *debug,
                                unsigned int initial_state,
                                unsigned int current_state);

/* ========================================================================*/
