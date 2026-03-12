/** ============================================================================
 *  @file
 *
 *  @brief This file contains Stub API implementation of sleep utility
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries. All rights reserved. Confidential and Proprietary -
 * Qualcomm Technologies, Inc.
 *
 *  ============================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_island.h"
#include "sns_sleep.h"
#include "sns_types.h"

/*==============================================================================
  Macros
  ============================================================================*/

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*==============================================================================
  Static data
  ============================================================================*/

/*==============================================================================
  Static Function definitions
  ============================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_sleep_register(sns_sleep_register_param *reg_param,
                          sns_sleep_handle **handle)
{
  UNUSED_VAR(reg_param);
  *handle = (sns_sleep_handle *)0xDEADDEAD;
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_sleep_deregister(sns_sleep_handle **handle)
{
  *handle = NULL;
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_sleep_request(sns_sleep_handle *handle,
                         sns_sleep_request_param *req_param)
{
  UNUSED_VAR(handle)
  UNUSED_VAR(req_param)
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
