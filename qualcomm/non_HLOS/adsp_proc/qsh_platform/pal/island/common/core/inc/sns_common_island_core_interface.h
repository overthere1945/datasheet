#pragma once
/** ============================================================================
 *  @file
 *
 *  @brief Island core interface APIs
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.\n 
 *  All rights reserved.\n 
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*=============================================================================
  Includes
  ===========================================================================*/

#include "sns_island_util.h"
#include "sns_isafe_list.h"
#include "sns_sensor.h"
#include "sns_types.h"
#include "sns_time.h"
#include "sns_rc.h"

/*============================================================================
  Macros
  ============================================================================*/

#define ISLAND_CODE   SNS_SECTION(".text.SNS")
#define ISLAND_DATA   SNS_SECTION(".data.SNS")
#define ISLAND_RODATA SNS_SECTION(".rodata.SNS")

/*============================================================================
  Types (Common)
  ============================================================================*/

/**
 * @brief Handle for island vote
 */
typedef struct sns_island_vote_handle sns_island_vote_handle;

/**
 * @brief Handle for island notification callback
 */
typedef struct sns_island_notification_cb_handle
    sns_island_notification_cb_handle;

/**
 * @brief Island state used in island notification callback
 */
typedef enum
{
  SNS_ISLAND_STATE_ENTER = 0,
  SNS_ISLAND_STATE_EXIT

} sns_island_notification_cb_state;

/**
 * @brief Island state notification callback function type
 */
typedef void (*sns_island_notification_cb_type)(
    sns_island_notification_cb_state state);

/**
 * @brief Island client type
 */
typedef enum
{
  SNS_ISLAND_CLIENT_MIN,
  SNS_ISLAND_CLIENT_QSH_FW = 1,
  SNS_ISLAND_CLIENT_QSH_SENSORS,
  SNS_ISLAND_CLIENT_SSC,
  SNS_ISLAND_CLIENT_QSHTECH,
  SNS_ISLAND_CLIENT_QSH_LOG,
  SNS_ISLAND_CLIENT_MAX

} sns_island_client_type;

/**
 * @brief Island vote type
 */
typedef enum
{
  SNS_ISLAND_VOTE_DISABLE = 1,
  SNS_ISLAND_VOTE_ENABLE,
} sns_island_vote_type;

/*=============================================================================
  Functions
  ===========================================================================*/

/**
 * @brief The API to set island vote for the given island client
 *
 * @param[in] handle          The handle for issuing the request.
 * @param[in] vote            The vote specified by the client.
 * @param[in] client_type     The type of the client making the request.
 */
void sns_island_vote_request(sns_island_vote_handle *handle,
                             sns_island_vote_type vote,
                             sns_island_client_type client_type);

/**
 * @brief The API to register handle for island vote
 *
 * @param[in] client_name     The name of the client thats being created.
 *
 * @return                    Non-NULL handle if the handle was created
 * successfully. NULL otherwise
 */
sns_island_vote_handle *sns_island_vote_client_register(char *client_name);

/**
 * @brief The API to register handle for island notification callback
 *
 * @param[in] callback        Provided callback for island notification
 * @param[in] client_type     The type of the island client.
 *
 * @return                    Non-NULL handle if the handle was created
 * successfully. NULL otherwise
 */
sns_island_notification_cb_handle *sns_island_notification_cb_client_register(
    sns_island_notification_cb_type callback,
    sns_island_client_type client_type);

/**
 * @brief The API to deregister handle for island notification callback
 *
 * @param[in] handle          Island notification callback handle
 * @param[in] client_type     The type of the island client.
 */
void sns_island_notification_cb_client_deregister(
    sns_island_notification_cb_handle *handle,
    sns_island_client_type client_type);

/**
 * @brief The API calls island exit
 */
void sns_island_exit(void);
