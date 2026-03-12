/** =============================================================================
 *  @file sns_lpi_ls_bw.c
 *
 *  @brief The APIs to vote for the LPI_LS Bus bandwidth. These utility
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

#include <stdint.h>

#include "mmpm.h"
#include "sns_lpi_ls_bw.h"
#include "sns_pd_util.h"
#include "sns_printf_int.h"

/*=============================================================================
  Constant Data Definitions
  ============================================================================*/

#define SNS_PWR_LPI_CLIENT_NAME_LEN 32

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

uint32_t sns_lpi_ls_bw_register(char *client_name,
                                sns_lpi_ls_bw_client client_type)
{
  uint32_t client_id;

  MmpmRegParamType mmpmRegParam;

  char client_name_pd[SNS_PWR_LPI_CLIENT_NAME_LEN];
  sns_pd_util_append(client_name_pd, SNS_PWR_LPI_CLIENT_NAME_LEN, client_name);

  mmpmRegParam.rev = MMPM_REVISION;
  mmpmRegParam.coreId = MMPM_CORE_ID_SLPI_SDC;
  mmpmRegParam.instanceId = MMPM_CORE_INSTANCE_0;
  mmpmRegParam.MMPM_Callback = NULL;
  mmpmRegParam.pClientName = client_name_pd;
  mmpmRegParam.pwrCtrlFlag = PWR_CTRL_NONE;
  mmpmRegParam.callBackFlag = CALLBACK_NONE;
  mmpmRegParam.cbFcnStackSize = 0;

#ifndef SNS_CESTA_DISABLED
  mmpmRegParam.cestaClient.clientType = MMPM_CLIENT_SW;
  mmpmRegParam.cestaClient.clientNum = MMPM_Q6_CLIENT;
  if(SNS_LPI_LS_BW_SDC == client_type)
  {
    mmpmRegParam.cestaClient.clientType = MMPM_CLIENT_HW;
    mmpmRegParam.cestaClient.clientNum = MMPM_SDC_CLIENT;
  }
#else
  UNUSED_VAR(client_type);
#endif
  client_id = MMPM_Register_Ext(&mmpmRegParam);
  return client_id;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_ls_bw_deregister(uint32_t client_id)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(0 != client_id)
  {
    MMPM_STATUS mmpm_rc = MMPM_Deregister_Ext(client_id);
    if(mmpm_rc != MMPM_STATUS_SUCCESS)
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "sns_lpi_ls_bw_deregister failed."
                 "client_id=%d, error=%d ",
                 client_id, mmpm_rc);
      rc = SNS_RC_FAILED;
    }
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Invalid client id.");
    rc = SNS_RC_INVALID_VALUE;
  }
  return rc;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_lpi_ls_bw_set(uint32_t client_id, sns_lpi_ls_bw_client client_type,
                         uint64_t bw, uint32_t usage_pct)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(0 == client_id)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Invalid client id.");
    return SNS_RC_INVALID_VALUE;
  }

  MMPM_STATUS req_result = MMPM_STATUS_SUCCESS;
  MmpmRscExtParamType req_rsc_param;
  MMPM_STATUS result_status[1];
  MmpmRscParamType req_param[1];
  MmpmGenBwReqType mmpmBwReqParam;
  MmpmGenBwValType bandWidth;
  MmpmParameterConfigType mmpmParamConfig;

  // Send BW vote to DSP PM
  req_rsc_param.apiType = MMPM_API_TYPE_SYNC;
  req_rsc_param.numOfReq = 1;
  req_rsc_param.reqTag = 0;
  req_rsc_param.pStsArray = result_status;
  req_rsc_param.pReqArray = req_param;

  bandWidth.busRoute.masterPort = MMPM_BW_PORT_ID_Q6DSP_MASTER;
  if(SNS_LPI_LS_BW_SDC == client_type)
  {
    bandWidth.busRoute.masterPort = MMPM_BW_PORT_ID_SLPI_SDC_MASTER;
  }
  bandWidth.busRoute.slavePort = MMPM_BW_PORT_ID_SLPI_SRAM_SLAVE;

  bandWidth.bwValue.busBwValue.usageType = MMPM_BW_USAGE_LPASS_DSP;
  bandWidth.bwValue.busBwValue.usagePercentage = usage_pct;

  mmpmBwReqParam.numOfBw = 1;
  mmpmBwReqParam.pBandWidthArray = &bandWidth;

  req_param[0].rscId = MMPM_RSC_ID_GENERIC_BW;
  req_param[0].rscParam.pGenBwReq = &mmpmBwReqParam;

#ifndef SNS_CESTA_DISABLED
  if(SNS_LPI_LS_BW_SDC == client_type)
  {
    mmpmBwReqParam.pwrMode = MMPM_CESTA_ACTIVE;
    req_param[0].rscId = MMPM_RSC_ID_CESTA_BW;
  }
#else
  UNUSED_VAR(mmpmParamConfig);
#endif
  bandWidth.bwValue.busBwValue.bwBytePerSec = bw;
  req_result = MMPM_Request_Ext(client_id, &req_rsc_param);
  if(req_result == MMPM_STATUS_SUCCESS)
  {
    SNS_PRINTF(HIGH, sns_fw_printf,
               "ssc_vote: lpi_ls_bw=0x%X%08X, client_id=%u",
               (uint32_t)(bw >> 32), (uint32_t)bw, client_id);
  }
  else
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "lpi_ls_bw req failed. error=%d, client_id=%d", req_result,
               client_id);
    rc = SNS_RC_FAILED;
  }
#ifndef SNS_CESTA_DISABLED
  if(SNS_LPI_LS_BW_SDC == client_type)
  {
    mmpmParamConfig.paramId = MMPM_PARAM_ID_CESTA_CHANNEL_SWITCH;
    mmpmParamConfig.pParamConfig = NULL;
    req_result = MMPM_SetParameter(client_id, &mmpmParamConfig);
    if(MMPM_STATUS_SUCCESS != req_result)
    {
      SNS_PRINTF(ERROR, sns_fw_printf, " CESTA FAIL : Failed to set parameter");
      rc = SNS_RC_FAILED;
    }
  }
#endif
  return rc;
}

/*----------------------------------------------------------------------------*/
