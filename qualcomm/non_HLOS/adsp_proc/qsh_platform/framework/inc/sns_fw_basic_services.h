#pragma once
/** ============================================================================
 * @file
 *
 * @brief Sensor state and functions only available within the Framework.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_file_service.h"
#include "sns_osa_lock.h"
#include "sns_power_mgr_service.h"
#include "sns_sensor_instance.h"
#include "sns_stream_service.h"
#include "sns_time.h"

/*=============================================================================
  Forward Declarations
  ===========================================================================*/

struct sns_request;
struct sns_fw_request;
struct sns_fw_sensor_event;
struct sns_fw_data_stream;
struct sns_fw_sensor;
struct sns_fw_sensor_instance;
struct sns_thread_mgr_task;
typedef struct sns_sync_com_port_service sns_sync_com_port_service;
typedef struct sns_registration_service sns_registration_service;
typedef struct sns_fw_pwr_rail_service sns_fw_pwr_rail_service;

/*=============================================================================
  Type Definitions
  ===========================================================================*/
/**
 * @brief Private state used by the Framework for the sensor file
 * service.
 *
 */
typedef struct sns_fw_file_service
{
  sns_file_service service; /*!< File service. */

} sns_fw_file_service;

/**
 * @brief Private state used by the Framework for the sensor power manager.
 *
 */
typedef struct sns_fw_power_mgr_service
{
  sns_power_mgr_service service; /*!< Power manager service. */
} sns_fw_power_mgr_service;

/**
 * @brief Private state to be used by the Framework for the Stream Manager.
 *
 */
typedef struct sns_fw_stream_service
{
  sns_stream_service service; /*!< Stream service. */
} sns_fw_stream_service;

/**
 *  @brief Operation Modes supported by Sensor subsystem.
 *
 */
typedef enum
{
  SNS_PWR_MGR_OP_MODE_OFF = 1, /*!< when sensor use case is not active. */
  SNS_PWR_MGR_OP_MODE_ON = 2   /*!< when at least one sensor use case is active.
                                */
} sns_power_mgr_service_op_mode;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Initialize the sensor file service;
 * to be used only by the Service Manager.
 * 
 * @return 
 *  - sns_fw_file_service* Pointer to the initialized file service.
 *
 */
sns_fw_file_service *sns_file_service_init(void);

/* fw_power_mgr_service*/
/**
 * @brief Initialize the sensor power manager service;
 * to be used only by the Service Manager.
 * 
 * @return 
 *  - sns_fw_power_mgr_service* Pointer to the initialized file service.
 *
 */
sns_fw_power_mgr_service *sns_power_mgr_service_init(void);

/**
 * @brief API to vote for sensor operation mode to sensor power manager service.
 * 
 * @note This API is not supported in island mode.
 *
 * @param[in] op_mode Operation mode.
 *
 * @return
 *  - None.
 *
 */
void sns_power_mgr_vote_sensor_op_mode(sns_power_mgr_service_op_mode op_mode);
/* fw_power_mgr_service*/

/* fw_sync_com_port_service */
/**
 * @brief Initialize the sync com port service; to be used only by the
 * Service Manager.
 * 
 * @return 
 *  - sns_sync_com_port_service* Pointer to the com port service.
 *
 */
sns_sync_com_port_service *sns_sync_com_port_service_init(void);

/**
 * @brief Notify Com port service when Q6 Sensor wakeup rate changes.
 *
 * @param[in] wakeup_rate Wakeup rate.
 *
 * @return
 * - None.
 *
 */
void sns_scp_notify_q6_sensor_wr_update(int32_t wakeup_rate);
/* fw_sync_com_port_service */

/* fw_registration_service */
/**
 * @brief Initialize the registration service
 * Used only by the Service Manager.
 * 
 * @return 
 *  - sns_registration_service* Pointer to the registration service.
 * 
 */
sns_registration_service *sns_registration_service_init(void);
/* fw_registration_service */

/* fw_stream_service */
/**
 * @brief Initialize and return a handle to the stream service.
 * 
 * @return 
 *  - sns_fw_stream_service* Pointer to the stream service.
 *
 */
sns_fw_stream_service *sns_stream_service_init(void);

/**
 * @brief Add new event to the stream's event array.
 *
 * @param[inout] stream Pointer the this data stream.
 * @param[in] fw_event  Pointer to the rvent to add (event is located on global 
 *                      circular buffer).
 *
 * @return
 *  -  None.
 *
 */
void sns_stream_service_add_event(struct sns_fw_data_stream *stream,
                                  struct sns_fw_sensor_event *fw_event);

/**
 * @brief Add a pending request to queue to be processed.
 * 
 * @param[inout] stream  Pointer the this data stream
 * @param[in]    request Pointer to the request added to this stream.
 *
 * @return
 *  - None.
 * 
 */
void sns_stream_service_add_request(struct sns_fw_data_stream *stream,
                                    struct sns_request *request);
/* fw_stream_service */

/* fw_pwr_rail_service */
/**
 * @brief Initialize the power rail service; to be used only by the
 * Service Manager.
 * 
 * @return 
 *  - sns_fw_pwr_rail_service* Pointer to the power rail service. 
 *
 */
sns_fw_pwr_rail_service *sns_pwr_rail_service_init(void);

/**
 * @brief Invoke power rail service to vote for all physical power rails.
 * Returns the timestamp of the most recent power rail to turn on in
 * rail_on_timestamp when vote_on is true or 0 if vote_on is false. If the
 * caller isn't a sensor, a dummy reference can be provided for sensor. The
 * sensor struct is never accessed, the address is only used as a unique
 * identifier for tracking clients. If the timestamp isn't needed, NULL can be
 * passed for rail_on_timestamp.
 *
 * @param[in] sensor               sensor reference if called by a sensor or a
 *                                 dummy reference otherwise.
 * @param[in] vote_on              true to vote rails on or false to vote rails
 *                                 off.
 * @param[out] rail_on_timestamp   timestamp of the most recent power rail to
 *                                 turn on if vote_on is true or 0 if vote_on is
 *                                 false.
 *
 * @return
 *  - None.
 *
 */
void sns_pwr_rail_service_vote_physical_rails(sns_sensor *const sensor,
                                              bool vote_on,
                                              sns_time *rail_on_timestamp);

/**
 * @brief Invoke power rail service to devote power rail for which power rail off timer
 * has expired and there are no active client available.
 *
 * @return 
 *  -None.
 *
 */
void sns_pwr_rail_service_devote(void);

/**
 * @brief Removes the given sensor from power rail service's client DB
 * if it exists.
 *
 * @param[in] sensor  Pointer to the sensor.
 *
 * @return
 *  - True  If removed.
 *  - False Otherwise.
 *
 */

bool sns_pwr_rail_svc_remove_client(struct sns_fw_sensor *sensor);
/* fw_pwr_rail_service */