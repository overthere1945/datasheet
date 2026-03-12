#pragma once
/** ============================================================================
 *  @file
 *
 *  @brief The APIs to vote for the DDR Bus bandwidth.
 *  @note These APIs are not supported in island.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or
 * itssubsidiaries.\n All rights reserved.\n Confidential and Proprietary -
 * Qualcomm Technologies, Inc.\n
 *
 *  ============================================================================
 */

/*==============================================================================
  Include Files
  ============================================================================*/

#include <stdint.h>
#include "sns_printf.h"
#include "sns_printf_int.h"
#include "sns_rc.h"

/*=============================================================================
  Public function declaration
  ============================================================================*/

/**
 * @brief The API to Register client for DDR BW voting.
 * @note  This API should be called only in big image.
 *
 * @param[in] client_name  Pointer to the client name.
 *
 * @return                 Non_Zero client ID when registration is successful.
 *                         Zero client ID when registration fails.
 */

uint32_t sns_ddr_bw_register(char *client_name);

/* ---------------------------------------------------------------------------*/
/**
 * @brief The API to deregister client for the DDR BW voting.
 * @note  This API should be called only in big image.
 *
 * @param[in] client_id          client_id registered with sensor_ddr_bw
 * utility.
 *
 * @retval SNS_RC_SUCCESS        client_id deregistered successfully.
 * @retval SNS_RC_INVALID_VALUE  Invalid client_id passed.
 * @retval SNS_RC_FAILED         client_id deregistration failed.
 *
 */
sns_rc sns_ddr_bw_deregister(uint32_t client_id);

/* ---------------------------------------------------------------------------*/
/**
 * @brief The API to vote for the DDR Bandwidth.
 * @note  This API should be called only in big image.
 *
 * @param[in] client_id         Client ID registered with the ddr bw utility.
 * @param[in] bw                Bandwidth in bytes per second.
 * @param[in] usage_pct         Bandwidth usage in percentage.
 *
 * @retval SNS_RC_SUCCESS       DDR BW vote is set successfully.
 * @retval SNS_RC_INVALID_VALUE Invalid client_id passed.
 * @retval SNS_RC_FAILED        operation failed.
 *
 */
sns_rc sns_ddr_bw_set(uint32_t client_id, uint64_t bw, uint32_t usage_pct);

/* ---------------------------------------------------------------------------*/
