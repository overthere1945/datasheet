/** =============================================================================
 *  @file sns_lpi_ls_bw_stub.c
 *
 *  @brief The Stub APIs to vote for the LPI_LS Bus bandwidth. These utility
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

#include "sns_lpi_ls_bw.h"
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

uint32_t sns_lpi_ls_bw_register(char *client_name,
                                sns_lpi_ls_bw_client client_type)
{
  UNUSED_VAR(client_name);
  UNUSED_VAR(client_type);
  return UINT32_MAX; // any non-zero value, won't be used
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_ls_bw_deregister(uint32_t client_id)
{
  UNUSED_VAR(client_id);
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_ls_bw_set(uint32_t client_id, sns_lpi_ls_bw_client client_type,
                         uint64_t bw, uint32_t usage_pct)
{
  UNUSED_VAR(client_id);
  UNUSED_VAR(client_type);
  UNUSED_VAR(bw);
  UNUSED_VAR(usage_pct);
  return SNS_RC_SUCCESS;
}

/*----------------------------------------------------------------------------*/
