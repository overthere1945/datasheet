#pragma once
/** =============================================================================
 *  @file
 *
 *  @brief The APIs to vote for the LPI_LS Bus bandwidth. This vote is required
 *  to access pram memory from the Q6 & SDC.
 *  @note These APIs are not supported in island.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries.\n All rights reserved.\n Confidential and Proprietary -
 * Qualcomm Technologies, Inc.\n
 *
 *  ===========================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include <stdint.h>
#include "sns_printf.h"
#include "sns_printf_int.h"
#include "sns_rc.h"

/*=============================================================================
  Type Definitions
  ============================================================================*/

/**
 * @brief client types who can vote for lpi_ls bandwidth
 *
 */
typedef enum sns_lpi_ls_bw_client
{
  SNS_LPI_LS_BW_Q6 = 0,
  SNS_LPI_LS_BW_SDC = 1

} sns_lpi_ls_bw_client;

/*=============================================================================
  Public function declaration
  ============================================================================*/

/**
 * @brief The API to Register client for the LPI_LS bus bandwidth voting.
 * @note  This API should be called only in big image.
 *
 * @param[in] *client_name  Ptr to the client name.
 *                          The max length is 32 characters.
 * @param[in] client_type   Client type ( SNS_LPI_LS_BW_Q6 / SNS_LPI_LS_BW_SDC )
 *
 * @return client_id        Non zero client ID when registration is successful.
 *                          Zero client ID when registration fails.
 *
 */

uint32_t sns_lpi_ls_bw_register(char *client_name,
                                sns_lpi_ls_bw_client client_type);

/* ---------------------------------------------------------------------------*/
/**
 * @brief The API to De-register client.
 * @note  This API should be called only in big image.
 *
 * @param[in] client_id  client_id registered with sensor lpi_ls_bw utility.
 *
 * @retval SNS_RC_SUCCESS         client_id deregistered successfully.
 * @retval SNS_RC_INVALID_VALUE   Invalid client_id passed.
 * @retval SNS_RC_FAILED          client_id deregistered failed.
 *
 */

sns_rc sns_lpi_ls_bw_deregister(uint32_t client_id);

/* ---------------------------------------------------------------------------*/
/**
 * @brief The Sensor API to vote for LPI LS NOC Bandwidth.
 * @note  This function should be called only in big image.
 *
 * @param[in] client_id          Client ID registered with the lpi ls bw
 * utility.
 * @param[in] client_type        Client type ( SNS_LPI_LS_BW_Q6 /
 * SNS_LPI_LS_BW_SDC )
 * @param[in] bw                 Bandwidth in bytes per second.
 * @param[in] usage_percentage   Bandwidth usage in percentage.
 *
 * @retval SNS_RC_SUCCESS        lpi_ls BW vote is set successfully.
 * @retval SNS_RC_INVALID_VALUE  Invalid client_id passed.
 * @retval SNS_RC_FAILED         operation failed.
 *
 */
sns_rc sns_lpi_ls_bw_set(uint32_t client_id, sns_lpi_ls_bw_client client_type,
                         uint64_t bw, uint32_t usage_percentage);

/* ---------------------------------------------------------------------------*/
