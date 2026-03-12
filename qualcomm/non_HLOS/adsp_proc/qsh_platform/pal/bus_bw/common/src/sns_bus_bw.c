/** ============================================================================
 * @file
 *
 * @brief The APIs to vote for the bus bandwidth
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries.\n All rights reserved.\n Confidential and Proprietary -
 * Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_bus_bw.h"
#include "sns_core_def.h"
#include "sns_ddr_bw.h"
#include "sns_lpi_ls_bw.h"
#include "sns_lpi_tcm_bw.h"
#include "sns_memmgr.h"
#include "sns_printf.h"
#include "sns_printf_int.h"
#include "sns_rc.h"

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef enum sns_bus_bw_bus_type
{
  SNS_BUS_BW_BUS_TYPE_UNKNOWN = 0,
  SNS_BUS_BW_BUS_TYPE_DDR = 1,
  SNS_BUS_BW_BUS_TYPE_SRAM = 2,
  SNS_BUS_BW_BUS_TYPE_TCM = 3
} sns_bus_bw_bus_type;

typedef struct sns_bus_bw_handle_priv
{
  sns_bus_bw_register_param reg_param;
  sns_bus_bw_req_param req_param;
  sns_lpi_tcm_bw_handle *tcm_handle;
  uint32_t ddr_handle;
  uint32_t lpi_ls_handle;
  sns_bus_bw_bus_type bus_type;
} sns_bus_bw_handle_priv;

/*==============================================================================
  macros
  ============================================================================*/

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_bus_bw_register(sns_bus_bw_register_param *reg_param,
                           sns_bus_bw_handle **handle)
{
  sns_rc rc = SNS_RC_SUCCESS;

  sns_bus_bw_handle_priv *priv_handle = NULL;

  if(NULL == reg_param || NULL == handle)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "NULL:reg_par(%x),handle(%x),*handle(%x)",
               reg_param, handle);
    return SNS_RC_INVALID_VALUE;
  }

  priv_handle = sns_malloc(SNS_HEAP_MAIN, sizeof(sns_bus_bw_handle_priv));
  if(NULL == priv_handle)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "memory allocation failed");
    rc = SNS_RC_FAILED;
    *handle = (sns_bus_bw_handle *)priv_handle;
    return rc;
  }

  priv_handle->reg_param.master_port = reg_param->master_port;
  priv_handle->reg_param.slave_port = reg_param->slave_port;

  switch(reg_param->master_port)
  {
  case SNS_BUS_BW_MASTER_PORT_ID_PRIMARY:
  {
    if(SNS_BUS_BW_SLAVE_PORT_ID_DDR == reg_param->slave_port)
    {
      priv_handle->bus_type = SNS_BUS_BW_BUS_TYPE_DDR;
      priv_handle->ddr_handle = sns_ddr_bw_register(reg_param->client_name);
    }
    else if((SNS_BUS_BW_SLAVE_PORT_ID_SRAM == reg_param->slave_port) &&
            (SNS_PRIMARY_CORE_ID == SNS_CORE_ID_ADSP))
    {
      priv_handle->bus_type = SNS_BUS_BW_BUS_TYPE_SRAM;
      priv_handle->lpi_ls_handle =
          sns_lpi_ls_bw_register(reg_param->client_name, SNS_LPI_LS_BW_Q6);
    }
    else if(SNS_BUS_BW_SLAVE_PORT_ID_TCM == reg_param->slave_port)
    {
      priv_handle->bus_type = SNS_BUS_BW_BUS_TYPE_TCM;
      priv_handle->tcm_handle = sns_lpi_tcm_bw_register(reg_param->client_name);
    }
    break;
  }
  case SNS_BUS_BW_MASTER_PORT_ID_SECONDARY:
  {
    if(SNS_BUS_BW_SLAVE_PORT_ID_SRAM == reg_param->slave_port)
    {
      priv_handle->bus_type = SNS_BUS_BW_BUS_TYPE_SRAM;
      priv_handle->lpi_ls_handle =
          sns_lpi_ls_bw_register(reg_param->client_name, SNS_LPI_LS_BW_SDC);
    }
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported reg params");
    sns_free(priv_handle);
    priv_handle = NULL;
    rc = SNS_RC_INVALID_VALUE;
  }
  }
  *handle = (sns_bus_bw_handle *)priv_handle;
  return rc;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_bus_bw_deregister(sns_bus_bw_handle **handle)
{
  sns_rc rc = SNS_RC_SUCCESS;

  sns_bus_bw_handle_priv *priv_handle = NULL;

  if(NULL == handle)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received NULL handle(%x)", handle);
    return SNS_RC_INVALID_VALUE;
  }
  else if(NULL == *handle)
  {
	SNS_PRINTF(ERROR, sns_fw_printf, "Received NULL *handle(%x)", *handle);
    return SNS_RC_INVALID_VALUE;
  }

  priv_handle = (sns_bus_bw_handle_priv *)*handle;

  switch(priv_handle->bus_type)
  {
  case SNS_BUS_BW_BUS_TYPE_DDR:
  {
    rc = sns_ddr_bw_deregister(priv_handle->ddr_handle);
    break;
  }
  case SNS_BUS_BW_BUS_TYPE_SRAM:
  {
    rc = sns_lpi_ls_bw_deregister(priv_handle->lpi_ls_handle);
    break;
  }
  case SNS_BUS_BW_BUS_TYPE_TCM:
  {
    rc = sns_lpi_tcm_bw_deregister(priv_handle->tcm_handle);
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported bus type");
    rc = SNS_RC_INVALID_VALUE;
  }
  }
  sns_free(priv_handle);
  *handle = NULL;
  return rc;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_bus_bw_request(sns_bus_bw_handle *handle,
                          sns_bus_bw_req_param *req_param)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_bus_bw_handle_priv *priv_handle = NULL;

  if(NULL == handle || NULL == req_param)
  {
    return SNS_RC_INVALID_VALUE;
  }
  priv_handle = (sns_bus_bw_handle_priv *)handle;

  switch(priv_handle->bus_type)
  {
  case SNS_BUS_BW_BUS_TYPE_DDR:
  {
    rc = sns_ddr_bw_set(priv_handle->ddr_handle, req_param->bw,
                        req_param->usage_pct);
    break;
  }
  case SNS_BUS_BW_BUS_TYPE_SRAM:
  {
    sns_bus_bw_master_port_id master_port;
    master_port = priv_handle->reg_param.master_port;

    if((master_port == SNS_BUS_BW_MASTER_PORT_ID_SECONDARY) &&
       (SNS_SECONDARY_CORE_ID == SNS_CORE_ID_SDC))
    {
      rc = sns_lpi_ls_bw_set(priv_handle->lpi_ls_handle, SNS_LPI_LS_BW_SDC,
                             req_param->bw, req_param->usage_pct);
    }
    break;
  }
  case SNS_BUS_BW_BUS_TYPE_TCM:
  {
    rc = sns_lpi_tcm_bw_set(priv_handle->tcm_handle, req_param->bw,
                            req_param->usage_pct);
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported bus type");
    rc = SNS_RC_INVALID_VALUE;
  }
  }
  return rc;
}
/*----------------------------------------------------------------------------*/
