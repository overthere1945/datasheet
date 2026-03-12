#pragma once
/** =============================================================================
 * @file
 *
 * @brief Represents a Client Sensor <-> Service Sensor connection.
 *
 * @note All Data Stream functions may only be called while holding the data
 * stream owner's library_lock.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include <inttypes.h>
#include <stdatomic.h>

#include "sns_data_stream.h"
#include "sns_fw_attribute_service.h"
#include "sns_island_util.h"
#include "sns_list.h"
#include "sns_osa_lock.h"
#include "sns_thread_manager.h"

/*=============================================================================
  Forward Declarations
  ===========================================================================*/

struct sns_cluster;
struct sns_fw_request;
struct sns_fw_sensor_event;
struct sns_fw_sensor_instance;
struct sns_fw_sensor;
struct sns_sensor_library;
struct sns_sensor_uid;

/*=============================================================================
  Preprocessor Definitions
  ===========================================================================*/

/**
 * @brief Quantity of clusters per cluster list; smaller is better to avoid 
 * internal fragmentation; larger is better to avoid linked-list overhead.
 * 
 */
#define SNS_DATA_STREAM_CLUSTER_CNT 5

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief A set of clusters with pending data for a data stream.  Note that this
 * design is simply an optimization over having a linked-list of clusters
 * directly (save linked-list overhead).
 *
 */
typedef struct sns_cluster_list
{
  sns_list_item list_entry;     /*!< Entry for the clusters list. */
  struct sns_cluster *clusters[SNS_DATA_STREAM_CLUSTER_CNT]; /*!< Clusters 
                                                              *   array. 
                                                              */
  uint8_t read_idx; /*!< Oldest cluster still in-use by this stream; if greater 
                     *   than write_idx, it implies that all clusters on this 
                     *   stream are exhausted, and there is no further pending 
                     *   events. 
                     */
  uint8_t write_idx; /*!< Within this list, the cluster with the newest events.
                      */
} sns_cluster_list;

/**
 * @brief Data steam structure. 
 * 
 * @note Sensor Instances cannot be moved to/from Island while it has
 * outstanding events pending processing by its clients.
 *
 */
typedef struct sns_fw_data_stream
{
  sns_data_stream stream;                /*!< Data stream.*/
  sns_list clusters;                     /*!< Clusters with pending data to 
                                          *   consume; type of sns_cluster_list.
                                          */
  sns_isafe_list_item list_entry_client; /*!< List entry for the dst_instance::
                                          *   streams or dst_sensor:streams 
                                          *   list. This is the creator/owner of
                                          *   the stream, and receives all 
                                          *   events.
                                          */
  sns_isafe_list_item list_entry_source; /*!< List entry for the client requests
                                          *   supported by a source instance.
                                          */
  struct sns_fw_sensor *data_source;     /*!< Sensor for the source of this 
                                          *   data.
                                          */
  struct sns_sensor_library *source_library;
  struct sns_fw_request *client_request; /*!< Client-generated configuration 
                                          *   request; Protected by 
                                          *   data_source->library->state_lock.
                                          */
  sns_osa_lock stream_lock;              /*!< Protects stream meta data.  Cannot  
                                          *   acquire while holding 
                                          *   event_service.lock.
                                          */
  struct sns_fw_sensor_instance *dst_instance; /*!< The destination of this data
                                                *   stream; Will not be modified
                                                *   until this structure is 
                                                *   destroyed (always done while
                                                *   holding data_source->
                                                *   library->state_lock).
                                                */
  struct sns_fw_sensor *dst_sensor;
  void* remote_stream;  /*!< Pointer to the remote sensing hub connection
                         *   associated with this stream*/
  _Atomic unsigned int unprocessed_events; /*!< Whether the data stream has been
                                            *   processed since last added to 
                                            *   task list; Must be atomically 
                                            *   read/written.
                                            */
  uint16_t event_cnt; /*!< How many unprocessed events are pending on the 
                       *   buffer.
                       */
  uint16_t block_idx; /*!< Current read block index; may be UINT16_MAX if 
                       *   event_cnt is 0 and there is not an active cluster in 
                       *   the cluster list.
                       */
  uint8_t read_off;   /*!< Read offset into the current block (block_idx);
                       * use SNS_BOFF_* macros to modify/access.
                       */
  /**
   * @note Protected by events_lock.
   * 
   */
  enum
  {
    SNS_DATA_STREAM_AVAILABLE = 0,     /*!< Data Stream has been created and is 
                                        *   available for use.
                                        */
    SNS_DATA_STREAM_WAIT_SRC = 1,      /*!< The client has requested that this  
                                       *    data stream be removed; waiting on  
                                       *    the data source to acknowledge 
                                       *    receipt.
                                       */
    SNS_DATA_STREAM_WAIT_RECEIPT = 2, /*!< The "Destroy Complete" event has been
                                       *   sent, and once received, this data 
                                       *   stream is ready to be freed and 
                                       *   cleared.
                                       */
  } removing;
  /**
   * @brief 
   * - 1. Source Sensor is available in Island Mode.
   * - 2. Destination Sensor or Sensor Instance is available in Island Mode.
   * - 3. All event_bins are available in island mode.
   * - 4. client_request is available in island mode.
   * - 5. This data stream is available in island mode.
   *
   */
  sns_island_state island_operation;
  sns_tmgr_task_prio event_priority; /*!< Events/Requests sent on this stream 
                                      *   should be treated with the given
                                      *   priority.
                                      */
  sns_tmgr_task_prio req_priority; /*!< Data stream.*/
  bool island_req; /*!< Whether requests sent on this stream should be allocated
                    *   in island.*/
  uint32_t stream_integrity; /*!< Stream Integrity checksum; must be at end of 
                              *   structure Purpose: gracefully catch and handle
                              *   Sensor's errors.
                              */
  bool is_dynamic_lib; /*!< Flag to check if source_library is dynamic or not.
                        * In some corner cases the the same flag which is within 
                        * source_library returns FALSE for dynamic library.
                        */
} sns_fw_data_stream;

/**
 * @brief Specifies the type of data stream to be created.
 * 
 */
typedef struct sns_find_sensor_arg
{
  sns_sensor_uid *suid;               /*!< Pointer to the SUID of the sensor. */ 
  struct sns_fw_sensor *sensor;       /*!< Pointer to the sensor. */
  struct sns_remote_sensor *remote_sensor; /*!<Pointer to the remote sensor */
  struct sns_sensor_library *library; /*!< Pointer to the sensor library. */
  char data_type[32];                 /*!< datatype of the sensor. */
  sns_attr_priority priority;         /*!< Priority. */
  bool available;                     /*!< True if sensor is available for 
                                       *   clients.
                                       */
  bool island_req;                    /*!< True if requests to this sensor 
                                       *    should be sent in island.
                                       */
} sns_find_sensor_arg;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/
/**
 * @brief Allocate and initialize a new data stream object.
 * Exactly one of dst_sensor or dst_instance may be non-NULL.
 * 
 * @param[in]     dst_sensor   Pointer to the destination sensor.
 * @param[in]     dst_instance Pointer to the destination sensor instance.
 * @param[in]     src_sensor   Pointer to the source sensor.
 * @param[inout]  data_stream  Pointer to data stream.
 * 
 * @return
 *  - SNS_RC_SUCCESS On Sucess.
 *  - SNS_RC_NOT_AVAILABLE - Sensor has not yet been marked as available.
 *
 */
sns_rc data_stream_init(struct sns_fw_sensor *dst_sensor,
                        struct sns_fw_sensor_instance *dst_instance,
                        sns_find_sensor_arg *src_sensor,
                        sns_fw_data_stream **data_stream);

/**
 * @brief Function called by a client to destroy a data stream.  The stream must
 * be independently removed from the client's list.
 *
 * @note This request will be handled ansynchronously; the client is
 * guaranteed to receive no further events after this function call returns.
 *
 * @param[inout]  stream  Pointer to data stream.
 *
 * @return
 *  - None.
 *
 */
void sns_data_stream_deinit(struct sns_fw_data_stream *stream);

/**
 * @brief Handle receipt of the "destroy complete" event.  Receipt of this event
 * indicates that no further events will be received on this connection,
 * and the framework may safely free the stream and its remaining state.
 *
 * @note if the client instance is in the process of being removed, check
 * if this is the final stream in its list.  if so, also remove and free that
 * instance.
 *
 * @param[inout]  stream  Pointer to data stream.
 *
 * @return
 * SNS_RC_INVALID_STATE Client sensor or instance is freed
 * SNS_RC_SUCCESS       On Sucess.
 *
 */
sns_rc sns_data_stream_destroy_complete(struct sns_fw_data_stream *stream);

/**
 * @brief Validate a data stream
 *
 * @param[inout] stream Data stream to be validated.
 *
 * @return 
 *  - True if stream is a valid data stream.
 *  - False otherwise.
 *
 */
bool sns_data_stream_validate(sns_fw_data_stream *stream);

/**
 * @brief See sns_data_stream_api::peek_input.
 *
 * @param[inout] stream Pointer to data stream.
 *
 * @return
 *  - sns_fw_sensor_event* Peek event on the stream.
 *
 */
struct sns_fw_sensor_event *
sns_data_stream_peek_input(sns_fw_data_stream *stream);

/**
 * @brief sns_data_stream_api::get_next_input.
 * 
 * @param[inout] stream Pointer to data stream.
 *
 * @return 
 *  - sns_fw_sensor_event* Next event on the stream.
 * 
 */
struct sns_fw_sensor_event *
sns_data_stream_get_next_input(sns_fw_data_stream *stream);

/**
 * @brief See sns_data_stream_api::initiate_flush.
 * 
 * @param[inout] stream Pointer to data stream.
 * 
 * @return 
 *  - SNS_RC_SUCCESS On Success.
 *  
 */
sns_rc sns_data_stream_initiate_flush(sns_fw_data_stream *stream);

/**
 * @brief see sns_data_stream_api::get_input_cnt.
 * 
 * @param stream  Pointer to data stream.
 *
 * @return 
 *  - uint32_t Return the event count.
 *
 */
uint32_t sns_data_stream_get_input_cnt(sns_fw_data_stream *stream);
