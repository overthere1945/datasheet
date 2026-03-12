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

SNS_SECTION(".text.sns")
void sns_com_port_profile(sns_sync_com_port_handle *port_handle,
                          sns_profile_action action,
                          function_list function_to_profile)
{
  UNUSED_VAR(action);
  UNUSED_VAR(port_handle);
  UNUSED_VAR(function_to_profile);
}
void sns_com_port_profile_deinit(sns_sync_com_port_handle *port_handle)
{
  UNUSED_VAR(port_handle);
}

void sns_com_port_profile_init(sns_sync_com_port_handle *port_handle)
{
  UNUSED_VAR(port_handle);
}

void sns_com_port_latency_dump(sns_sync_com_port_handle *port_handle)
{
  UNUSED_VAR(port_handle);
}
