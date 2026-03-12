/*=============================================================================
  @file sns_island_stub.c

  @brief
  Island Stub API interface.

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ===========================================================================*/

/*=============================================================================
  INCLUDES
  ===========================================================================*/

#include "sns_island.h"
#include "sns_island_service.h"
#include "sns_island_util.h"
#include "sns_sensor_instance.h"
#include "sns_sensor_uid.h"
#include "sns_types.h"

/*=============================================================================
  Extern Declarations
  ===========================================================================*/

extern uint32_t __sensors_island_start;
extern uint32_t __sensors_island_end;

/*=============================================================================
  Globals
  ===========================================================================*/

uintptr_t sns_island_size = 0;

/*=============================================================================
  Public Functions
  ===========================================================================*/
sns_rc qsh_island_init(void)
{
  // DO NOTHING
  return SNS_RC_SUCCESS;
}

void sns_island_configure_island_transition_debug(bool enable_island_debug)
{
  UNUSED_VAR(enable_island_debug);
}

sns_rc sns_island_exit_internal(void)
{
  return SNS_RC_NOT_SUPPORTED;
}
sns_island_state sns_island_get_island_state(void)
{
  return SNS_ISLAND_STATE_ISLAND_DISABLED;
}

sns_island_client_handle
sns_island_aggregator_register_client(const char *client_name)
{
  UNUSED_VAR(client_name);
  return NULL;
}

void sns_island_aggregator_deregister_client(
    sns_island_client_handle client_handle)
{
  UNUSED_VAR(client_handle);
}

sns_rc sns_island_block(sns_island_client_handle client_handle)
{
  UNUSED_VAR(client_handle);
  return SNS_RC_NOT_SUPPORTED;
}

sns_rc sns_island_unblock(sns_island_client_handle client_handle)
{
  UNUSED_VAR(client_handle);
  return SNS_RC_NOT_SUPPORTED;
}

bool qsh_island_allow(_Atomic unsigned int *active_threads_count)
{
  // DO NOTHING
  UNUSED_VAR(active_threads_count);
  return false;
}

sns_rc qsh_island_memory_vote(sns_sensor *const sensor, bool enable)
{
  // DO NOTHING
  UNUSED_VAR(sensor);
  UNUSED_VAR(enable);
  return SNS_RC_SUCCESS;
}

void sns_island_configure_island_test_debug(bool enable_island_test)
{
  // DO NOTHING
  UNUSED_VAR(enable_island_test);
}

sns_rc sns_island_generate_and_commit_state_log(uint64_t req_id)
{
  UNUSED_VAR(req_id);
  return SNS_RC_NOT_SUPPORTED;
}

sns_rc sns_island_log_trace(uint64_t user_defined_id)
{
  UNUSED_VAR(user_defined_id);
  return SNS_RC_NOT_SUPPORTED;
}

void qsh_island_transition_logging(bool force)
{
  UNUSED_VAR(force);
}
