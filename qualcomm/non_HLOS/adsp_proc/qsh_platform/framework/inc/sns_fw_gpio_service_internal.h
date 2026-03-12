#pragma once
/** ============================================================================
 * @file
 *
 * @brief Framework header for Sensors GPIO Service.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

#include <stdint.h>
#include <stdbool.h>

#include "sns_gpio_service.h"

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Forward Declaration for GPIO service.
 *
 */
typedef struct sns_fw_gpio_service sns_fw_gpio_service;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Initialize the GPIO service; to be used only by the Service Manager.
 *
 * @return
 *  - sns_fw_gpio_service* Pointer to the initialized GPIO service.
 *
 */
sns_fw_gpio_service *sns_gpio_service_init(void);
