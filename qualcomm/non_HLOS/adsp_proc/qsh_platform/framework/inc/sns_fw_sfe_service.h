#pragma once
/**=============================================================================
 * @file
 *
 * @brief Private Framework state for SFE Service
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/**
  ==============================================================================
  Include Files
  ==============================================================================
*/
#include "sns_service.h"

/**
  ==============================================================================
  Forward Declarations
  ==============================================================================
*/
struct sns_service_manager;

/**
  ==============================================================================
  Public Functions
  ==============================================================================
*/

/**
 * @brief Initialize the Sensing Front End service
 *
 * @note To be used only by the Service Manager
 *
 * @return The initialized public SFE service
 *
 */
sns_service *sns_sfe_service_init(struct sns_service_manager *svc_mgr);
