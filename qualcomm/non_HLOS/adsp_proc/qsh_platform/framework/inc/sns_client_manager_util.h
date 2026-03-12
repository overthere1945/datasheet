/** =============================================================================
 * @file
 *  
 * @brief Client manager utilities.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ============================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include "sns_pb_util.h"
#include "sns_std.pb.h"
#include "sns_std_type.pb.h"

/*=============================================================================
  Defines
  ===========================================================================*/

/*=============================================================================
  Data Type Definitions
  ===========================================================================*/

/**
 * @brief Structure containing parameters for resampling in the client 
 * Manager.
 *
 */
typedef struct sns_cm_resampler_param
{
  float resampled_rate; /*!< Resampled rate of the sensor data. */
  uint8_t rate_type;    /*!< Type of the resampled rate. */
  bool filter;          /*!< Flag indicating whether filtering is enabled. */
  bool event_gated;     /*!< Flag indicating whether resampling is event-gated. */
} sns_cm_resampler_param;

/**
 * @brief Structure containing parameters for thresholds in the client 
 * Manager.
 *
 */
typedef struct sns_cm_threshold_param
{
  uint8_t threshold_type;                /*!< Type of threshold. */
  pb_float_arr_arg *threshold_val_arg;   /*!< Argument for threshold values. */
  uint32_t payload_cfg_msg_id;           /*!< Message ID for the payload configuration. */
  pb_buffer_arg *payload;                /*!< Buffer for the payload. */
} sns_cm_threshold_param;

/*=============================================================================
  Function Definitions
  ===========================================================================*/

/**
 * @brief checks if a valid resampler suid has been stored.
 *
 * @return 
 *  - True  If valid resampler suid is stored.
 *  - False Otherwise.
 *
 */
bool sns_cm_is_valid_resampler_suid(void);

/**
 * @brief checks if a valid threshold suid has been stored.
 *
 * @return 
 *  - True  If valid threshold suid is stored.
 *  - False Otherwise.
 */
bool sns_cm_is_valid_threshold_suid(void);

/**
 * @brief To get the resampler message ID.
 *
 * @return 
 *  - uint32_t Resampler config msg id.
 *
 */
uint32_t sns_cm_util_get_resampler_msg_id(void);

/**
 * @brief To get the Threshold message ID.
 *
 * @return
 *  - uint32_t Threshold config msg id.
 *
 */
uint32_t sns_cm_util_get_threshold_msg_id(void);

/**
 * @brief This function stores Resampler SUID to be used later.
 *
 * @param[in] suid  Pointer to resampler SUID.
 *
 * @return
 *  - None.
 *
 */
void sns_cm_store_resampler_suid(sns_sensor_uid *suid);

/**
 * @brief This function returns Resampler SUID.
 *
 * @param[out] suid  Pointer to resampler SUID.
 *
 * @return 
 * -  None.
 *
 */
void sns_cm_get_resampler_suid(sns_sensor_uid *suid);

/**
 * @brief This function stores Threshold SUID to be used later
 *
 * @param[in] suid  Pointer to threshold SUID.
 *
 * @return 
 *  - None.
 *
 */
void sns_cm_store_threshold_suid(sns_sensor_uid *suid);

/**
 * @brief This function returns Threshold SUID.
 *
 * @param[out] suid  Pointer to threshold SUID.
 *
 * @return
 *  - None.
 *
 */
void sns_cm_get_threshold_suid(sns_sensor_uid *suid);

/**
 * @brief This function encode standard reqeust with given resampler parameters
 * and store it in buffer as encoded message.
 *
 * @param[out] buffer       Pointer to the buffer to store encoded message.
 * @param[in]  buffer_len   Size of buffer.
 * @param[in]  suid         Desired sensor uid to stream.
 * @param[in]  resamp_param Pointer to the resampler configuration.
 * @param[in]  request      Pointer to the sns_std_request configuration.
 *
 * @return
 *  - size_t Size of encoded message.
 *
 */
size_t sns_cm_enc_resamp_req(void *buffer, size_t buffer_len,
                             sns_sensor_uid suid,
                             sns_cm_resampler_param *resamp_param,
                             sns_std_request *request);

/**
 * @brief This function encode standard reqeust with given threshold parameters
 * and store it in buffer as encoded message.
 *
 * @param[out] buffer           Pointer to the buffer to store encoded message.
 * @param[in]  buffer_len       Size of buffer.
 * @param[in]  suid             Desired sensor uid to stream.
 * @param[in]  threshold_param  Pointer to the threshold configuration.
 * @param[in]  request          Pointer to the sns_std_request configuration.
 *
 * @return
 *  - size_t Size of encoded message.
 *
 */
size_t sns_cm_enc_thresh_req(void *buffer, size_t buffer_len,
                             sns_sensor_uid suid,
                             sns_cm_threshold_param *threshold_param,
                             sns_std_request *request);

/**
 * @brief This function encode standard reqeust with given threshold and resampler
 * parameters and store it in buffer as encoded message.
 *
 * @param[out] buffer           Pointer to the buffer to store encoded message.
 * @param[in]  buffer_len       Size of buffer.
 * @param[in]  suid             Desired sensor uid to stream.
 * @param[in]  resamp_param     Pointer to the resampler configuration.
 * @param[in]  threshold_param  Pointer to the threshold configuration.
 * @param[in]  request          Pointer to the sns_std_request configuration.
 *
 * @return
 *  - size_t size of encoded message.
 *
 */
size_t sns_cm_enc_thresh_resamp_req(void *buffer, size_t buffer_len,
                                    sns_sensor_uid suid,
                                    sns_cm_resampler_param *resamp_param,
                                    sns_cm_threshold_param *threshold_param,
                                    sns_std_request *request);
