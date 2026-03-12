#pragma once
/** ============================================================================
 * @file
 *
 * @brief Sensor Instance state only available to the Framework.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include "sns_cstruct_extn.h"
#include "sns_diag_print.h"
#include "sns_fw_diag_types.h"
#include "sns_isafe_list.h"
#include "sns_island_util.h"
#include "sns_osa_lock.h"
#include "sns_printf.h"
#include "sns_sensor_instance.h"

/*==============================================================================
  Forward Declarations
  ============================================================================*/

struct sns_fw_sensor;
struct sns_cluster;

/*==============================================================================
  Type Definitions
  ============================================================================*/

/**
 * @brief A list of clients for a specific Sensor.
 *
 */
typedef struct sns_client_req_list
{
  sns_sensor_uid const *suid;         /*!< Sensor requested by client_requests.
                                       */
  struct sns_cluster *active_cluster; /*!< Active cluster used to publish client
                                       *   events.  Will be set to NULL when the
                                       *   client list changes; at which time a
                                       *   new active cluster will be created
                                       *   and distributed to all clients.
                                       */
  sns_isafe_list client_requests;     /*!< Clients for this Instance, specific
                                       *   to this SUID; see sns_fw_data_stream.
                                       */
  sns_isafe_list_item list_entry;     /*!< Add to sns_fw_sensor_instance::
                                       *   client_req_lists. Each request
                                       *   contains the associated data stream
                                       *   on which the  request was received,
                                       *   and on which all  outgoing events
                                       *   will be sent.
                                       */
  sns_isafe_list_iter iter;           /*!< Iterator used by Sensor within
                                       *   sns_sensor_instance_cb::
                                       *   get_client_request.
                                       *
                                       */
} sns_client_req_list;

typedef struct sns_fw_sensor_instance
{
  sns_sensor_instance instance;      /*!< Base sensor instance structure. */
  sns_isafe_list client_req_lists;   /*!< A list of SUIDs this Instance is
                                      *   presently supporting.  For each, a
                                      *   list of client requests serviced by
                                      *   list this instance. For most Sensors
                                      *   this will contain a single entry,
                                      *   where that  entry contains a list of
                                      *   all client. requests For some Sensors
                                      *   (likely Drivers), this list will
                                      *   contain an entry per associated
                                      *   Sensor. The  configuration of this
                                      *   Sensor Instance may be a logical
                                      *   combination these ClientRequests.
                                      *   @see sns_client_req_list.
                                      */
  struct sns_fw_sensor *sensor;      /*!< Reference to the associated Sensor.
                                      */
  sns_isafe_list_item list_entry;    /*!< Used by Sensor to add to sns_sensor::
                                      *   instances. */
  struct sns_sensor_event *event;    /*!< Event allocated for sensor instance
                                      *   to publish events.
                                      */
  sns_isafe_list data_streams;       /*!< All active data streams incoming to
                                      *   this Sensor Instance.
                                      */
  sns_island_state island_operation; /*!< Used by framework to determine if
                                      *   Sensor instance can run in Island
                                      *   mode. When set to
                                      *   SNS_ISLAND_STATE_IN_ISLAND, instance
                                      *   will be operated in island mode. When
                                      *   set to SNS_ISLAND_STATE_NOT_IN_ISLAND,
                                      *   instance will never be operated in
                                      *   island mode. When set to
                                      *   SNS_ISLAND_STATE_ISLAND_DISABLED,
                                      *   instance will not be operated in
                                      *   island mode but can be set to
                                      *   SNS_ISLAND_STATE_IN_ISLAND at a future
                                      *   time when all data streams associated
                                      *   with instance is available in island
                                      *   mode.
                                      */

  /**
   * @brief State of removal process. Once removal process is started,
   * no further API function calls will be made .
   *
   */
  enum
  {
    SNS_INST_REMOVING_NOT_START = 0, /*!< Instance is active. */
    SNS_INST_REMOVING_START = 1,     /*!< Removal process started. */
    SNS_INST_REMOVING_WAITING = 2,   /*!< Removal process started and waiting
                                      *   all data streaming to complete
                                      *   deinitialization.
                                      */
  } inst_removing;
  _Atomic bool in_use;               /*!< True: If this instance is currently
                                      *    in use by worker thread.
                                      */
  sns_diag_src_instance diag_header; /*!< Diag configuration. */
  sns_cstruct_extn extn;             /*!< C structure extentions. */

} sns_fw_sensor_instance;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief See sns_sensor_cb::create_instance.
 *
 * @note sensor::library::lock must be acquired prior to calling this function.
 * This will already be the case if sns_sensor_instance_init is called within a
 * Sensor API function.
 *
 * @param[in] sensor      Sensor pointer.
 * @param[in] state_len   Sensor state structure length.
 *
 * @return
 *  - sns_sensor_instance* Sensor instnace pointer.
 *
 */
sns_sensor_instance *sns_sensor_instance_init(sns_sensor *sensor,
                                              uint32_t state_len);

/**
 * @brief See sns_sensor_cb::create_instance_v2.
 *
 * @note sensor::library::lock must be acquired prior to calling this function.
 * This will already be the case if sns_sensor_instance_init_v2 is called within
 * a Sensor API function.
 *
 * @param[in] sensor        Sensor pointer.
 * @param[in] state_len     Sensor state structure length.
 * @param[in] state_len_ni  Sensor non-island state structure length.
 *
 * @return
 *   - sns_sensor_instance* Return the referance of the sensor instance.
 *
 */
sns_sensor_instance *sns_sensor_instance_init_v2(sns_sensor *sensor,
                                                 uint32_t state_len,
                                                 uint32_t state_len_ni);

/**
 * @brief Deinit a Sensor Instance; begins the process of removing the Instance.
 * Clean-up all requests, incoming and outgoing data streams.
 *
 * @note - If function returns false, sns_sensor_instance_destroy will be
 * called once all incoming data streams have been closed.
 * - instance::sensor::library::state_lock must be
 * acquired prior to calling this function.
 *
 * @param[in] instance Sensor Instance to be removed.
 *
 * @return
 *  - None.
 *
 */
void sns_sensor_instance_deinit(sns_sensor_instance *instance);

/**
 * @brief Once all incoming data streams to a Sensor Instance have been closed
 * and removed (and hence there are no further pending events), free the
 * instance memory.
 *
 * @param[in] instance Sensor Instance to be removed.
 *
 * @return
 *  - None.
 *
 */
void sns_sensor_instance_delete(sns_fw_sensor_instance *instance);

/**
 * @brief Initialize static state in the sns_fw_sensor_instance module.
 *
 * @return
 * SNS_RC_FAILED  Catastrophic error has occurred; restart Framework
 * SNS_RC_SUCCESS Upon Success.
 *
 */
sns_rc sns_sensor_instance_init_fw(void);

/**
 * @brief Attempt to set the island mode of a Sensor Instance.  Specifying
 * disabled will always succeed; enabled may fail due to other state
 * constraints.
 *
 * @param[in] instance  Instance to be set.
 * @param[in] enabled   Attempt to mark as in Island Mode;
 *                      Mark as temporarily unable to support island mode
 *
 * @return
 *  - True  Upon successful set.
 *  - False Otherwise.
 *
 */
bool sns_sensor_instance_set_island(sns_fw_sensor_instance *instance,
                                    bool enabled);

/**
 * @brief Check if all the data streams for this instance is in island mode.
 *
 * @param[in] instance Instance to be checked.
 *
 * @return
 *  - True  If all data streams for this instance are in island mode.
 *  - False Otherwise.
 *
 */
bool sns_instance_streams_in_island(sns_sensor_instance const *instance);

/**
 * @brief Removes all requests from the given instance.
 *
 * @param[in] instance Instance whose client request lists are to be cleared
 *
 * @return
 *  - int Number of requests removed.
 *
 */
int sns_sensor_instance_remove_all_requests(sns_fw_sensor_instance *instance);
