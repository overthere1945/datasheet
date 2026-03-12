/*============================================================================
  @file

  @brief
  Island interface for Sensors.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#if !defined(ENABLE_QSH_IN_STD_ISLAND) ||                                      \
    !defined(ENABLE_SSC_IN_STD_ISLAND) ||                                      \
    !defined(ENABLE_QSHTECH_IN_STD_ISLAND)
#include "cache_manager.h"
#endif

#include "sns_common_island_core_interface.h"
#include "island_user.h"
#include "npa.h"
#include "npa_resource.h"
#include "sns_isafe_list.h"
#include "sns_island.h"
#include "sns_island_config.h"
#include "uSleep_mode_trans.h"

/*============================================================================
  Macros
  ============================================================================*/

#define ISLAND_CODE SNS_SECTION(".text.SNS")
#define ISLAND_DATA SNS_SECTION(".data.SNS")

/*============================================================================
  Static Data
  ============================================================================*/

static sns_island_notification_cb_type
    sleep_notification_cb_array[SNS_ISLAND_CLIENT_MAX] ISLAND_DATA;

/*============================================================================
  Static Functions
  ============================================================================*/

ISLAND_CODE static void
qsh_island_state_notification_cb_internal(uSleep_state_notification state)
{
  sleep_notification_cb_array[SNS_ISLAND_CLIENT_QSH_FW](
      (sns_island_notification_cb_state)state);
}

ISLAND_CODE static void
qshtech_island_state_notification_cb_internal(uSleep_state_notification state)
{
  sleep_notification_cb_array[SNS_ISLAND_CLIENT_QSHTECH](
      (sns_island_notification_cb_state)state);
}

ISLAND_CODE static void
sns_island_state_notification_cb_internal(uSleep_state_notification state)
{
  sleep_notification_cb_array[SNS_ISLAND_CLIENT_SSC](
      (sns_island_notification_cb_state)state);
}

ISLAND_CODE static void
qsh_log_notification_cb_internal(uSleep_state_notification state)
{
  sleep_notification_cb_array[SNS_ISLAND_CLIENT_QSH_LOG](
      (sns_island_notification_cb_state)state);
}

/*============================================================================
  Public Functions
  ============================================================================*/

ISLAND_CODE
void sns_island_vote_request(sns_island_vote_handle *handle,
                             sns_island_vote_type vote,
                             sns_island_client_type client_type)
{
  npa_resource_state island_state = 0;

  // Handle Votes based on client and Vote request
  switch(client_type)
  {
  /*----------------------------------------------------------------------------*/
  case(SNS_ISLAND_CLIENT_QSH_FW):
  {
    // Handle QSH_FW Segment w/ Restrict Vote
    if(SNS_ISLAND_VOTE_DISABLE == vote)
    {
      island_state =
          USLEEP_VOTE(USLEEP_ALL_ISLANDS, USLEEP_CLIENT_RESTRICT_ISLAND);
    }
    // Handle QSH_FW Segment w/ Allow Vote
    else if(SNS_ISLAND_VOTE_ENABLE == vote)
    {
#ifdef SNS_USES_MULTI_ISLAND
      island_state = USLEEP_VOTE_DONT_CARE;
#else
      island_state =
          USLEEP_VOTE(USLEEP_ALL_ISLANDS, USLEEP_CLIENT_ALLOW_ISLAND);
#endif
    }
    break;
  }
  /*----------------------------------------------------------------------------*/
  case(SNS_ISLAND_CLIENT_QSH_SENSORS):
  {
    // Handle QSH_SENSORS Segment w/ Restrict Vote
    if(SNS_ISLAND_VOTE_DISABLE == vote)
    {
#ifdef SNS_USES_MULTI_ISLAND
      island_state = USLEEP_VOTE_DONT_CARE;
#else
      island_state =
          USLEEP_VOTE(QSH_USLEEP_ISLAND, USLEEP_CLIENT_RESTRICT_ISLAND);
#endif
    }

    // Handle QSH_SENSORS Segment w/ Allow Vote
    else if(SNS_ISLAND_VOTE_ENABLE == vote)
    {
#ifndef ENABLE_QSH_IN_STD_ISLAND
      uint32_t qsh_pool_mask = 0;

      qsh_pool_mask = LLCMgr_getPoolMask(QSH_ISLAND_POOL);

      island_state =
          USLEEP_VOTE(QSH_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND) |
          USLEEP_VOTE_LLC_POOL(qsh_pool_mask);
#else
      island_state = USLEEP_VOTE(QSH_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND);
#endif
    }
    break;
  }
  /*----------------------------------------------------------------------------*/
  case(SNS_ISLAND_CLIENT_SSC):
  {
    // Handle SSC Segment w/ Restrict Vote
    if(SNS_ISLAND_VOTE_DISABLE == vote)
    {
#ifdef SNS_USES_MULTI_ISLAND
      island_state = USLEEP_VOTE_DONT_CARE;
#else
      island_state =
          USLEEP_VOTE(SNS_USLEEP_ISLAND, USLEEP_CLIENT_RESTRICT_ISLAND);
#endif
    }
    // Handle SSC Segment w/ Allow Vote
    else if(SNS_ISLAND_VOTE_ENABLE == vote)
    {
#ifndef ENABLE_SSC_IN_STD_ISLAND
      uint32_t ssc_pool_mask = 0;

      ssc_pool_mask = LLCMgr_getPoolMask(SSC_ISLAND_POOL);

      island_state =
          USLEEP_VOTE(SNS_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND) |
          USLEEP_VOTE_LLC_POOL(ssc_pool_mask);

#else
      island_state = USLEEP_VOTE(SNS_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND);
#endif
    }
    break;
  }
  /*----------------------------------------------------------------------------*/
  case(SNS_ISLAND_CLIENT_QSHTECH):
  {
    // Handle QSHTech Segment w/ Restrict Vote
#ifndef QSHTECH_ISLAND_VOTE_UTIL_DISABLE
    if(SNS_ISLAND_VOTE_DISABLE == vote)
    {
#ifdef SNS_USES_MULTI_ISLAND
      island_state = USLEEP_VOTE_DONT_CARE;
#else
      island_state =
          USLEEP_VOTE(SNS_USLEEP_ISLAND, USLEEP_CLIENT_RESTRICT_ISLAND);
#endif
    }
    // Handle QSHTech Segment w/ Allow Vote
    else if(SNS_ISLAND_VOTE_ENABLE == vote)
    {
#ifndef ENABLE_QSHTECH_IN_STD_ISLAND
      uint32_t qshtech_pool_mask = 0;
      qshtech_pool_mask = LLCMgr_getPoolMask(QSHTECH_ISLAND_POOL);

      island_state =
          USLEEP_VOTE(QSHTECH_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND) |
          USLEEP_VOTE_LLC_POOL(qshtech_pool_mask);
#else
      island_state =
          USLEEP_VOTE(QSHTECH_USLEEP_ISLAND, USLEEP_CLIENT_ALLOW_ISLAND);
#endif
    }
#endif
    break;
  }
  default:
    break;
    /*----------------------------------------------------------------------------*/
  }

  npa_issue_required_request((npa_client_handle)handle, island_state);
}

ISLAND_CODE
sns_island_vote_handle *sns_island_vote_client_register(char *client_name)
{
  npa_client_handle npa_handle = npa_create_sync_client(
      USLEEP_NODE_NAME, client_name, NPA_CLIENT_REQUIRED);
  return (sns_island_vote_handle *)npa_handle;
}

sns_island_notification_cb_handle *sns_island_notification_cb_client_register(
    sns_island_notification_cb_type callback,
    sns_island_client_type client_type)
{
  uSleep_island_type island_type = USLEEP_ALL_ISLANDS;
  uSleep_notification_cb_type cb = NULL;

  if(client_type > SNS_ISLAND_CLIENT_MIN && client_type < SNS_ISLAND_CLIENT_MAX)
  {
    sleep_notification_cb_array[client_type] = callback;
  }

  switch(client_type)
  {
  case(SNS_ISLAND_CLIENT_QSH_FW):
  case(SNS_ISLAND_CLIENT_QSH_SENSORS):
  {
    island_type = USLEEP_ALL_ISLANDS;
    cb = qsh_island_state_notification_cb_internal;
    break;
  }
  case(SNS_ISLAND_CLIENT_SSC):
  {
    island_type = SNS_USLEEP_ISLAND;
    cb = sns_island_state_notification_cb_internal;
    break;
  }
  case(SNS_ISLAND_CLIENT_QSHTECH):
  {
    island_type = QSHTECH_USLEEP_ISLAND;
    cb = qshtech_island_state_notification_cb_internal;
    break;
  }
  case(SNS_ISLAND_CLIENT_QSH_LOG):
  {
    island_type = USLEEP_ALL_ISLANDS;
    cb = qsh_log_notification_cb_internal;
  }
  default:
    break;
  }

  return (sns_island_notification_cb_handle *)
      uSleep_registerNotificationCallbackEx(
          island_type, SNS_USLEEP_ISLAND_CB_ENTER_LATENCY_TICKS,
          SNS_USLEEP_ISLAND_CB_EXIT_LATENCY_TICKS, cb);
}

void sns_island_notification_cb_client_deregister(
    sns_island_notification_cb_handle *handle,
    sns_island_client_type client_type)
{
  if(client_type == SNS_ISLAND_CLIENT_QSH_LOG)
  {
    uSleep_island_type island_type = USLEEP_ALL_ISLANDS;
    uSleep_deregisterNotificationCallback(
        island_type, (uSleep_notification_cb_handle)handle);
  }
}

ISLAND_CODE
void sns_island_exit(void)
{
  uSleep_exit();
}
