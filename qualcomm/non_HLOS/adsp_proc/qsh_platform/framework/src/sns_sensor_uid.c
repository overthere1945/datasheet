/*=============================================================================
  @file sns_sensor_uid.c

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include <stdbool.h>
#include "sns_island.h"
#include "sns_mem_util.h"
#include "sns_sensor_uid.h"

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

SNS_SECTION(".text.sns")
bool sns_sensor_uid_compare(sns_sensor_uid const *uid1,
                            sns_sensor_uid const *uid2)
{
  int result = sns_memcmp(uid1, uid2, sizeof(*uid1));
  return 0 == result;
}
