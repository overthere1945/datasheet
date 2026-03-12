/** ============================================================================
 *  @file
 *
 *  @brief The APIs to vote for the DDR BW.
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

#include "mmpm.h"
#include "sns_ddr_bw.h"
#include "sns_pd_util.h"
#include "sns_printf_int.h"

/*==============================================================================
  Macros
  ============================================================================*/

#define SNS_DDR_BW_CLIENT_NAME_LEN 32

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
  uint32_t client_id;

  MmpmRegParamType mmpmRegParam;

  char client_name_pd[SNS_DDR_BW_CLIENT_NAME_LEN];
  sns_pd_util_append(client_name_pd, SNS_DDR_BW_CLIENT_NAME_LEN, client_name);

  mmpmRegParam.rev = MMPM_REVISION;
  mmpmRegParam.coreId = MMPM_CORE_ID_LPASS_ADSP;
  mmpmRegParam.instanceId = MMPM_CORE_INSTANCE_0;
  mmpmRegParam.MMPM_Callback = NULL;
  mmpmRegParam.pClientName = client_name_pd;
  mmpmRegParam.pwrCtrlFlag = PWR_CTRL_PERIODIC_CLIENT;
  mmpmRegParam.callBackFlag = CALLBACK_NONE;
  mmpmRegParam.cbFcnStackSize = 128;

  client_id = MMPM_Register_Ext(&mmpmRegParam);
  return client_id;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_ddr_bw_deregister(uint32_t client_id)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(0 != client_id)
  {
    MMPM_STATUS mmpm_rc = MMPM_Deregister_Ext(client_id);
    if(mmpm_rc != MMPM_STATUS_SUCCESS)
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "sns_ddr_bw_deregister failed."
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

sns_rc sns_ddr_bw_set(uint32_t client_id, uint64_t bw, uint32_t usage_pct)
{
  sns_rc rc = SNS_RC_SUCCESS;
  if(0 == client_id)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Invalid client id.");
    rc = SNS_RC_INVALID_VALUE;
    return rc;
  }

  MMPM_STATUS req_result = MMPM_STATUS_SUCCESS;
  MmpmRscExtParamType req_rsc_param;
  MMPM_STATUS result_status[1];
  MmpmRscParamType req_param[1];
  MmpmGenBwReqType mmpmBwReqParam;
  MmpmGenBwValType bandWidth;

#ifndef SNS_DISABLE_ISLAND
  MMPM_STATUS set_param_rc = MMPM_STATUS_FAILED;
  MmpmParameterConfigType paramConfig;
  MmpmIslandParticipationType islandOption;

  // Send set_param request to specify this is island participation client.
  // so that dsp pm will supress ddr bw vote if system is going to island.
  islandOption.islandOpt = MMPM_ISLAND_PARTICIPATION_LPI;

  paramConfig.paramId = MMPM_PARAM_ID_ISLAND_PARTICIPATION;
  paramConfig.pParamConfig = &islandOption;

  set_param_rc = MMPM_SetParameter(client_id, &paramConfig);

  if(set_param_rc != MMPM_STATUS_SUCCESS)
  {
    SNS_PRINTF(ERROR, sns_fw_printf,
               "BW set_param failed. error=%d, client_id=%u", set_param_rc,
               client_id);
    rc = SNS_RC_FAILED;
  }
  else
#endif
  {
    // Send BW vote to DSP PM
    req_rsc_param.apiType = MMPM_API_TYPE_ASYNC;
    req_rsc_param.numOfReq = 1;
    req_rsc_param.reqTag = 0;
    req_rsc_param.pStsArray = result_status;
    req_rsc_param.pReqArray = req_param;

    bandWidth.busRoute.masterPort = MMPM_BW_PORT_ID_ADSP_MASTER;
    bandWidth.busRoute.slavePort = MMPM_BW_PORT_ID_DDR_SLAVE;

    bandWidth.bwValue.busBwValue.usageType = MMPM_BW_USAGE_LPASS_DSP;
    bandWidth.bwValue.busBwValue.usagePercentage = usage_pct;

    mmpmBwReqParam.numOfBw = 1;
    mmpmBwReqParam.pBandWidthArray = &bandWidth;

    req_param[0].rscId = MMPM_RSC_ID_GENERIC_BW;
    req_param[0].rscParam.pGenBwReq = &mmpmBwReqParam;

    bandWidth.bwValue.busBwValue.bwBytePerSec = bw;

    req_result = MMPM_Request_Ext(client_id, &req_rsc_param);

    if(req_result == MMPM_STATUS_SUCCESS)
    {
      SNS_PRINTF(HIGH, sns_fw_printf, "ssc_vote: ddr bw=0x%X%08X, client_id=%u",
                 (uint32_t)(bw >> 32), (uint32_t)bw, client_id);
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "ddr bw req failed. error=%d, client_id=%d", req_result,
                 client_id);
      rc = SNS_RC_FAILED;
    }
  }
  return rc;
}

/*----------------------------------------------------------------------------*/
