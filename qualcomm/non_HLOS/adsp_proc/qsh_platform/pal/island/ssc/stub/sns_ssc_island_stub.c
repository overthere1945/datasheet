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

/*=============================================================================
  Globals
  ===========================================================================*/

/*=============================================================================
  Static Data
  ===========================================================================*/

/*=============================================================================
  Public Functions
  ===========================================================================*/

sns_rc sns_island_init(void)
{
  return SNS_RC_SUCCESS;
}

sns_rc ssc_island_memory_vote(sns_sensor *const sensor, bool enable)
{
  UNUSED_VAR(sensor);
  UNUSED_VAR(enable);
  return SNS_RC_SUCCESS;
}

bool is_ssc_island_usecase_active(void)
{
  return false;
}
