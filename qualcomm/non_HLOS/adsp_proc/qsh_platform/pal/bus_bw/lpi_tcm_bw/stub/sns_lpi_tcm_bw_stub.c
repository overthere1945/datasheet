/** =============================================================================
 *  @file sns_lpi_tcm_bw_stub.c
 *
 *  @brief The Stub APIs to vote for the LPI TCM bus bandwidth. These utility
 *  APIs are not supported in island.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries. All rights reserved. Confidential and Proprietary - Qualcomm
 * Technologies, Inc.
 *  ===========================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_lpi_tcm_bw.h"
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

sns_lpi_tcm_bw_handle *sns_lpi_tcm_bw_register(const char *client_name)
{
  UNUSED_VAR(client_name);
  return (
      sns_lpi_tcm_bw_handle *)0xBADDFACE; // any non-NULL value, won't be used
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_tcm_bw_deregister(sns_lpi_tcm_bw_handle *client_handle)
{
  UNUSED_VAR(client_handle);
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_tcm_bw_set(sns_lpi_tcm_bw_handle *client_handle, uint64_t bw,
                          uint32_t usage_pct)
{
  UNUSED_VAR(client_handle);
  UNUSED_VAR(bw);
  UNUSED_VAR(usage_pct);
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
