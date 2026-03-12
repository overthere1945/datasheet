#pragma once
/** ============================================================================
 * @file sns_fw_event_service.h
 *
 * @brief Handles the storage of all Sensor events.  The ThreadManager is 
 * notified when new events are available for processing.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include <stdatomic.h>

#include "sns_event_service.h"
#include "sns_isafe_list.h"
#include "sns_island.h"
#include "sns_mem_def.h"
#include "sns_osa_lock.h"
#include "sns_thread_manager.h"

/*==============================================================================
  Forward Declarations
  ============================================================================*/

struct sns_fw_sensor_instance;
struct sns_fw_sensor;
struct sns_fw_data_stream;
struct sns_fw_sensor_event;
struct sns_osa_thread;
struct sns_thread_mgr_task;

typedef unsigned long sns_bfield;

/*==============================================================================
  Preprocessor Definitions
  ============================================================================*/

// Enable detailed messages for event handling; useful for Sensor development
// PEND: Temporarily disabled pending a more fine-grained dynamic filter.
//#define SNS_EVENT_SERVICE_VERBOSE_LOG

/**
 * @brief Number of most recent events to retain when the event buffers are low 
 * on memory.
 * 
 */
#define SNS_STREAM_REMAINDER 10

/**
 * @brief Number of event clusters, at maximum, in non-island mode
 * 
 */
#ifndef SNS_FW_EVENT_SERVICE_CLUSTER_CNT
#define SNS_EVENT_SERVICE_CLUSTER_CNT 2048
#else
#define SNS_EVENT_SERVICE_CLUSTER_CNT SNS_FW_EVENT_SERVICE_CLUSTER_CNT
#endif /* SNS_FW_EVENT_SERVICE_CLUSTER_CNT */

/**
 * @brief Number of event clusters, at maximum, in island mode.
 * 
 */
#ifndef SNS_FW_EVENT_SERVICE_CLUSTER_CNT_ISLAND
#define SNS_EVENT_SERVICE_CLUSTER_CNT_ISLAND 96
#else
#define SNS_EVENT_SERVICE_CLUSTER_CNT_ISLAND                                   \
  SNS_FW_EVENT_SERVICE_CLUSTER_CNT_ISLAND
#endif /* SNS_FW_EVENT_SERVICE_CLUSTER_CNT_ISLAND */
/**
 * @brief Total island event buffer space for the Event Service.
 * 
 */
#define SNS_EVENT_SERVICE_BUFFER_ISLAND                                        \
  (SNS_FW_EVENT_SERVICE_BUFFER_ALLOC * 1024)

/**
 * @brief Total non-island event buffer space for the Event Service.
 * 
 */
#define SNS_EVENT_SERVICE_BUFFER (SNS_EVENT_HEAP_ALLOC * 1024)

/**
 * @brief Block size of the event service buffer, in bytes;
 * must be multiple of 256; ideally divides evenly into 
 * SNS_EVENT_SERVICE_BUFFER.
 * 
 */
#define SNS_EVENT_SERVICE_BLOCK_SIZE 512
_Static_assert(SNS_EVENT_SERVICE_BLOCK_SIZE % 256 == 0, "Illegal block size");

/**
 * @brief Indices into a block are stored in 8-bit fields. Therefore 
 * multiplication must be performed to find true offset.
 * 
 */
#define SNS_EVENT_SERVICE_IDX_MULTIPLE                                         \
  (SNS_EVENT_SERVICE_BLOCK_SIZE / UINT8_MAX)

/**
 * @brief Give an 8-bit offset into a block, determine the true byte index.
 * 
 */
#define SNS_BOFF(off) (SNS_EVENT_SERVICE_IDX_MULTIPLE * (off))

/**
 * @brief Increment an 8-bit offset by 'off' bytes;
 * 
 * @note inc always a multiple of 8; no need to worry about remainders.
 * 
 */
#define SNS_BOFF_INC(off, inc) (off) += ((inc) / SNS_EVENT_SERVICE_IDX_MULTIPLE)

/**
 * @brief Decrement an 8-bit offset by 'off' bytes.
 * 
 */#define SNS_BOFF_DEC(off, inc) (off) -= ((inc) / SNS_EVENT_SERVICE_IDX_MULTIPLE)

/**
 * @brief Convert a block index into an offset into the global buffer.
 * 
 */
#define SNS_BIDX(idx) ((idx)*SNS_EVENT_SERVICE_BLOCK_SIZE)

#define SNS_EVENT_SERVICE_BLOCK_CNT                                            \
  SNS_EVENT_SERVICE_BUFFER / SNS_EVENT_SERVICE_BLOCK_SIZE
#define SNS_EVENT_SERVICE_BLOCK_CNT_ISLAND                                     \
  SNS_EVENT_SERVICE_BUFFER_ISLAND / SNS_EVENT_SERVICE_BLOCK_SIZE
_Static_assert(SNS_EVENT_SERVICE_BLOCK_CNT <= UINT16_MAX,
               "Event heap too large");
_Static_assert(SNS_EVENT_SERVICE_BLOCK_CNT_ISLAND <= UINT16_MAX,
               "Island event heap too large");

#define SNS_EVENT_SERVICE_BITMAP_LEN                                           \
  ((SNS_EVENT_SERVICE_BLOCK_CNT / (sizeof(sns_bfield) * 8)) + 1)
#define SNS_EVENT_SERVICE_BITMAP_LEN_ISLAND                                    \
  ((SNS_EVENT_SERVICE_BLOCK_CNT_ISLAND / (sizeof(sns_bfield) * 8)) + 1)

#define SNS_EVENT_SERVICE_BLOCK_ALLOC_MAX 8

/**
 * @brief Given a block index, determine the containing integer array index.
 * 
 */
#define BIT_IDX(idx) ((idx) / (8 * sizeof(sns_bfield)))
/**
 * @brief Given a block index, determine the integer array index offset.
 * 
 */
#define BIT_NUM(idx) ((idx) % (8 * sizeof(sns_bfield)))
/**
 * @brief Given a block array and index, get the bit value.
 * 
 */
#define BIT_GET(map, idx) (0 != (map[BIT_IDX(idx)] & (1 << BIT_NUM(idx))))
/**
 * @brief Check if 'range' consecutive bits starting at 'idx' in 'map' are free;
 * bit_range defined in sns_event_service.c.
 * 
 */
// PEND: Macro broken, do not use
#define BIT_GETR(map, idx, range)                                              \
  ((map[BIT_IDX(idx)] & (bit_range[range] << BIT_NUM(idx))) ==                 \
   (bit_range[range] << BIT_NUM(idx)))
/**
 * @brief Given a block array and index, set the bit value.
 * 
 */
#define BIT_SET(map, idx) (map[BIT_IDX(idx)] |= (1 << BIT_NUM(idx)))
/**
 * @brief Given a block array and index, clear the bit value.
 * 
 */
#define BIT_CLR(map, idx) (map[BIT_IDX(idx)] &= ~(1 << BIT_NUM(idx)))

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief A cluster of memory blocks which may be used to temporarily store 
 * sensor events.  Clusters grow/shrink dynamically, and consist solely of equal
 * - sized blocks.
 *
 * @note The Event Service lock shall be acquired while accessing/modifying
 * any field of any cluster object.
 *
 */
typedef struct sns_cluster
{
  uint16_t start_idx; /*!< Block indices which starts the data for this cluster.
                       *   Will always be contiguous; start_idx may be freed 
                       *   when no longer needed; blk_len will correspondingly 
                       *   shrink.  Index into sns_fw_event_service::
                       *   buffer[_island] 
                       */
  uint8_t blk_len;     /*!< Length of contiguious blocks past start_idx in this 
                        *   cluster */

  uint8_t read_off;   /*!< Byte-offset into start_idx, before which data has  
                       *   been consumed by all clients.  start_idx block may be 
                       *   freed once read_off > 255. 
                       */
  uint8_t write_off;  /*!< Byte-offset into start_idx+blk_len where next event 
                       *   will be allocated; a new block may be allocated 
                       *   if/when write_idx would extend past 255. 
                       */
  uint8_t ref_cnt;    /*!< How many other data structures contain a pointer to 
                       *   this cluster; Can release cluster when reaches zero. 
                       *   Protected by event service lock.  
                       */
  bool active;        /*!<  Whether this cluster is actively being appended.
                       * If false, free cluster when 0==ref_cnt. 
                       */
  bool island;        /*!< True if an island cluster;
                       *   PEND: Can remove field (and check against range) if 
                       *   memory is needed. 
                       */
} sns_cluster;

/**
 * @brief Event Service Buffers.
 * 
 */
enum sns_es_buffer_id
{
  SNS_ES_BUFFER_ID_ISLAND,  /*!< Buffer ID for the island mode. */
  SNS_ES_BUFFER_ID_MAIN,    /*!< Buffer ID for the big image mode. */
  SNS_ES_BUFFER_ID_LAST     /*!< Marker to indicate the last valid buffer ID. */
};

typedef struct sns_es_heap
{
  uint8_t *buffer;              /*!< Global statically pre-allocated buffers for
                                 *   all event allocation; subdivided into 
                                 *   blocks of size 
                                 *   SNS_EVENT_SERVICE_BLOCK_SIZE. 
                                 */
  sns_bfield *available_blocks;  /*!< Whether a particular block in the buffer 
                                  *   is available for use; BIT_SET to mark as 
                                  *   available; BIT_CLR to mark in-use. 
                                  */
  sns_cluster *clusters;        /*!< Pointer to clusters array. */
  uint32_t tot_blk_cnt;         /*!< Total number of blocks available in this 
                                 *   heap */
  uint32_t tot_cluster_cnt;     /*!< Total number of clusters available in this 
                                 *   heap 
                                 */
  uint32_t current_alloc_cnt;   /*!< Current number of blocks allocated */
  uint32_t current_cluster_cnt; /*!< Current number of clusters in use */
  uint32_t max_alloc_cnt;       /*!< Maximum number of blocks allocated since 
                                 *   last DIAG memory log packet 
                                 */
  int32_t max_cluster_cnt;      /*!< Maximum number of in-use clusters since 
                                 *   last DIAG memory log packet 
                                 */
  uint32_t alloc_failures;      /*!< Number of allocation failues since the last
                                 *   DIAG log packet 
                                 */
  uint32_t max_size;            /*!< Maximum allocation size (in bytes) since 
                                 *   last log packet 
                                 */
  bool clear_max;               /*!< Whether to clear maximum counts on next log
                                 *   encoding; needed due to quirk in diag 
                                 *   service feature 
                                 */
} sns_es_heap;

/**
 * @brief Private state used by the Framework for the event manager.
 *
 */
typedef struct sns_fw_event_service
{
  sns_event_service service;                /*!< Event service. */
  sns_es_heap heaps[SNS_ES_BUFFER_ID_LAST]; /*!< Heap for events. */
  sns_osa_lock lock;                        /*!< Protects buffers and all 
                                             *   cluster meta data.  Does not 
                                             *   need to be held when reading an
                                             *   event from the buffer. Cannot 
                                             *   acquire any 
                                             *   sns_fw_data_stream::stream_lock
                                             *   while holding this lock. 
                                             */
  sns_island_client_handle island_client;   /*!< Island client for event 
                                             *   service. 
                                             */
  sns_osa_thread *low_mem_thread;           /*!< Thread handling low memory on 
                                             *   Event Service buffers. 
                                             */
  _Atomic unsigned int low_mem_active;      /*!< Whether the low memory thread  
                                             *   is actively dropping Event  
                                             *   Service samples; Field is not 
                                             *   mutex-protected. 
                                             */
  sns_osa_lock *low_mem_lock;               /*!< To synchronize sensor specific 
                                             *   threads with low memory process.
                                             *   Threads will wait on this lock 
                                             *   to check low mem process 
                                             *   completion. 
                                             */
  uint8_t island_full; /*!< If non-zero, island is full: allocate from 
                        *   non-island. Decrement upon each freed island block.
                        */
} sns_fw_event_service;

/*=============================================================================
  Shared Variable Declaration.
  ===========================================================================*/
extern sns_fw_event_service event_service;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Initialize the event service; to be used only by the Service Manager.
 *
 * @return
 *  - sns_fw_event_service* Pointer to the initializred event service.
 *
 */
sns_fw_event_service *sns_event_service_init(void);

/**
 * @brief See sns_event_service_api::publish_error.
 * 
 * @param instance Pointer to the sensor instance for which the error event is 
 *                 published.
 * @param reason   The error reason code to be published.
 *
 * @return
 *  - SNS_RC_SUCCESS On successfully publishing the error.
 *  
 */
sns_rc sns_es_publish_error(struct sns_fw_sensor_instance *instance,
                            sns_rc reason);

/**
 * @brief Allocate and send an event to a single client.  This function may be used by
 * the Framework when an event is only applicable to a single client, and that
 * client may not have (yet) an instance to satisfy it.  Examples include
 * errors during set_client_request, or handling Attribute Requests.
 *
 * @param[in] stream      Stream on which to send the event.
 * @param[in] message_id  Message id for the event.
 * @param[in] event_len   Length of the event buffer.
 * @param[in] event       Pointer to a PB encoded event message.
 *
 * @return
 *  - SNS_RC_NOT_AVAILABLE Closing stream is unavailable.
 *  - SNS_RC_SUCCESS       On Success.
 *
 */
sns_rc sns_es_send_event(struct sns_fw_data_stream *stream, uint32_t message_id,
                         uint32_t event_len, uint8_t *event);

/**
 * @brief Publish a generic error event with appropriate error reason.
 *
 * @note This is a helper function; an error event could also be
 * generated and published using the alloc_event/publish_event API.
 *
 * @param[in] stream Stream on which to send the event.
 * @param[in] reason Reason for the error.
 *
 * @return
 *  - SNS_RC_SUCCESS        On Success
 *  - SNS_RC_FAILED         A catastrophic failure has occurred, restart the 
 *                          Framework.
 *  - SNS_RC_NOT_AVAILABLE  No data is available for processing.
 *
 */
sns_rc sns_es_send_error_event(struct sns_fw_data_stream *stream,
                               sns_rc reason);

/**
 * @brief Unlike other structures in the API, the start of the 
 * sns_fw_sensor_event and sns_sensor_event structures are not equivalent.
 * Therefore some pointer arithmatic must be performed.
 *
 * @param[in] fw_event Valid sensor event.
 *
 * @return 
 *  - sns_fw_sensor_event* Framework event corresponding to input.
 *
 */
struct sns_fw_sensor_event *
sns_es_get_fw_event(struct sns_sensor_event *fw_event);

/**
 * @brief Generates log packets that contain statistics on the event buffer 
 * usage.
 *
 * @param[in] cookie Cookie that is used to populate the user_defiend field
 *                   in the log packet.
 *
 * @return 
 *  - None.
 *
 */
void sns_es_log_buffer_use(uint64_t cookie);

/**
 * @brief Remove an unpublished event allocated by a sensor instance that is
 * about to be deinitialized.
 *
 * @param[in] instance Instance that is about to be deinitialized.
 * 
 * @return 
 *  - None.
 *
 */
void sns_es_deinit_instance(struct sns_fw_sensor_instance *instance);

/**
 * @brief Determine if the low memory thread is actively dropping events.
 * 
 * @return 
 *  - True  if the low memory thread is actively dropping events.
 *  - False Otherwise. 
 *
 */
bool sns_es_low_mem_active(void);

/**
 * @brief Add a datastream to list of pending events.
 *
 * @param[in] stream Data stream that has pending events.
 *
 * @return 
 * - None.
 *
 */
void sns_es_add_pending_event(struct sns_fw_data_stream *stream);

/**
 * @brief Mark an event (on the given cluster) as consumed.  Free resources as
 * appropiate.
 *
 * @param[inout] cluster  Containing cluster; may be emptied.
 * @param[inout] event    Event to mark as consumed.
 *
 * @note Event service lock should be held.
 *
 * @return 
 *  - None.
 *
 */
void sns_es_consume(struct sns_cluster *cluster,
                    struct sns_fw_sensor_event *event);

/**
 * @brief Decrement the cluster reference count, and release cluster if no
 * references remain.  Should only be called once per client or service,
 * when the pointer to this cluster is removed.
 *
 * @param[in] cluster Pointer to the cluster.
 *
 * @return 
 *  - None.
 *
 */
void sns_es_release_cluster(struct sns_cluster *cluster);

/**
 * @brief Drop events from the provided list of streams.  Leave 'remainder' left
 * (if available) on each stream.
 *
 * @param[in] service   The Event Service.
 * @param[in] list      Data Streams of a Sensor or Sensor Instance.
 * @param[in] remainder Keep at most this many latest events.
 *
 * @return
 *  - None.
 *
 */
void sns_es_drop(sns_fw_event_service *service, sns_isafe_list *list,
                 uint32_t remainder);

/**
 * @brief Low memory has been reached in the big image Event Service buffer. 
 * Take whatever steps are appropriate to free memory.  This may involve 
 * dropping samples.
 *
 * @param[in] service The Event Service.
 *
 * @return
 *  - None.
 *
 */
void sns_es_handle_low_mem(sns_fw_event_service *service);

/**
 * @brief Sensor threads waits on below API till low memory thread handling 
 * completes. Below API should be called before acquiring library lock/any other
 * lock.
 *
 * @return
 *  - None.
 *
 */
void sns_es_low_mem_block(void);
