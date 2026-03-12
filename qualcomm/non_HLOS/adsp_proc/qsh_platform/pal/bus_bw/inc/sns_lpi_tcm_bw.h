#pragma once
/** =============================================================================
 *  @file
 *
 *  @brief The APIs to vote for the LPI_TCM bandwidth. This vote is required to
 *  access LPI_TCM memory from the Q6 & SDC.
 *  @note These APIs are not supported in island.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its
 * subsidiaries. \n All rights reserved. \n Confidential and Proprietary -
 * Qualcomm Technologies, Inc. \n
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
  Static Data Definitions
  ============================================================================*/

/*=============================================================================
  Type definitions
  ============================================================================*/

/**
 *  @brief Opaque handle to the lpi_tcm_bw interface registration.
 *
 */
typedef struct sns_lpi_tcm_bw_handle sns_lpi_tcm_bw_handle;

/*=============================================================================
  Public function declaration
  ============================================================================*/

/**
 * @brief The Sensor API to Register for the LPI TCM BW voting.
 * @note This API should be called only in big image.
 *
 * @param[in] client_name          Ptr to the client name.
 *                                 The max length is 32 characters.
 *
 * @return sns_lpi_tcm_bw_handle   Non Null when registration is successful.
 *                                 NULL when registration fails.
 */

sns_lpi_tcm_bw_handle *sns_lpi_tcm_bw_register(const char *client_name);

/* ---------------------------------------------------------------------------*/
/**
 * @brief The Sensor API to De-register LPI TCM client.
 * @note This function should be called only in big image.
 *
 * @param[in] client_handle      client_handle registered by the client.
 *
 * @retval SNS_RC_SUCCESS        client_handle deregistered successfully.
 * @retval SNS_RC_INVALID_VALUE  Invalid client_handle passed.
 * @retval SNS_RC_FAILED         operation failed.
 *
 */

sns_rc sns_lpi_tcm_bw_deregister(sns_lpi_tcm_bw_handle *client_handle);

/* ---------------------------------------------------------------------------*/

/**
 * @brief Sensor API to vote for LPI TCM BW.
 * @note This API should be called only in big image.
 *
 * @param[in] client_handle       client handle registered by the client.
 * @param[in] bw                  Bandwidth in bytes per second.
 * @param[in] usage_pct           Bandwidth usage in percentage.
 *
 * @retval SNS_RC_SUCCESS         lpi_tcm BW vote is set successfully.
 * @retval SNS_RC_INVALID_VALUE   Invalid client_handle passed.
 * @retval SNS_RC_FAILED          operation failed.
 *
 */

sns_rc sns_lpi_tcm_bw_set(sns_lpi_tcm_bw_handle *client_handle, uint64_t bw,
                          uint32_t usage_pct);

/* ---------------------------------------------------------------------------*/
