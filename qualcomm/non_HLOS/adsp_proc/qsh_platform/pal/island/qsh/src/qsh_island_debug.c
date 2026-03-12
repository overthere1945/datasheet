/*============================================================================
  @file qsh_island_debug.c

  @brief
  Island debug utility for Sensors.

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/
#include <stdatomic.h>

#include "qsh_island_debug.h"
#include "qsh_island_hexagon.h"
#include "sns_assert.h"
#include "sns_fw_diag_service.h"
#include "sns_fw_diag_types.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_printf.h"
#include "sns_printf_int.h"
#include "sns_types.h"

#if defined(SSC_TARGET_X86)
#include <stdio.h>
#endif

/*============================================================================
  Macros
  ============================================================================*/

/*============================================================================
  Globals
  ============================================================================*/

/*============================================================================
  Static Functions
  ============================================================================*/

/*============================================================================
  Public Functions
  ============================================================================*/

SNS_SECTION(".text.SNS")
void qsh_enable_island_debug(sns_island_transition_debug *debug)
{
  if(NULL == debug->island_debug)
  {
    debug->island_debug =
        sns_malloc(SNS_HEAP_ISLAND, sizeof(sns_island_debug_state));

    if(NULL != debug->island_debug)
    {
      sns_osa_lock_attr attr;
      sns_osa_lock_attr_init(&attr);
      sns_osa_lock_init(
          &attr, &debug->island_debug->island_transition_write_index_lock);
      debug->enable_island_debug = true;
      debug->island_debug->island_transition_write_index = 0;
      debug->island_debug->island_transition_read_index = 0;
      debug->island_debug->island_transition_pending_count = 0;
    }
  }
}

SNS_SECTION(".text.SNS")
void qsh_island_transiton_debug(sns_island_transition_debug *debug,
                                unsigned int initial_state,
                                unsigned int current_state)
{
  if(NULL != debug->island_debug && (true == debug->enable_island_debug) &&
     (initial_state != current_state))
  {
    uint32_t write_idx = 0;
    qsh_island_transition_debug_state *debug_ptr = NULL;

    sns_osa_lock_acquire(
        &debug->island_debug->island_transition_write_index_lock);
    write_idx = debug->island_debug->island_transition_write_index;
    debug->island_debug->island_transition_write_index =
        (debug->island_debug->island_transition_write_index + 1) %
        DEBUG_ARRAY_LENGTH;
    if(debug->island_debug->island_transition_read_index ==
       debug->island_debug->island_transition_write_index)
    {
      debug->island_debug->island_transition_read_index =
          (debug->island_debug->island_transition_read_index + 1) %
          DEBUG_ARRAY_LENGTH;
    }
    sns_osa_lock_release(
        &debug->island_debug->island_transition_write_index_lock);

    debug_ptr = debug->island_debug->island_transition_debug;

    debug_ptr[write_idx].island_transition_states[0] = initial_state;
    debug_ptr[write_idx].island_transition_states[1] = current_state;
    debug_ptr[write_idx].island_transition_ts = sns_get_system_time();
    debug_ptr[write_idx].caller0 = (void *)__builtin_return_address(1);
    debug_ptr[write_idx].caller1 = (void *)__builtin_return_address(2);
    debug_ptr[write_idx].caller2 = (void *)__builtin_return_address(3);

    if(sns_diag_get_log_status(SNS_LOG_ISLAND_TRANSITION))
    {
      atomic_fetch_add(&(debug->island_debug->island_transition_pending_count),
                       1);
    }

    if(!IS_IN_ISLAND(current_state) &&
       current_state != NOT_IN_ISLAND_UNBLOCKED_ENABLED)
    {
      if((debug->island_debug->island_transition_pending_count >=
          ISLAND_TRANSITION_LOGGING_THRESHOLD))
      {
        sns_diag_consume_island_transition_debug_logs();
      }
    }
  }
}
