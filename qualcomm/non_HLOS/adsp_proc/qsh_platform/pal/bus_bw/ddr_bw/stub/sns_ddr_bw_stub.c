/** ============================================================================
 *  @file
 *
 *  @brief The stub implementation of the APIs to vote for the DDR BW.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.\n 
 *  All rights reserved.\n 
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_ddr_bw.h"
#include "sns_types.h"

/*=============================================================================
  Constant Data Definitions
  ============================================================================*/

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

uint32_t sns_ddr_bw_register(char *client_name)
{
  UNUSED_VAR(client_name);
  return UINT32_MAX; // any non-zero value, won't be used
}
/*----------------------------------------------------------------------------*/

sns_rc sns_ddr_bw_deregister(uint32_t client_id)
{
  UNUSED_VAR(client_id);
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_ddr_bw_set(uint32_t client_id, uint64_t bw, uint32_t usage_pct)
{
  UNUSED_VAR(client_id);
  UNUSED_VAR(bw);
  UNUSED_VAR(usage_pct);
  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/
