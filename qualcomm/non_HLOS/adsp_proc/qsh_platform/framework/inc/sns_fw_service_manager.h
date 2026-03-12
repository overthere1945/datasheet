#pragma once
/** ============================================================================
 * @file
 *
 * @brief Private state used by the Framework in conjunction with 
 * sns_service_manager.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include "sns_rc.h"
#include "sns_service.h"
#include "sns_service_manager.h"

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Manages services provided by the Framework.
 * 
 */
typedef struct sns_fw_service_manager
{
  sns_service_manager service;                    /*!< Base service manager 
                                                   *   structure. 
                                                   */
  sns_service *services[SNS_SERVICE_TYPE_LENGTH]; /*!< Array of all known 
                                                   *   services.  Populated at 
                                                   *   boot-up by the Framework.
                                                   */
} sns_fw_service_manager;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Initialize the Service Manager, and populate the list of known 
 * services.
 * 
 * @return
 *  - SNS_RC_SUCCESS Success.
 *  - SNS_RC_FAILED  Failure.
 */
sns_rc sns_service_manager_init(void);

/**
 * @brief Return a pointer to the service manager.
 *
 * @return 
 *  - sns_fw_service_manager* Service manager pointer.
 *
 */
sns_fw_service_manager *sns_service_manager_ref(void);
