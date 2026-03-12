#pragma once
/** ============================================================================
 * @file
 *
 * @brief Manages Diagnostic Services for Sensors and Sensor Instances.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_diag.pb.h"
#include "sns_diag_print.h"
#include "sns_diag_service.h"
#include "sns_fw_diag_types.h"
#include "sns_fw_sensor.h"
#include "sns_log.h"
#include "sns_log_codes.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_time.h"

/*==============================================================================
  Macros and Constants
  ============================================================================*/
#ifdef SNS_ISLAND_INCLUDE_DIAG
#define DIAG_ISLAND_TEXT      SNS_SECTION(".text.sns")
#define DIAG_ISLAND_DATA      SNS_SECTION(".data.sns")
#define DIAG_ISLAND_RODATA    SNS_SECTION(".rodata.sns")
#define SNS_DIAG_OSA_MEM_TYPE SNS_OSA_MEM_TYPE_ISLAND
#define SNS_DIAG_HEAP_TYPE    SNS_HEAP_ISLAND
/**
 * @brief Condition required only for non-island diag.
 * 
 */
#define SNS_NON_ISLAND_DIAG_CHECK(x)

/**
 * @brief Diag allows maximum 1012 bytes including diag log header in island.
 * Diag log header packet length is 12 bytes and diag service additional fields
 * length is ~53. The maximum permitted log size that sensors - algos / drivers
 * are allowed to allocate 946 bytes in island and 1600 bytes in non-island.
 *
 */
#define SNS_DIAG_IN_ISLAND_MAX_SENSOR_LOG_SIZE (946) // bytes
#else
#define DIAG_ISLAND_TEXT
#define DIAG_ISLAND_DATA
#define DIAG_ISLAND_RODATA
#define SNS_DIAG_OSA_MEM_TYPE                  SNS_OSA_MEM_TYPE_NORMAL
#define SNS_DIAG_HEAP_TYPE                     SNS_HEAP_MAIN
#define SNS_NON_ISLAND_DIAG_CHECK(x)           x
/**
 * @brief For non-island diag, max log packet size is '0' when system is in 
 * island.
 * 
 */
#define SNS_DIAG_IN_ISLAND_MAX_SENSOR_LOG_SIZE 0
#endif
/*==============================================================================
  Forward Declarations
  ============================================================================*/

struct sns_sensor;
struct sns_fw_sensor_instance;
struct sns_fw_request;
struct sns_sensor_uid;
struct sns_fw_diag_service;

/*=============================================================================
  Externs
  ===========================================================================*/
extern sns_diag_datatype_config *default_datatype;

/*=============================================================================
  Type definitions
  ===========================================================================*/
/**
 * @brief Diag log mask change notify cb function.
 *
 * @param[in] sensor Sensor Pointer.
 *
 * @return
 *  - None.
 * 
 */
typedef void (*sns_diag_log_mask_change_cb)(sns_sensor *sensor);

/**
 * @brief Diag msg mask change notify cb function.
 *
 * @param[in] sensor Sensor Pointer.
 *
 * @return
 *  - None.
 * 
 */
typedef void (*sns_diag_msg_mask_change_cb)(sns_sensor *sensor);

/*=============================================================================
  Public functions
  ===========================================================================*/
/**
 * @brief Register to get notification cb when Diag log mask is changed.
 * This function should be called only in big image.
 * Sensor can call sns_diag_get_log_status() to get status of the diag log 
 * codes.
 *
 * @param[in] sensor Sensor Pointer.
 * @param[in] cb     Callback function to get notification.
 *
 * @return 
 *  - SNS_RC_SUCCESS Registration is successful.
 *  - SNS_RC_FAILED  Registration failed.
 *
 */
sns_rc sns_register_diag_log_mask_change_cb(sns_sensor *sensor,
                                            sns_diag_log_mask_change_cb cb);
/* ---------------------------------------------------------------------------*/

/**
 * @brief De-register callback to get notification when Diag log mask is changed
 * This function should be called only in big image.
 *
 * @param[in] sensor Sensor Pointer.
 *
 * @return
 *  - SNS_RC_SUCCESS De-registration is successful.
 *  - SNS_RC_FAILED  De-registration failed.
 *
 */
sns_rc sns_deregister_diag_log_mask_change_cb(sns_sensor *sensor);
/* ---------------------------------------------------------------------------*/

/**
 * @brief Register to get notification cb when Diag msg mask is changed.
 * This function should be called only in big image.
 * Sensor can call sns_diag_get_msg_status() to get status of the diag msg.
 *
 * @param[in] sensor Sensor Pointer.
 * @param[in] cb     Callback function to get notification.
 *
 * @return
 *  - SNS_RC_SUCCESS Registration is successful.
 *  - SNS_RC_FAILED  Registration failed.
 *
 */
sns_rc sns_register_diag_msg_mask_change_cb(sns_sensor *sensor,
                                            sns_diag_msg_mask_change_cb cb);
/* ---------------------------------------------------------------------------*/

/**
 * @brief De-register callback to get notification when Diag msg mask is changed
 * This function should be called only in big image.
 *
 * @param[in] sensor   Sensor Pointer
 * 
 * @return
 *  - SNS_RC_SUCCESS De-registration is successful.
 *  - SNS_RC_FAILED  De-registration failed.
 *
 */
sns_rc sns_deregister_diag_msg_mask_change_cb(sns_sensor *sensor);
/* ---------------------------------------------------------------------------*/

/**
 * @brief Get status of Diag Message Id by passing Diag MSG SSID & MSG level
 * This function should be called only in big image.
 *
 * @param[in] msg_ssid  Subsystem Diag message id.
 * @param[in] msg_level Message Levels.
 *
 * @return 
 *  - True - Diag message id is enabled.
 *  - False - Diag message id is disabled.
 *
 */
bool sns_diag_get_message_status(sns_fw_diag_service_msg_ssid msg_ssid,
                                 sns_fw_diag_service_msg_level msg_level);
/* ---------------------------------------------------------------------------*/

/**
 * @brief Get the sizes of encoded diagnostics data for different components. 
 * 
 * @param type Type of encoded diagnostics data.
 *
 * @return
 *  - size Get the size.
 * 
 */
size_t sns_diag_get_preencoded_size(sns_fw_diag_enc_size type);

/**
 * @brief Records the data_type provided by this sensor.
 *
 * @param[in] sensor Registering with diag.
 *
 * @return
 *  - None.
 *
 */
void sns_diag_register_sensor(struct sns_fw_sensor *sensor);

/**
 * @brief printf() style function for printing a debug message from
 * sensor context.
 * 
 * @note The ssid for each Sensor can be accessed only using the macro
 * SNS_DIAG_SSID. This macro is defined for each sensor through scons by either
 * directly setting CPPDEFINES or by passing diag_ssid through AddSSCSU.
 *
 * @param[in] ssid    DIAG Subsystem ID.
 * @param[in] sensor  pointer the sns_sensor printing this message.
 * @param[in] prio    priority (level) of this message.
 * @param[in] file    file name string.
 * @param[in] line    file line number of the caller.
 * @param[in] format  printf() style format string.
 *
 * @return
 *  - None.
 *
 */
void sns_diag_sensor_sprintf(uint16_t ssid, const sns_sensor *sensor,
                             sns_message_priority prio, const char *file,
                             uint32_t line, const char *format, ...);

/**
 * @brief Publish a framework log packet. Log packet memory is always freed.
 *
 * @param[in] log_id                The log packet ID.
 * @param[in] payload_size          Size of the log packet payload.
 * @param[in] payload               Pointer to the log packet payload.
 * @param[in] payload_encoded_size  Size of the encoded payload.
 * @param[in] payload_encode_cb     Call back function to encode payload.
 *
 * @return 
 *  - SNS_RC_SUCESS On Success.
 *  - SNS_RC_FAILED On Failure.
 *
 */
sns_rc sns_diag_publish_fw_log(sns_fw_diag_service_log_id log_id,
                               uint32_t payload_size, void *payload,
                               uint32_t payload_encoded_size,
                               sns_diag_encode_log_cb payload_encode_cb);

/**
 * @brief Publish a Sensor log packet. Log packet memory is always freed.
 * Publish a Sensor log packet. Log packet memory is always freed.
 * 
 * @param[in] instance_id           Unique identifier of the sensor instance,
 *                                  set to 0 if log is not pertinent to a
 *                                  specific instance.
 * @param[in] sensor_uid            UID of the sensor generating the log,
 *                                  can be set to NULL in instances when sensor 
 *                                  UID is unknown.
 * @param[in] data                  type of the sensor generating the log.
 * @param[in] log_id                The log packet ID.
 * @param[in] payload_size          Size of the log packet payload.
 * @param[in] payload               Pointer to the log packet payload.
 * @param[in] payload_encoded_size  Size of the encoded payload.
 * @param[in] payload_encode_cb     Call back function to encode payload.
 *
 * @return
 *  - SNS_RC_SUCCESS On Success.
 *  - SNS_RC_FAILED  On Failure.
 *
 */
sns_rc sns_diag_publish_sensor_log(uintptr_t instance_id,
                                   struct sns_sensor_uid const *sensor_uid,
                                   char const *data_type,
                                   sns_fw_diag_service_log_id log_id,
                                   uint32_t payload_size, void *payload,
                                   uint32_t payload_encoded_size,
                                   sns_diag_encode_log_cb payload_encode_cb);

/**
 * @brief Check Diag log code status before allocating / submitting log.
 *
 * @param[in] log_id Diag service log id.
 *
 * @return 
 *  - True  If Diag log code is enabled. 
 *  - False Otherwise.
 *
 */
bool sns_diag_get_log_status(sns_fw_diag_service_log_id log_id);

/**
 * @brief Allocate memory for a log packet.
 *
 * @param[in] log_size Size of the log packet in bytes.
 * @param[in] log_id   Log packet ID.
 *
 * @return 
 * Void* Pointer to allocated memory.
 *
 */
void *sns_diag_log_alloc(uint32_t log_size, sns_fw_diag_service_log_id log_id);

/**
 * @brief Initializes the diag service.
 *
 * @return 
 *  - sns_fw_diag_service* pointer to the diag service.
 *
 */
struct sns_fw_diag_service *sns_diag_service_init(void);

/**
 * @brief De-initializes the diag service.
 *
 * @return
 *  - None.
 *
 */
void sns_diag_service_deinit(void);

/**
 * @brief Apply the diag filter for all data types.
 *
 * @param[in] enable - True to enable diag.
 *                   - False to disable diag.
 *
 * @return
 * - None.
 *
 */
void sns_diag_set_filter_all(bool enable);

/**
 * @brief Apply the diag filter for a given data_type.
 *
 * @param[in] data_type Whose filter to be updated.
 * @param[in] enable    - True to enable diag.
 *                      - False to disable diag.
 *
 * @return 
 *  - SNS_RC_SUCCESS On Sucess.
 *  - SNS_RC_FAILED  On Failure.
 *
 */
sns_rc sns_diag_set_filter(const char *data_type, bool enable);

/**
 * @brief Create a inst debug config object and populate it as appropriate.
 *
 * @param[in] instance Sensor Instance which may need a diag_config.
 * @param[in] sensor   Sensor reference.
 *
 * @return
 *  - SNS_RC_SUCCESS On success.
 *
 */
sns_rc sns_diag_sensor_instance_init(struct sns_fw_sensor_instance *instance,
                                     struct sns_fw_sensor *sensor);

/**
 * @brief Returns debug configuration for an instance corresponding to
 * a particular SUID.
 *
 * @param[in] fw_instance reference to the Sensor Instance
 * @param[in] sensor_uid  Sensor UID for sensor
 *
 * @return 
 *  - sns_diag_instance_config* Pointer to the diag config instance.
 *
 */
sns_diag_instance_config *
sns_diag_find_instance_config(struct sns_fw_sensor_instance *instance,
                              struct sns_sensor_uid const *sensor_uid);

/**
 * @brief Free any state allocated by the DIAG Service within a Senosr Instance.
 *
 * @param[in] instance Instance in which to clear/free DIAG state.
 *
 * @return
 *  - None.
 *
 */
void sns_diag_sensor_instance_deinit(struct sns_fw_sensor_instance *instance);

/**
 * @brief Submit a log to be encoded asynchrounously at a later time.
 * The log data is stored in a circular buffer. Data may be lost
 * in the circular buffer.
 *
 * @note Use this call to avoid deadlocks with ISLAND_EXIT().
 *
 * @param[in] log   Log to be submitted asynchronusly.
 *
 * @return 
 *  - SNS_RC_SUCCESS if successful.
 *  - SNS_RC_FAILED otherwise.
 *
 */
sns_rc sns_diag_submit_async_log(sns_diag_async_log_item log);

/**
 * @brief Consume all asynchronously submitted logs and add them to
 * the sns_diag_list. Unless force is set to true, this function
 * might refuse to consume logs if there are not enough logs.
 *
 * @param[in] force  If true, force consuming all async logs.
 *
 * @return
 *  - None.
 */
void sns_diag_consume_async_logs(bool force);

/**
 * @brief Pre-initialize diag service to enable logging during the boot
 * process.
 *
 * @return
 *  - SNS_RC_FAILED   On Failure.
 *  - SNS_RC_Success  On Success.
 *
 */
sns_rc sns_diag_fw_preinit(void);

/**
 * @brief Deinitialize diag configuration infomration associated with
 * this sensor.
 *
 * @note Diag deinit uses attributes. Deinitialize diag before
 * deinitializing the attribtes
 *
 * @param[in] sensor  Sensor to be deinitialized.
 *
 * @return
 *  - None.
 *
 */
void sns_diag_svc_sensor_deinit(sns_fw_sensor *sensor);

/**
 * @brief Sets the signal in diag thread that consumes all the
 * log packets for island transition debug structure.
 *
 * @return 
 *  - None.
 *
 */
void sns_diag_consume_island_transition_debug_logs(void);

/**
 * @brief Frees the memory allocated for the log.
 * 
 * @return
 *  - None.
 *
 */
void sns_diag_log_free(sns_diag_log_info *log_info);

/**
 * @brief Provides teh info present within payload.
 *
 * @param[in] payload Pointer to the payload.
 *
 * @return
 * - sns_diag_log_info* Pointer to the log info.
 *
 */
sns_diag_log_info *sns_diag_get_log_info(void *payload);