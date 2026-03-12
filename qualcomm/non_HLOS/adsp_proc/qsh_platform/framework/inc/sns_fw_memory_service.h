#pragma once
/** ============================================================================
 * @file
 *
 * @brief Framework header for Sensors Memory Service.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_memory_service.h"
#include "sns_osa_lock.h"

/*=============================================================================
  Macros and constants
  ===========================================================================*/
/**
 * @brief Defines the number of sensors supported by the Memory Service.
 * This macro specifies the maximum number of sensors that the Memory Service
 * can manage simultaneously.
 *
 */
#define SNS_MEMORY_SERVICE_NUM_SENSORS (50)

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Structure containing sensor or sensor instance pointer and associated
 * count of outstanding memory allocations.
 *
 */
typedef struct sns_memory_service_sensor_info
{
  void *ptr;          /*!< pointer to the sensor or sensor instance. */
  uint16_t alloc_cnt; /*!< Count of outstanding memory
                       *   allocations for this sensor or sensor instance.
                       */
} sns_memory_service_sensor_info;

/**
 * @brief Private state used by the Framework for the sensor memory
 * service.
 *
 */
typedef struct sns_fw_memory_service
{
  sns_memory_service service;                         /*!< Base memory service 
                                                       *   structure. 
                                                       */
  sns_memory_service_sensor_info                      
      sensor_info_lst[SNS_MEMORY_SERVICE_NUM_SENSORS]; /*!< List of sensor info 
                                                       *   structures. 
                                                       */
  sns_osa_lock lock;                                  /*! Mutex for 
                                                       *  synchronization.
                                                       */
} sns_fw_memory_service;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/
/* fw_memory_service */
/**
 * @brief Initialize the sensor memory service;
 * to be used only by the Service Manager.
 * 
 * @return 
 *  - sns_fw_memory_service* Pointer to the initialized memory service.
 *
 */
sns_fw_memory_service *sns_memory_service_init(void);

/**
 * @brief Cleanup outstanding memory service allocations.
 *
 * @param[in] Pointer to the sensor or sensor instance whose allocations need to
 *                     be cleaned up.
 *  
 * @return
 *  - None.
 *
 */
void sns_memory_service_cleanup(void *ptr);
/* fw_memory_service */
