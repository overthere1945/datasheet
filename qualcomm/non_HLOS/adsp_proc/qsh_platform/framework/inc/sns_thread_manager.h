#pragma once
/** =============================================================================
 * @file
 *
 * @brief Thread manager creates and manages pools of worker threads to handle
 * sensor tasks. There are two thread pools, one with high thread priority and
 * one with low thread priority.
 *
 * Each thread pool maintains one or more lists of pending tasks. The worker
 * threads in a thread pool execute tasks from these lists. There are three task
 * lists, the one with high priority is managed by the high priority thread pool
 * while the ones with medium and low priority are managed by the low priority
 * thread pool.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_island_util.h"
#include "sns_list.h"
#include "sns_osa_lock.h"
#include "sns_osa_sem.h"
#include "sns_osa_thread.h"
#include "sns_rc.h"
#include "sns_time.h"

/*=============================================================================
  Macros
  ===========================================================================*/

/**
 * @brief Length of task array.
 *
 */
#define SNS_TMGR_TASK_ARR_LEN (1 << 4) /**< must be power of 2, max is 2^15 */

#define TMGR_DEBUG

/*=============================================================================
  Forward Declarations
  ===========================================================================*/

struct sns_sensor_library;

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Task function to be invoked for a sensor task.
 *
 * @param[in] arg Task function arguments.
 *
 * @return
 *  - SNS_RC_INVALID_STATE Sensor/ Instance is in an invalid state after
 *                         task function execution.
 *  - SNS_RC_SUCCESS       Success.
 *
 */
typedef sns_rc (*sns_tmgr_task_func)(void *arg);

/**
 * @brief Enumeration representing indices for thread pools managed by the Task
 * Manager. This enumeration defines two thread pool indices.
 *
 */
typedef enum sns_tmgr_thread_pool_idx
{
  SNS_TMGR_THREAD_POOL_IDX_HIGH = 0, /*!< High priority thread pool index. */
  SNS_TMGR_THREAD_POOL_IDX_LOW = 1   /*!< Low priority thread pool index. */
} sns_tmgr_thread_pool_idx;

/**
 * @brief Enumeration representing task list priorities within a high-priority
 * thread pool.
 *
 */
typedef enum sns_tmgr_task_lists_pool_high
{
  SNS_TMGR_TASK_LIST_PRIO_HIGH = 0, /*!< Highest priority in the high-priority
                                     *   thread pool.
                                     */
  SNS_TMGR_TASK_LIST_MAX_POOL_HIGH  /*!< Placeholder to indicate the end of the
                                     *   high-priority task list priorities. */
} sns_tmgr_task_lists_pool_high;

/**
 * @brief Enumeration representing task list priorities within a low-priority
 * thread pool.
 *
 */
typedef enum sns_tmgr_task_lists_pool_low
{
  SNS_TMGR_TASK_LIST_PRIO_MED = 0, /*!< Medium priority in the low-priority
                                    *   thread pool.
                                    */
  SNS_TMGR_TASK_LIST_PRIO_LOW = 1, /*!< Low priority in the low-priority thread
                                    *   pool.
                                    */
  SNS_TMGR_TASK_LIST_MAX_POOL_LOW  /*!< Placeholder to indicate the end of the
                                    *   low-priority task list priorities.
                                    */
} sns_tmgr_task_lists_pool_low;

/**
 * @brief Enumeration representing task priorities within the Task Manager.
 *
 */
typedef enum sns_tmgr_task_prio
{
  SNS_TMGR_TASK_PRIO_HIGH = 0, /*!< High priority. */
  SNS_TMGR_TASK_PRIO_MED = 1,  /*!< Medium priority. */
  SNS_TMGR_TASK_PRIO_LOW = 2   /*!< Low priority. */
} sns_tmgr_task_prio;

/**
 * @brief Enumeration representing task status
 *
 */
typedef enum sns_tmgr_task_status
{
  SNS_TMGR_TASK_STATUS_INVALID,   /*!< task data is invalid */
  SNS_TMGR_TASK_STATUS_READY,     /*!< task is ready to be processed */
  SNS_TMGR_TASK_STATUS_REMOVING,  /*!< task is being removed from task list */
  SNS_TMGR_TASK_STATUS_REMOVED,   /*!< task is off task list */
  SNS_TMGR_TASK_STATUS_EXECUTING, /*!< a worker thread is processing this task
                                   */
  SNS_TMGR_TASK_STATUS_DISCARDED, /*!< task is discarded when library or stream
                                     is deleted */
} sns_tmgr_task_status;

/**
 * @brief Debugging information for a task, including the queue timestamp.
 *
 */
typedef struct sns_tmgr_task_debug
{
  sns_time queue_ts; /*!< Timestamp indicating when the task was queued. */
} sns_tmgr_task_debug;

/**
 * @brief Arguments structure for creating a task manager task.
 *
 */
typedef struct sns_tmgr_task_args
{
  sns_osa_lock *task_lock;                 /*!< Lock for synchronizing access
                                            *   to the task.
                                            */
  struct sns_fw_sensor_instance *instance; /*!< Reference to the sensor
                                            *   instance.
                                            */
} sns_tmgr_task_args;

/**
 * @brief Represents a task managed by the Task Manager.
 *
 */
typedef struct sns_tmgr_task
{
  struct sns_sensor_library *library;  /*!< The library this task works on */
  sns_tmgr_task_func func;             /*!< Function to call to handle this
                                        * task
                                        */
  void *func_arg;                      /*!< Argument to pass into function. */
  sns_tmgr_task_args task_args;        /*!< */
  _Atomic sns_tmgr_task_status status; /*!< Status of this task */
  bool island_task;                    /*!< Whether this task runs in island */
#ifdef TMGR_DEBUG
  uint8_t skip_not_ready;
  uint8_t skip_lib_busy;
  uint8_t skip_contention;
  void *taker;
  uint32_t age;
  uint64_t entry_ts;
#endif
} sns_tmgr_task;

/**
 *  @brief A set of pending tasks
 *
 */
typedef struct sns_tmgr_task_arr
{
  sns_list_item list_entry;                   /*!< Task list entry point. */
  sns_tmgr_task tasks[SNS_TMGR_TASK_ARR_LEN]; /*!< Array of tasks. */
  _Atomic uint16_t write_idx;                 /*!< Write index into the tasks
                                               *   array.
                                               */
} sns_tmgr_task_arr;

/**
 *  @brief A list of pending tasks
 *
 */
typedef struct sns_tmgr_task_pending_task_list
{
  sns_list list;                  /*!< list of sns_tmgr_task_arr */
  _Atomic uint16_t read_idx;      /*!< index of the most recent processed
                                   * task
                                   */
  _Atomic uint16_t pending_idx;   /*!< index of the latest task being added */
  uint32_t max_dyamic_list_count; /*!< Max value of list.cnt */
  uint8_t pool_idx;
  uint8_t list_idx;
  sns_tmgr_task_arr task_data; /*!< Circular task array */
} sns_tmgr_task_pending_task_list;

/**
 * @brief A particular worker thread
 *
 */
typedef struct sns_tmgr_worker_thread
{
  sns_osa_thread *thread;            /*!< Worker thread pointer. */
  struct sns_tmgr_thread_pool *pool; /*!< Thread pool pointer. */
  sns_osa_sem *sem_wait;             /*!< Will point to the semaphore in a
                                      *   sns_thread_pool::sem.
                                      */
  sns_osa_sem *sem_wait_continue;    /*!< If not NULL, points to semaphore of
                                      * the other pool to continue processing */
  /*! Tracks the busy libraries encountered by this thread every time it
   *  wakes up */
  struct sns_sensor_library *busy_libs[SNS_NUM_OF_WORKER_THREADS_POOL_H +
                                       SNS_NUM_OF_WORKER_THREADS_POOL_L - 1];
  sns_tmgr_task task; /*!< Current task being executed by this
                       *   thread.
                       */
#ifdef TMGR_DEBUG
  sns_time get_task_ts;
  uintptr_t task_addr;
  uint16_t orig_read_idx;
  uint16_t got_read_idx;
#endif
  uint16_t max_num_b2b_tasks;      /*!< Max number of tasks executed back to
                                    *   back.
                                    */
  uint16_t static_array_write_idx; /*!< Reset before getting a ready task*/
  uint8_t num_busy_libs;
  bool exit; /*!< Flag to signal thread to exit*/
} sns_tmgr_worker_thread;

/* A pool of worker threads, assigned to a particular SNS_OSA_MEM_TYPE. */
typedef struct sns_tmgr_thread_pool
{
  char name;                       /*!< Name of this thread pool. */
  sns_osa_mem_type type;           /*!< Heap on which this thread's stack
                                    *   is allocated.
                                    */
  sns_osa_sem *sem;                /*!< Semaphore all threads in this
                                    *   pool wait upon.
                                    */
  sns_tmgr_worker_thread *threads; /*!< Array of worker threads assigned
                                    *   to this pool.
                                    */
  int32_t threads_len;             /*!< Length of threads array. */
  sns_osa_lock pending_tasks_lock; /*!< Protects following fields. */
  uint32_t block_island_cnt;       /*!< Quantity of non-island entries on
                                    *   pending_tasks queue.
                                    */
  /*! Note: The low priority thread pool maintains 2 lists of tasks
   * for medium & low priority items. So it has 2 lists. In the high
   * priority thread pool, the 2nd list is unused */
  sns_tmgr_task_pending_task_list pending_task_lists[2];
  int32_t pending_task_lists_len;         /*!< Length of task lists array. */
  sns_island_client_handle island_client; /*!< Island client handle. */
} sns_tmgr_thread_pool;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Add a new pending task to the thread manager queue.
 *
 * @param[in] library   Library for which this task is added.
 * @param[in] cb_func   Task callback function.
 * @param[in] cb_arg    Task callback function argument.
 * @param[in] priority  Task priority.
 * @param[in] task_args Task arguments, refer sns_tmgr_task_args structure.
 *
 * @return
 *  - None.
 *
 */
void sns_thread_manager_add(struct sns_sensor_library *library,
                            sns_tmgr_task_func cb_func, void *cb_arg,
                            sns_tmgr_task_prio priority,
                            sns_tmgr_task_args *task_args);

/**
 * @brief Remove a library from the pending task in thread manager queue.
 *
 * @param[in] library Library which is going to be removed
 *
 * @return
 *  - True  if given library is completely removed from pending tasks and worker
 *          threads.
 *  - False if given library is still in use by at least one of the worker
 *          threads.
 *
 */
bool sns_thread_manager_remove(struct sns_sensor_library *library);

/**
 * @brief Initialize the Thread Manager, and begin all worker threads.
 *
 */
sns_rc sns_thread_manager_init(void);

/**
 * @brief Deinit the Thread Manager.
 *
 * @return
 *  - None.
 *
 */
void sns_thread_manager_deinit(void);

/**
 * @brief Disable worker threads from handling any further tasks.  Active tasks
 * may be continued until their completion.
 *
 * @param[in] status True to disable; false to re-enable.
 *
 * @return
 *  - None.
 *
 */
void sns_thread_manager_disable(bool status);

/**
 * @brief Remove pending events from task queue of each thread pool.
 *
 * @param[in] cb_arg  Stream to be removed.
 *
 * @return
 *  - None.
 *
 */
void sns_thread_manager_remove_pending_events(void *cb_arg);

/**
 * @brief Get thread pools structure pointer.
 *
 * @return
 *  - sns_tmgr_thread_pool* pointer to the thread pools structure.
 */
sns_tmgr_thread_pool *sns_thread_manager_get_thread_pools(void);
