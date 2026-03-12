#pragma once
/** ============================================================================
 * @file
 *
 * @brief Sensor state and functions only available within the Framework.
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
#include "sns_fw_diag_types.h"
#include "sns_isafe_list.h"
#include "sns_island_util.h"
#include "sns_memory_service.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_register.h"
#include "sns_sensor.h"
#include "sns_ulog.h"

/*==============================================================================
  Macros
  ============================================================================*/

#if defined(SNS_ULOG_ENABLE) && defined(SNS_FW_DBG)
/**
 * @brief Debugging log level used in firmware to capture detailed diagnostic
 * information.
 *
 * @note SNS_ULOG_ENABLE and SNS_FW_DBG has to be defined. Otherwise till be
 * fall back to SNS_PRINTF.
 *
 */
#define SNS_FW_DBG_LOG(...) SNS_ULOG_ADD(SNS_ULOG_ID_FW_DBG, __VA_ARGS__)
#else
#define SNS_FW_DBG_LOG(...) SNS_PRINTF(HIGH, sns_fw_printf, __VA_ARGS__)
#endif

#if defined(SNS_ULOG_ENABLE) && defined(SNS_FW_DBG)
/**
 * @brief Framework logging macro that uses SNS_FW_DBG_LOG is SNS_ULOG_ENABLE
 * and SNS_FW_DBG define else false back to SNS_SPRINTF.
 *
 */
#define SNS_FW_DBG_LOG_STR SNS_FW_DBG_LOG
#else
#define SNS_FW_DBG_LOG_STR(...) SNS_SPRINTF(HIGH, sns_fw_printf, __VA_ARGS__)
#endif

#if defined(SNS_ULOG_ENABLE) && defined(ENABLE_BOOT_UP_PROFILING)
/**
 * @brief SNS_ULOG_BOOTUP_PROFILE is a macro used to log messages during the
 * boot-up sequence of a Sensor Hub system. It utilizes structured logging via
 * SNS_ULOG_ADD when both SNS_ULOG_ENABLE and ENABLE_BOOT_UP_PROFILING are
 * defined. This aids in tracing and understanding the boot process.
 *
 */
#define SNS_ULOG_BOOTUP_PROFILE(...)                                           \
  SNS_ULOG_ADD(SNS_ULOG_ID_INIT_SEQ, __VA_ARGS__)
#else
#define SNS_ULOG_BOOTUP_PROFILE(...)
#endif

/*=============================================================================
  Forward Declarations
  ===========================================================================*/

struct sns_sensor_library;

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * @brief Sensor state used by the Framework; not accessible to Sensor
 * developers. All state is protected by library_lock.
 *
 */
typedef struct sns_fw_sensor
{
  sns_sensor sensor;                               /*!< Structure for the
                                                    *   generic sensor. */
  sns_isafe_list data_streams;                     /*!< All active data streams
                                                    *   incoming to this Sensor.
                                                    */
  sns_isafe_list sensor_instances;                 /*!< List of active Instances
                                                    *    for this Sensor.
                                                    */
  sns_isafe_list_iter instances_iter;              /*!< Iterator used by Sensors
                                                    *   using sns_sensor_cb::
                                                    *   get_sensor_instance.
                                                    */
  sns_isafe_list_item list_entry;                  /*!< Kept on
                                                    *   sns_sensor_library::
                                                    *   sensors.
                                                    */
  struct sns_sensor_library *library;              /*!< Library containing this
                                                    *   Sensor.
                                                    */
  sns_osa_lock *sensor_instances_list_lock;        /*!< Lock to be held my
                                                    *   multithreaded libs to
                                                    *   access sensor instances
                                                    *   list. */
  sns_diag_sensor_config diag_config;              /*!< Pointer to debug
                                                    *   configuration for the
                                                    *   specific sensor data
                                                    *   type within the diag
                                                    *   service.  sns_diag_lock
                                                    *   must be acquired prior
                                                    *   to accessing this field.
                                                    */
  struct sns_attribute_info *attr_info;            /*!< Sensor state managed by
                                                    *   the attribute service.
                                                    */
  sns_island_memory_vote_cb island_memory_vote_cb; /*!< Sensor Island memory
                                                    *   pool vote callback
                                                    *   function.
                                                    */
  uint16_t active_req_cnt;                         /*!< Active requsts on the
                                                    *   sensor.
                                                    */
  sns_island_state island_operation;               /*!< Whether the Sensor
                                                    *   supports island mode
                                                    *   operation.
                                                    */
  bool removing;                                   /*!< Set to true if this
                                                    *   Sensor is in the process
                                                    *   of being removed.
                                                    */
} sns_fw_sensor;

/**
 * @brief Collect the statistical information about the sensor library for
 * debug.
 *
 */
typedef struct sns_sensor_library_time_stats
{
  sns_time min; /*!< Minimum execution time observed for any task. */
  sns_time max; /*!< Maximum execution time observed for any task. */
  sns_time avg; /*!< Average execution time observed for all tasks. */
} sns_sensor_library_time_stats;

/**
 * @brief Aggregate statistics about the sensor library.
 *
 */
typedef struct sns_sensor_library_stats
{
  uint32_t num_tasks;                       /*!< Total number of tasks managed
                                             *   by the sensor library.
                                             */
  sns_sensor_library_time_stats exec_time;  /*!< Statistics about execution
                                             *   times of tasks.
                                             */
  sns_sensor_library_time_stats queue_time; /*!< Statistics about queuing times
                                             *   of tasks.
                                             */
} sns_sensor_library_stats;

/**
 * @brief Protected by library_lock.
 *
 */
typedef enum
{

  SNS_LIBRARY_INIT = 0,            /*!< Library sensors are being initialized.
                                    */
  SNS_LIBRARY_ACTIVE = 1,          /*!< All sensor initialization has
                                    *   completed.
                                    */
  SNS_LIBRARY_REMOVE_START = 2,    /*!< Library is waiting for all Sensors to
                                    *   complete deinitialization.
                                    */
  SNS_LIBRARY_REMOVE_COMPLETE = 3, /*!< Library is ready to be removed. */
} sns_library_removing_state;

/**
 * @brief Keeps track of numbers of sensor tasks being processed by
 * high and low priority threads
 *
 */
typedef union sns_sensor_task_count
{
  struct
  {
    uint16_t count_low;
    uint16_t count_high;
  };
  _Atomic uint32_t count;
} sns_sensor_task_count;

/**
 * @brief A statically or dynamically loaded library which contains one or more
 * Sensors. Practically, there will be a object of this type per each
 * sns_register_sensors() function call.
 *
 */
typedef struct sns_sensor_library
{
  sns_isafe_list_item list_entry;          /*!< Kept on sns_sensor.c::
                                            *   libraries.
                                            */
  sns_time creation_ts;                    /*!< Time of library creation. */
  sns_osa_lock *library_lock;              /*!< Protects the list of sensors;
                                            *   Protects the state of
                                            *   associated Sensors; this
                                            *   mutex must be acquired prior
                                            *   to calling or accessing any
                                            *   Sensor in this library.
                                            */
  sns_isafe_list sensors;                  /*!< List of Sensors contained
                                            *   within this library.
                                            */
  sns_isafe_list_iter sensors_iter;        /*!< Iterator used by Sensors
                                            *   using sns_sensor_cb::
                                            *   get_library_sensor.
                                            */
  uint32_t registration_index;             /*!< Registration index of this
                                            *   copy of the library.
                                            */
  sns_register_sensors register_func;      /*!< Registration function
                                            *   associated with this library.
                                            */
  _Atomic unsigned int removing;           /*!< Number of threads currently
                                            *   executing a task for this
                                            *   library.
                                            */
  uint32_t dynamic_lib_integrity;          /*!< Dynamic library integrity
                                            *   checksum.
                                            */
  sns_sensor_task_count sensor_task_count; /*!< - Count of sensor tasks
                                            *   on the task list. */
  _Atomic uint32_t exec_count;             /*!< Number of threads currently
                                            *   executing a task for this
                                            *   library.
                                            */
  sns_mem_segment mem_segment;             /*!< Sensor memory segment. */
  bool is_dynamic_lib;                     /*!< - True: If it is a dynamic
                                            *   library.
                                            *   - False: for static library.
                                            */
  bool is_multithreaded;                   /*!< - True: If independent
                                            *   instance tasks can be
                                            *   executed across multiple
                                            *   threads.
                                            *   - Flase: Otherwise.
                                            */
#ifdef SNS_TMGR_TASK_DEBUG
  sns_sensor_library_stats lib_stats; /*!< Collect the statistical
                                       *   information about the sensor
                                       *   library for debug.
                                       */
#endif
} sns_sensor_library;

/**
 * @brief Callback function used in sns_sensor_foreach().
 *
 * @param[in] sensor  Framework sensor pointer.
 * @param[in] arg     User-specified argument for this callback fucntion.
 *
 * @return
 *   - Result code define by user.
 *
 */
typedef bool (*sns_sensor_foreach_func)(sns_fw_sensor *sensor, void *arg);

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * @brief Function initializes a sensor with a specified state length and API
 * implementations. It allocates the necessary state buffer for the sensor and
 * configures it with the provided interfaces. See sns_register_cb::init_sensor.
 *
 * @param[in] state_len     Size of the sensor state structure.
 * @param[in] sensor_api    Pointer to the sensor APIs.
 * @param[in] instance_api  Pointer to the sensor instnace APIs.
 *
 * @return
 *  - SNS_RC_SUCCESS  Successfully initializing the sensor.
 *  - SNS_RC_POLICY   if the sensor state structure length exceeds the maximum
 *                    size allowed for a Sensor to request for its state buffer.
 *  - SNS_RC_FAILED   On failure to initialize the sensor.
 *
 */
sns_rc sns_sensor_init(uint32_t state_len,
                       struct sns_sensor_api const *sensor_api,
                       struct sns_sensor_instance_api const *instance_api);

/**
 * @brief Deinitialize and free a Sensor and all associated state. Recursively
 * deinit all active Sensor Instances and their associated state.
 * Remove this Sensor from the associated Library's list.
 *
 * @note library::state_lock and library::sensor_lock must be held.
 *
 * @param[in] sensor Pointer to the sensor object to be deinitialized.
 *
 * @return
 *  - SNS_RC_SUCCESS If the sensor was successfully deinitialized.
 *
 */
sns_rc sns_sensor_deinit(sns_fw_sensor *sensor);

/**
 * @brief Check whether a Sensor is ready to be removed/deleted, and do so now
 * if possible.
 *
 * @param[in] sensor Pointer to the sensor object to be deleted.
 *
 * @return
 *  - True  If deleted.
 *  - False Otherwise.
 *
 */
bool sns_sensor_delete(sns_fw_sensor *sensor);

/**
 * @brief Adds checksum for the given dynamic library
 *
 * @note library must be a dynamic library.
 *
 * @param[in] library Poitner to the library.
 *
 * @return
 *  - None.
 *
 */
void sns_sensor_dynamic_library_add_checksum(sns_sensor_library *library);

/**
 * @brief Validates the given dynamic library.
 *
 * @note library must be a dynamic library.
 *
 * @param[in] library Poitner to the library.
 *
 * @return
 *  - True  If library is a valid dynamic library.
 *  - False Otherwise.
 *
 */
bool sns_sensor_dynamic_library_validate(sns_sensor_library *library);

/**
 * @brief Print all the collected statistics for the library since last device
 * reboot refer structure 'sns_sensor_library_stats' for more details.
 *
 * @return
 * - None.
 *
 */
void sns_sensor_print_lib_stats(void);

/**
 * @brief Allocation, initialize, and add a new library object to the libraries
 * list.
 *
 * @param[in] register_func       Registration function entry point into the
 *                                library.
 * @param[in] registration_index  How many copies of this library to register.
 * @param[in] is_islandLib        True, if library compiled for island mode.
 * @param[in] is_multithreaded    True, if instance tasks can be executed across
 *                                multiple threads
 * @param[in] mem_segment         memory segment where sensor library is placed
 *                               ( QSH, * SSC, QSHTECH)
 *
 * @return
 *  - sns_sensor_library* Pointer to the newly allocated library reference;
 *  - NULL                If Out of Memory.
 *
 */
sns_sensor_library *sns_sensor_library_init(sns_register_sensors register_func,
                                            uint32_t registration_index,
                                            bool is_island_lib,
                                            bool is_multithreaded,
                                            sns_mem_segment mem_segment);

/**
 * @brief Deinitialize and free a Library and all associated state.  Recursively
 * deinit all active Sensor and Sensor Instances.
 *
 * @param[in] library Poitner to the library.
 *
 * @return
 *  - SNS_RC_SUCCESS Upon successfully deinitializing the library.
 *
 */
sns_rc sns_sensor_library_deinit(sns_sensor_library *library);

/**
 * @brief Checks whether any of the sensors in given library is a
 * physical sensor.
 *
 * @param[in] library Poitner to the library.
 *
 * @return
 *  - True  If library is in use.
 *  - False Otherwise.
 *
 */
bool sns_sensor_library_has_physical_sensor(sns_sensor_library *library);

/**
 * @brief Initialize static state within this file.
 *
 * @return
 *  - SNS_RC_FAILED  Catastrophic error has occurred; restart Framework.
 *  - SNS_RC_SUCCESS Upon successfully Initializing the library.
 *
 */
sns_rc sns_sensor_init_fw(void);

/**
 * @brief For each sensor on the system, call func with arg.
 *
 * @note This function cannot be called while holding *any* library_lock.
 *
 * @param[in] func  Function called for each known Sensor.
 * @param[in] arg   User-specified argument to be used in func callback.
 *
 * @return
 *  - False If func() returned false for a given sensor input.
 *
 */
bool sns_sensor_foreach(sns_sensor_foreach_func func, void *arg);

/**
 * @brief Attempt to set the island mode of a Sensor.  Specifying
 * disabled will always succeed; enabled may fail due to other state
 * constraints.
 *
 * @param[in] sensor  Sensor to be set.
 * @param[in] enabled Attempt to mark as in Island Model;
 *                    Mark as temporarily unable to support island mode.
 *
 * @return
 *  - True upon successful set.
 *  - False otherwise.
 *
 */
bool sns_sensor_set_island(sns_fw_sensor *sensor, bool enabled);

/**
 * @brief Allocate and initialize a Sensor in the same library of requesting
 * sensor
 *
 * @param[in] sensor        sensor reference that is requesting creation of new
 *                          sensor. This values should never be NULL.
 * @param[in] state_len     Size to be allocated for sns_sensor::state.
 * @param[in] sensor_api    Sensor API implemented by the Sensor developer.
 * @param[in] instance_api  Sensor Instance API; by the Sensor developer.
 *
 * @return
 * - SNS_RC_POLICY  state_len too large.
 * - SNS_RC_NOT_AVAILABLE Sensor UID is already in-use.
 * - SNS_RC_FAILED  Sensor initialization failed.
 * - SNS_RC_SUCCESS.
 *
 */
sns_rc sns_sensor_create(const sns_sensor *sensor, uint32_t state_len,
                         struct sns_sensor_api const *sensor_api,
                         struct sns_sensor_instance_api const *instance_api);

/**
 * @brief Populate the sensors suid in the framework sensor structure and
 * attribute service structure.
 *
 * @param[in] sensor sensor for which suid should be populated.
 *
 * @return
 *  - True  If suid was populated successfully.
 *  - False Otherwise.
 *
 */
bool sns_sensor_populate_suid(sns_fw_sensor *sensor);

/**
 * @brief Frees the given sensor's instances, requests, and resources.
 *
 * @param[in] sensor Sensor pointer thats instance need be clean.
 *
 * @return
 *  - int Number of instances removed.
 *
 */
int sns_sensor_clean_up(sns_fw_sensor *sensor);

/**
 * @brief Deletes all empty libraries.
 *
 * @return
 *  - None.
 *
 */
void sns_framework_delete_empty_libraries(void);

/**
 * @brief Deletes unavailable physical sensors.
 *
 * @return
 *  - None.
 *
 */
void sns_framework_delete_unavailable_phy_sensors(void);

/**
 * @brief Checks if the given library is valid; Nullifies it if
 * it's not valid, returns whether it's empty and/or ready.
 *
 * @param[inout] library  Sensor library.
 * @param[inout] is_empty Optional, returns true if library has no sensors.
 * @param[inout] is_ready Optional, returns true if at least one sensor is.
 *                        available
 *
 * @return
 *  - SNS_RC_SUCCESS        If library is valid.
 *  - SNS_RC_NOT_AVAILABLE  If library is not found.
 *
 */
sns_rc sns_sensor_library_get_status(sns_sensor_library **library,
                                     bool *is_empty, bool *is_ready);

/**
 * @brief The API to checks if Sensor API & Sensor instance API is placed in
 * island or not.
 *
 * @param[in] sensor_api    Sensor API pointer.
 * @param[in] instance_api  Sensor instance API pointer.
 *
 * @return
 *  - True  If sensor API & sensor instance API is placed in island.
 *  - False Otherwise.
 */
bool sns_is_island_sensor(sns_sensor_api const *sensor_api,
                          sns_sensor_instance_api const *instance_api);

/**
 *
 * @brief The API to check if Sensor Instance API is placed in island or not.
 *
 * @param[in] instance_api Sensor instance API pointer.
 *
 * @return
 *  - True  If sensor instance API is placed in island.
 *  - False Otherwise.
 *
 */
bool sns_is_island_sensor_instance(sns_sensor_instance_api const *instance_api);

/**
 * @brief The API returns the sensor active request count
 *
 * @param[in] sensor Sensor pointer.
 *
 * @return
 *  - uint16_t Active request count.
 *
 */
uint16_t sns_get_sensor_active_req_cnt(sns_fw_sensor *sensor);

/**
 * @brief The API to vote for sensor island mode.
 * If sensor has registered sns_island_memory_vote_cb() function,
 * This API calls registered callback function to vote for the Sensor island
 * vote.
 *
 * @param[in] sensor sensor pointer.
 * @param[in] vote   Island Vote:
 *                   - SNS_ISLAND_ENABLE Enable sensor island mode.
 *                   - SNS_ISLAND_DISABLE Disable sensor island mode.
 *
 * @return
 * - None.
 *
 */
void sns_vote_sensor_island_mode(sns_sensor *sensor, sns_ssc_island_vote vote);

/**
 * @brief The API to safely access sensor APIs in island.
 * This API exits island,
 * - If passed sensor is in big image.
 * - If passed sensor is ssc sensor & ssc island pool is not pulled into island.
 * - If passed sensor is non ssc sensor & there are no active requests to this
 * sensor.
 *
 * @param[in] fw_sensor Sensor pointer.
 *
 * @return
 *  - None.
 */
void sns_isafe_sensor_access(sns_fw_sensor *fw_sensor);
