/** ============================================================================
 *  @file
 *
 *  @brief This file contains API implementation of sleep utility
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

#include <stdbool.h>

#include "cache_mgr_node.h"
#include "npa.h"
#include "sns_island.h"
#include "sns_island_config.h"
#include "sns_memmgr.h"
#include "sns_pd_util.h"
#include "sns_printf.h"
#include "sns_printf_int.h"
#include "sns_sleep.h"
#include "sns_types.h"
#include "unpa_remote.h"
#include "uSleep_islands.h"
#include "uSleep_npa.h"

/*==============================================================================
  Macros
  ============================================================================*/

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef struct sns_sleep_handle_priv
{
  union
  {
    unpa_remote_client unpa_client;
    npa_client_handle npa_client;
  };
  void *attr_info;
  sns_sleep_attr attr;

} sns_sleep_handle_priv;

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
  sns_rc rc = SNS_RC_SUCCESS;
  sns_sleep_handle_priv *priv_handle = NULL;
  char client_name_pd[SNS_SLEEP_CLIENT_NAME_LEN];

  if(NULL == reg_param || NULL == handle)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "NULL:reg_par(%x),handle(%x)", reg_param,
               handle);
    return SNS_RC_INVALID_VALUE;
  }

  priv_handle = sns_malloc(SNS_HEAP_ISLAND, sizeof(sns_sleep_handle_priv));
  if(NULL == priv_handle)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "memory allocation failed");
    rc = SNS_RC_FAILED;
    *handle = (sns_sleep_handle *)priv_handle;
    return rc;
  }
  priv_handle->attr = reg_param->attr;

  sns_pd_util_append(client_name_pd, SNS_SLEEP_CLIENT_NAME_LEN,
                     reg_param->client_name);

  switch(priv_handle->attr)
  {
  case SNS_SLEEP_ATTR_ISLAND_LPM:
  {
    priv_handle->unpa_client = unpaRemote_initClientEx(
        client_name_pd, UNPA_CLIENT_REQUIRED, ISLAND_CPU_VDD_NODE_NAME,
        (void *)QSH_USLEEP_ISLAND);

    if(0 != priv_handle->unpa_client)
    {
      SNS_PRINTF(HIGH, sns_fw_printf,
                 "island_lpm: registration successful."
                 "unpa_client(%d)",
                 priv_handle->unpa_client);
      *handle = (sns_sleep_handle *)priv_handle;
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "island_lpm: registration failed.");
      sns_free(priv_handle);
      rc = SNS_RC_FAILED;
    }
    break;
  }
  case SNS_SLEEP_ATTR_TCM_LPM:
  {
    priv_handle->npa_client = npa_create_sync_client(
        TCM_MGR_LPR_NODE_NAME, client_name_pd, NPA_CLIENT_REQUIRED);

    if(NULL != priv_handle->npa_client)
    {
      SNS_PRINTF(HIGH, sns_fw_printf,
                 "tcm_lpm: registration successful. "
                 "npa_client(%d)",
                 priv_handle->npa_client);
      *handle = (sns_sleep_handle *)priv_handle;
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf, "tcm_lpm: registration failed.");
      sns_free(priv_handle);
      rc = SNS_RC_FAILED;
    }
    break;
  }
  case SNS_SLEEP_ATTR_WAKUP_INTERVAL:
  {
    priv_handle->npa_client = npa_create_sync_client(
        SLEEP_USEC_MAX_DURATION_NODE_NAME, client_name_pd, NPA_CLIENT_REQUIRED);
    if(NULL != priv_handle->npa_client)
    {
      SNS_PRINTF(HIGH, sns_fw_printf,
                 "wakeup_interval: registration "
                 "successful. npa_client(%d)",
                 priv_handle->npa_client);
      *handle = (sns_sleep_handle *)priv_handle;
    }
    else
    {
      SNS_PRINTF(ERROR, sns_fw_printf,
                 "wakeup_interval: registration "
                 "failed.");
      sns_free(priv_handle);
      rc = SNS_RC_FAILED;
    }
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported attr_id(%d)",
               reg_param->attr);
    sns_free(priv_handle);
    *handle = NULL;
    rc = SNS_RC_INVALID_VALUE;
    break;
  }
  }
  return rc;
}
/*----------------------------------------------------------------------------*/

sns_rc sns_sleep_deregister(sns_sleep_handle **handle)
{
  sns_rc rc = SNS_RC_SUCCESS;

  sns_sleep_handle_priv *priv_handle = NULL;

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

  priv_handle = (sns_sleep_handle_priv *)*handle;

  switch(priv_handle->attr)
  {
  case SNS_SLEEP_ATTR_ISLAND_LPM:
  {
    unpaRemote_destroyClient(priv_handle->unpa_client);
    sns_free(priv_handle);
    *handle = NULL;
    break;
  }
  case SNS_SLEEP_ATTR_TCM_LPM:
  {
    npa_destroy_client(priv_handle->npa_client);
    sns_free(priv_handle);
    *handle = NULL;
    break;
  }
  case SNS_SLEEP_ATTR_WAKUP_INTERVAL:
  {
    npa_destroy_client(priv_handle->npa_client);
    sns_free(priv_handle);
    *handle = NULL;
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported attr_id(%d)",
               priv_handle->attr);
    sns_free(priv_handle);
    *handle = NULL;
    rc = SNS_RC_INVALID_VALUE;
    break;
  }
  }
  return rc;
}
/*----------------------------------------------------------------------------*/
SNS_SECTION(".text.sns")
sns_rc sns_sleep_request(sns_sleep_handle *handle,
                         sns_sleep_request_param *req_param)
{
  sns_rc rc = SNS_RC_SUCCESS;
  sns_sleep_handle_priv *priv_handle = NULL;
  sns_island_lpm_info_t *island_lpm;
  sns_tcm_lpm_info_t *tcm_lpm;
  sns_wakeup_interval_info_t *wakeup_interval;

  if(NULL == handle || NULL == req_param)
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "NULL: handle(%x) or req_param(%x)",
               handle, req_param);
    return SNS_RC_INVALID_VALUE;
  }

  priv_handle = (sns_sleep_handle_priv *)handle;

  switch(priv_handle->attr)
  {
  case SNS_SLEEP_ATTR_ISLAND_LPM:
  {
    island_lpm = (sns_island_lpm_info_t *)req_param->attr_info;
    if(island_lpm->island_lpm == SNS_SLEEP_MODE_ENABLE)
    {
      unpaRemote_issueRequest(priv_handle->unpa_client, ISLAND_LPR_ENABLE_ALL);
      SNS_PRINTF(HIGH, sns_fw_printf, "island_lpm: enabled");
    }
    else if(island_lpm->island_lpm == SNS_SLEEP_MODE_DISABLE)
    {
      unpaRemote_issueRequest(priv_handle->unpa_client,
                              ISLAND_LPR_DISABLE_LPM_PLUS);
      SNS_PRINTF(HIGH, sns_fw_printf, "island_lpm: disabled");
    }
    break;
  }
  case SNS_SLEEP_ATTR_TCM_LPM:
  {
    tcm_lpm = (sns_tcm_lpm_info_t *)req_param->attr_info;

    if(tcm_lpm->tcm_pc == SNS_SLEEP_MODE_ENABLE)
    {
      SNS_ISLAND_EXIT();
      npa_issue_required_request(priv_handle->npa_client,
                                 TCM_MGR_LPR_ALLOW_ALL);
      SNS_PRINTF(HIGH, sns_fw_printf, "tcm_lpm: enabled");
    }
    else if(tcm_lpm->tcm_pc == SNS_SLEEP_MODE_DISABLE)
    {
      SNS_ISLAND_EXIT();
      npa_issue_required_request(priv_handle->npa_client,
                                 TCM_MGR_LPR_RETENTION_ONLY);
      SNS_PRINTF(HIGH, sns_fw_printf, "tcm_lpm: disabled");
    }
    break;
  }
  case SNS_SLEEP_ATTR_WAKUP_INTERVAL:
  {
    wakeup_interval = (sns_wakeup_interval_info_t *)req_param->attr_info;
    SNS_ISLAND_EXIT();
    if(wakeup_interval != 0)
    {
      npa_issue_required_request(priv_handle->npa_client,
                                 wakeup_interval->wakeup_interval_us);

      SNS_PRINTF(HIGH, sns_fw_printf, "set system wakeup_interval:(%d)us",
                 wakeup_interval->wakeup_interval_us);
    }
    else
    {
      npa_complete_request(priv_handle->npa_client);
    }
    break;
  }
  default:
  {
    SNS_PRINTF(ERROR, sns_fw_printf, "Received unsupported attr_id(%d)",
               priv_handle->attr);
    rc = SNS_RC_INVALID_VALUE;
    break;
  }
  }
  return rc;
}
/*----------------------------------------------------------------------------*/
