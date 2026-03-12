#pragma once
/** ============================================================================
 * @file
 *
 * @brief Client-generated configuration request.  Buffer contains a published,
 * backward-compatible structure (possibly QMI-decoded).  Requests may
 * have a version number, but Sensors must appropriately
 * handle older or newer versions.
 *
 * @copyright Copyright (c) 2016-2018,2020 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include <inttypes.h>
#include "sns_isafe_list.h"
#include "sns_request.h"
#include "sns_fw_message_id.h"
/*==============================================================================
  Macros and constants
  ============================================================================*/

/**
 * @brief Sanity check for a request.
 * 
 */
#define SNS_STREAM_SERVICE_REQUEST_SANITY 0x53AC53AC

/*==============================================================================
  Type Definitions
  ============================================================================*/

typedef struct sns_fw_request
{
  sns_request request;                  /*!< Basic request properties. */
  struct sns_fw_data_stream *stream;    /*!< The data stream on which the  
                                         *   request arrived. 
                                         */
  struct sns_sensor_instance *instance; /*!< The sensor instance servicing this 
                                         *   request. 
                                         */
  uint32_t sanity;                      /*!< Sanity Check. */
} sns_fw_request;

/*=============================================================================
  Functions
  ===========================================================================*/

/**
 * @brief Function to validate a request.
 *
 * @param[in] request Request to be validated.
 *
 * @return 
 *  - True  If request is valid. 
 *  - False Otherwise.
 *
 */
bool sns_data_stream_validate_request(sns_fw_request *request);
