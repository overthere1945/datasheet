/** =============================================================================
 *  @file sns_lpi_tcm_bw.c
 *
 *  @brief The APIs to vote for the LPI TCM bus bandwidth. These utility
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

#include "icbarb.h"
#include "npa.h"

#include "sns_lpi_tcm_bw.h"
#include "sns_pd_util.h"

/*=============================================================================
  Constant Data Definitions
  ============================================================================*/

#define SNS_LPI_TCM_BW_NODE_NAME    "/icb/arbiter"
#define SNS_LPI_TCM_CLIENT_NAME_LEN 32

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_lpi_tcm_bw_handle *sns_lpi_tcm_bw_register(const char *client_name)
{
  npa_client_handle npa_handle;
  sns_lpi_tcm_bw_handle *client_handle;

  char client_name_pd[SNS_LPI_TCM_CLIENT_NAME_LEN];
  sns_pd_util_append(client_name_pd, SNS_LPI_TCM_CLIENT_NAME_LEN, client_name);

  ICBArb_MasterSlaveType master_slave[1];
  master_slave[0].eMaster = ICBID_MASTER_LPASS_PROC;
  master_slave[0].eSlave = ICBID_SLAVE_LPASS_LPI_TCM;

  npa_handle = npa_create_sync_client_ex(
      SNS_LPI_TCM_BW_NODE_NAME, client_name_pd, NPA_CLIENT_SUPPRESSIBLE_VECTOR,
      sizeof(master_slave), &master_slave);

  client_handle = (sns_lpi_tcm_bw_handle *)npa_handle;
  return client_handle;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_tcm_bw_deregister(sns_lpi_tcm_bw_handle *client_handle)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(NULL != client_handle)
  {
    npa_destroy_client((npa_client_handle)client_handle);
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received NULL client_handle.");
    rc = SNS_RC_INVALID_VALUE;
  }
  return rc;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_tcm_bw_set(sns_lpi_tcm_bw_handle *client_handle, uint64_t bw,
                          uint32_t usage_pct)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(NULL != client_handle)
  {
    ICBArb_RequestType req[1];

    req[0].arbType = ICBARB_REQUEST_TYPE_2;
    req[0].arbData.type2.uThroughPut = bw;
    req[0].arbData.type2.uUsagePercentage = usage_pct;

    npa_issue_vector_request((npa_client_handle)client_handle, 1,
                             (npa_resource_state *)req);
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received NULL client_handle.");
    rc = SNS_RC_INVALID_VALUE;
  }
  return rc;
}

/*----------------------------------------------------------------------------*/
