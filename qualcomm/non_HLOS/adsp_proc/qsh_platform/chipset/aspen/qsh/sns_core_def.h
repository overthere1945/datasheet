#pragma once

/** ===========================================================================
 * @file
 *
 * @brief
 * This file contains list of supported core Ids
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*=============================================================================
 * Type Definitions
 ============================================================================*/

/*=============================================================================
 * Macros
 ============================================================================*/

/**
 * List of supported core ids
 */
#define SNS_CORE_ID_UNSUPPORTED        0
#define SNS_CORE_ID_ADSP               1
#define SNS_CORE_ID_SDC                2

/**
 * Primary core id is mapped to ADSP
 */
#define SNS_PRIMARY_CORE_ID            SNS_CORE_ID_ADSP

/**
 * Secondary core id is mapped to SDC
 */
#define SNS_SECONDARY_CORE_ID          SNS_CORE_ID_SDC

/*----------------------------------------------------------------------------*/
