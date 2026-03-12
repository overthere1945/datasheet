/** ============================================================================
 *  @file
 *
 *  @brief The stub APIs to vote for the bus bandwidth
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries.\n All rights reserved.\n Confidential and Proprietary -
 * Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_bus_bw.h"
#include "sns_types.h"

/*==============================================================================
  Macros
  ============================================================================*/

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_bus_bw_register(sns_bus_bw_register_param *reg_param,
                           sns_bus_bw_handle **handle)
{
  UNUSED_VAR(reg_param);
  *handle = (sns_bus_bw_handle *)0xDEADDEAD;
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_bus_bw_deregister(sns_bus_bw_handle **handle)
{
  *handle = NULL;
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_bus_bw_request(sns_bus_bw_handle *handle,
                          sns_bus_bw_req_param *req_param)
{
  UNUSED_VAR(handle)
  UNUSED_VAR(req_param)
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
