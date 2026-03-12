/*============================================================================
  @file sns_com_port_profile.c

  @brief Latency profile for com port module.

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  ============================================================================*/
#include "sns_com_port_profile.h"
#include "sns_com_port_priv.h"
#include "sns_island.h"
#include "sns_island_util.h"
#include "sns_memmgr.h"
/**
*****************************************************************************************
                               Data
*****************************************************************************************
*/
/**
*****************************************************************************************
                              Public Functions
*****************************************************************************************
*/
SNS_SECTION(".text.sns")
void sns_com_port_profile(sns_sync_com_port_handle *port_handle,
                          sns_profile_action action,
                          function_list function_to_profile)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  sns_profile_t *com_port_profile = NULL;
  if(NULL == priv_handle)
  {
    return;
  }
  com_port_profile = (sns_profile_t *)priv_handle->prof;
  sns_latency_profile(action, &com_port_profile[function_to_profile]);
}

void sns_com_port_profile_deinit(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  if(NULL == priv_handle)
  {
    return;
  }
  SNS_PRINTF(
      MED, sns_fw_printf,
      "sns_com_port_profile_deinit:: De-Allocate Memory for profiling %x",
      priv_handle);
  sns_free(priv_handle->prof);
  priv_handle->prof = NULL;
}

void sns_com_port_profile_init(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  int iter = 0;
  sns_profile_t *com_port_profile = NULL;

  if(NULL == priv_handle->prof)
  {
    priv_handle->prof =
        sns_malloc(SNS_HEAP_ISLAND, sizeof(sns_profile_t) * PROFILE_FUNC_LAST);
    if(NULL == priv_handle->prof)
    {
      SNS_PRINTF(
          MED, sns_fw_printf,
          "sns_com_port_profile:: Unable to allocated memory for profiling");
      return;
    }
  }
  SNS_PRINTF(MED, sns_fw_printf,
             "sns_com_port_profile :: Allocate Memory for profiling %x",
             priv_handle);
  com_port_profile = (sns_profile_t *)priv_handle->prof;
  for(iter = 0; iter < PROFILE_FUNC_LAST; iter++)
  {
    sns_latency_profile_reset(&com_port_profile[iter]);
  }
}

void sns_com_port_latency_dump(sns_sync_com_port_handle *port_handle)
{
  sns_com_port_priv_handle *priv_handle =
      (sns_com_port_priv_handle *)port_handle;
  int iter = 0;
  sns_profile_t *com_port_profile = NULL;

  if(NULL == priv_handle)
  {
    return;
  }
  com_port_profile = (sns_profile_t *)priv_handle->prof;
  for(iter = 0; iter < PROFILE_FUNC_LAST; iter++)
  {
    SNS_PRINTF(MED, sns_fw_printf, "Profile Data for %d -Func-%d %p",
               priv_handle->bus_info.bus_type, iter, port_handle);
    sns_latency_profile_log(&com_port_profile[iter]);
  }
}
