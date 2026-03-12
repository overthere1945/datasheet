#pragma once
/** ============================================================================
 * @file
 *
 * @brief Type definitions for diag service.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ==========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <stdbool.h>
#include <stdint.h>
#include "sns_cstruct_extn.h"
#include "sns_diag.pb.h"
#include "sns_diag_print.h"
#include "sns_isafe_list.h"
#include "sns_list.h"
#include "sns_log.h"
#include "sns_sl_list.h"
#include "sns_time.h"

/*==============================================================================
  Macros and Constants
  ============================================================================*/
/**
 * @brief Maximum length of the name of data type.
 *
 */
#define SNS_SENSOR_DATATYPE_NAME_LEN 32

/**
 * @brief Maximum number of supported sensor state log packets.
 *
 */
#define SNS_DIAG_SENSOR_STATE_LOG_NUM 3

/**
 * @brief Length of a uint64_t buffer allocated for the log packet payload.
 *
 */
#define SNS_LOG_PKT_UINT64_BUFF_SIZE (200)

/**
 * @brief Length of a buffer used for a log in sns_diag_async_log_item.
 *
 */
#define SNS_DIAG_ASYNC_CIRC_BUFFER_SIZE 128

#define SNS_DIAG_SERVICE_VERSION 2

/*=============================================================================
  Type definitions
  ===========================================================================*/

struct sns_sensor_uid;

/**
 * @brief Supported Log Packet Types in the diag service.
 *
 */
typedef enum
{
  SNS_LOG_CLIENT_API_REQUEST = 0,        /*!< Log ID for client API request.
                                          */
  SNS_LOG_CLIENT_API_RESPONSE = 1,       /*!< Log ID for client API response.
                                          */
  SNS_LOG_CLIENT_API_EVENT = 2,          /*!< Log ID for client API event. */
  SNS_LOG_SENSOR_API_REQUEST = 3,        /*!< Log ID for sensor API request. */
  SNS_LOG_SENSOR_API_EVENT = 4,          /*!< Log ID for sensor API event. */
  SNS_LOG_SENSOR_STATE_RAW_INT = 5,      /*!< Log ID for raw internal sensor
                                          *   state.
                                          */
  SNS_LOG_SENSOR_STATE_INTERNAL_INT = 6, /*!< Log ID for internal internal
                                          *   sensor state.
                                          */
  SNS_LOG_SENSOR_STATE_RAW_EXT = 7,      /*!< Log ID for raw external sensor
                                          *   state.
                                          */
  SNS_LOG_SENSOR_STATE_INTERNAL_EXT = 8, /*!< Log ID for internal external
                                          *   sensor state.
                                          */
  SNS_LOG_SENSOR_STATE_INTERRUPT = 9,    /*!< Log ID for sensor interrupt
                                          *   state.
                                          */
  SNS_LOG_INSTANCE_MAP = 10,             /*!< Log ID for sensor instance map.
                                          */
  SNS_LOG_ISLAND_TRANSITION = 11,        /*!< Log ID for island transition. */
  SNS_LOG_ISLAND_EXIT_VOTE = 12,         /*!< Log ID for island exit vote. */
  SNS_LOG_MEMORY_UTILIZATION = 13,       /*!< Log ID for memory utilization. */
  SNS_LOG_RESERVED_ID = 14,              /*!< Reserved */
  SNS_LOG_ES_LOG = 15,                   /*!< Log ID for event service log. */
  SNS_LOG_QSOCKET_CLIENT_API_REQ =
      16, /*!< Log ID for qsocket client API Request. */
  SNS_LOG_QSOCKET_CLIENT_API_RESP =
      17, /*!< Log ID for qsocket client API Response. */
  SNS_LOG_QSOCKET_CLIENT_API_EVENT =
      18,        /*!< Log ID for qsocket client API Event. */
  SNS_LOG_MAX_ID /*!< Marker to indicate the last valid
                  *   log ID. */
} sns_fw_diag_service_log_id;

/**
 * @brief Supported MSG SSIDs in the diag service.
 *
 */
typedef enum sns_fw_diag_service_msg_ssid
{
  SNS_MSG_SSID_SNS = 0,            /*!< Subsystem ID for the primary SNS
                                    *   module.
                                    */
  SNS_MSG_SSID_SNS_FRAMEWORK = 1,  /*!< Subsystem ID for the SNS
                                    *   Framework.
                                    */
  SNS_MSG_SSID_SNS_PLATFORM = 2,   /*!< Subsystem ID for the SNS
                                    *   Platform layer.
                                    */
  SNS_MSG_SSID_SNS_SENSOR_INT = 3, /*!< Subsystem ID for internal sensors.
                                    */
  SNS_MSG_SSID_SNS_SENSOR_EXT = 4, /*!< Subsystem ID for external sensors.
                                    */
  SNS_MSG_SSID_MAX_ID              /*!< Marker to indicate the last valid
                                    *   SSID.
                                    */
} sns_fw_diag_service_msg_ssid;

/**
 * @brief Supported MSG levels in the diag service.
 */
typedef enum sns_fw_diag_service_msg_level
{
  SNS_MSG_LOW = 0,   /*!< Low-level informational message. */
  SNS_MSG_MED = 1,   /*!< Medium-level informational message. */
  SNS_MSG_HIGH = 2,  /*!< High-level informational message. */
  SNS_MSG_ERROR = 3, /*!< Error message indicating a problem. */
  SNS_MSG_FATAL = 4, /*!< Fatal error message indicating a severe problem. */
  SNS_MSG_MAX_LEVEL  /*!< Marker to indicate the last valid message level. */
} sns_fw_diag_service_msg_level;

/**
 * @brief Enumeration representing the data type for encoded diagnostics data
 * for different components.
 *
 */
typedef enum
{
  SNS_DIAG_ENC_SIZE_SENSOR_API_HDR = 0,   /*!< Size of the encoded sensor API
                                           *   header.
                                           */
  SNS_DIAG_ENC_SIZE_CLIENT_API_HDR = 1,   /*!< Size of the encoded client API
                                           *   header.
                                           */
  SNS_DIAG_ENC_SIZE_ISLAND_TRANSITION = 2 /*!< Size of the encoded island
                                           *   transition data.
                                           */
} sns_fw_diag_enc_size;

/**
 * @brief Packed structure representing a diagnostic log.
 *
 */
typedef SNS_PACK(struct)
{
  sns_log_hdr_type log_hdr; /*!< Header for log entry. */
  uint64_t buffer[1];       /*!< Buffer to hold data. */
}
sns_diag_log;

/**
 * @brief Represent the type of diagnostic logs.
 *
 */
typedef enum
{
  SNS_DIAG_LOG_TYPE_SENSOR = 0, /*!< Sensor type diagnostic logs. */
  SNS_DIAG_LOG_TYPE_FW = 1      /*!< Header for log entry. */
} sns_diag_log_type;

/**
 * @brief Unencoded diag log header, stores all the information
 * necessary to create the final encoded log packet
 * before submission to Core diag.
 *
 */
typedef struct sns_diag_log_info
{
  struct
  {
    sns_isafe_list_item list_entry; /*!< sns_diag_list. */
    uint64_t log_time_stamp;
    uint32_t payload_encoded_size;
    uint32_t sanity;
    uint32_t instance_id;
    sns_diag_encode_log_cb payload_encode_cb;
    sns_std_suid const *suid; /*!< SUID of this source. */
    char const *data_type;
    uint16_t diag_log_code;
    uint16_t log_id;
    uint16_t payload_alloc_size; /*!< allocated payload size. */
    uint16_t payload_size;       /*!< actual payload size. */
    sns_diag_log_type log_type;
  } info;
  uint8_t payload[]; /*!< unencoded payload. */
} sns_diag_log_info;

typedef struct
{
  /**
   * @brief Log data. Data stored here could be either the unencoded log packet
   * message  or any suitable data from which the encoded log packet can be
   * generated.
   *
   */
  uint8_t payload[SNS_DIAG_ASYNC_CIRC_BUFFER_SIZE]; /*!< Payload*/
  size_t payload_size;                              /*!< size of payload*/
  size_t encoded_size;                              /*!< Size of the encoded
                                                     *   payload
                                                     */
  sns_diag_encode_log_cb payload_encode_cb;         /*!< Callback to encode the
                                                     *   payload
                                                     */
  sns_time timestamp;                               /*!< Timestamp of the log
                                                     *   packet
                                                     */
  sns_fw_diag_service_log_id log_id;                /*!< Log ID*/
} sns_diag_async_log_item;

/**
 * @brief Diag filter settings for a datatype.
 *
 */
typedef struct
{
  sns_sl_list_item datatypes_list_entry; /*!< Entry into the list of known data
                                          *   types.
                                          */
  char datatype[SNS_SENSOR_DATATYPE_NAME_LEN]; /*!< ata type. */
  sns_diag_vprintf_fptr vprintf_fptr; /*!< Print function used for this data
                                       *   type.
                                       */
  bool enabled;                       /*!< If diag features enabled for this
                                       *   datatype.
                                       */
} sns_diag_datatype_config;

/**
 * @brief Sensor's diag configuration which includes filter settings of diag
 * logs and F3 messages.
 *
 */
typedef struct
{
  const struct sns_sensor_uid *suid; /*!< SUID of this source. */
  sns_diag_datatype_config *config;  /*!< Datatype config that maps to the above
                                      *   SUID.
                                      */
  uint32_t debug_ssid;               /*!< Debug SSID used by this source. */

} sns_diag_sensor_config;

/**
 * @brief Instance diag configuration per sensor along with log packets.
 *
 */
typedef struct
{
  const struct sns_sensor_uid *suid; /*!< SUID of this source. */
  sns_diag_datatype_config *config;  /*!< Datatype config that maps to the above
                                      *   SUID.
                                      */
  uint32_t debug_ssid;               /*!< Debug SSID used by this source. */
  /**
   * @brief Log Packets allocated for an instance.
   *
   */
  struct sns_diag_log_info *alloc_log[SNS_DIAG_SENSOR_STATE_LOG_NUM];
} sns_diag_instance_config;

/**
 * @brief  Diag meta information stored in a sensor instance.
 *
 */
typedef struct
{
  uint8_t datatypes_cnt;     /*!< Number of datatypes supported by instance. */
  uint8_t cstruct_enxtn_idx; /*!< Index used to obtain the array of source
                              *   configs. */
} sns_diag_src_instance;
