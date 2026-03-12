#pragma once
/** =============================================================================
 * @file
 *
 * @brief Framework header for Sensors FILE Service.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sns_rc.h"
#include "sns_service.h"
#include "sns_sensor.h"
#include "sns_printf_int.h"
/*=============================================================================
  Macros and constants
  ===========================================================================*/
#ifdef SNS_FILE_SERVICE_DEBUG_ENABLE
/**
 * @brief Macro defining the size of the debug buffer for the File Service.
 * 
 */
#define SNS_FILE_SERVICE_DEBUG_BUFFER_SIZE     1024

/**
 * @brief Macro defining the maximum length of a debug message for the File 
 * Service.
 * 
 */
#define SNS_FILE_SERVICE_DEBUG_MESSAGE_MAX_LEN 256

/**
 * @brief Convenience macro for printing log messages with varying severity 
 * levels.
 * This macro is designed to simplify the logging of messages with different
 * severity levels (e.g., error, warning, info) directly to the file service.
 * The severity level is specified by `prio`, followed by the sensor identifier,
 * the format string `fmt`, and the variable arguments `...`.
 *
 * @example 
 * SNS_FILE_PRINTF(ERROR, MY_SENSOR, "An error occurred: %d", error_code);
 * 
 *
 * @param[in] prio    Severity level of the log message (e.g., ERROR, WARN, 
 *                    INFO).
 * @param[in] sensor  Identifier of the sensor generating the log message.
 * @param[in] fmt     Format string for the log message.
 * @param[in] ...     Variable arguments corresponding to the format specifiers 
 *                    in `fmt`.
 *
 */
#define SNS_FILE_PRINTF(prio, sensor, fmt, ...)                                \
  do                                                                           \
  {                                                                            \
    sns_file_service_sprintf(SNS_DIAG_SSID, sensor, SNS_##prio, __FILENAME__,  \
                             __LINE__, fmt, ##__VA_ARGS__);                    \
  } while(0)

/**
 * @brief Convenience macro for printing log messages with varying severity 
 * levels.
 * This macro is designed to simplify the logging of messages with different
 * severity levels (e.g., error, warning, info) directly to the file service.
 * The severity level is specified by `prio`, followed by the sensor identifier,
 * the format string `fmt`, and the variable arguments `...`.
 *
 * Example usage:
 * ```c
 * SNS_FILE_SPRINTF(error, MY_SENSOR, "An error occurred: %d", error_code);
 * ```
 *
 * @param[in] prio    Severity level of the log message (e.g., ERROR, WARN, 
 *                    INFO).
 * @param[in] sensor  Identifier of the sensor generating the log message.
 * @param[in] fmt     Format string for the log message.
 * @param[in] ...     Variable arguments corresponding to the format specifiers 
 *                    in `fmt`.
 *
 */
#define SNS_FILE_SPRINTF(prio, sensor, fmt, ...)                               \
  do                                                                           \
  {                                                                            \
    sns_file_service_sprintf(SNS_DIAG_SSID, sensor, SNS_##prio, __FILENAME__,  \
                             __LINE__, fmt, ##__VA_ARGS__);                    \
  } while(0)
#else /* SNS_FILE_SERVICE_DEBUG_ENABLE */
#define SNS_FILE_PRINTF  SNS_PRINTF
#define SNS_FILE_SPRINTF SNS_SPRINTF
#endif /* SNS_FILE_SERVICE_DEBUG_ENABLE */

#ifdef SSC_TARGET_X86
#define SNS_RPCD_OVERRIDE(available)                                           \
  do                                                                           \
  {                                                                            \
    sim_rpcd_override = available;                                             \
    SNS_FILE_PRINTF(HIGH, sns_fw_printf, "RPCD Override %d", available);       \
  } while(0)
#define SNS_CHK_RPCD_OVERRIDE()                                                \
  do                                                                           \
  {                                                                            \
    if(sim_rpcd_override && rpcd_available)                                    \
      rpcd_available = false;                                                  \
  } while(0)
#else
#define SNS_RPCD_OVERRIDE(available)
#define SNS_CHK_RPCD_OVERRIDE()
#endif /* SSC_TARGET_X86 */

#ifdef SNS_FILE_SERVICE_DEBUG_ENABLE
void sns_file_service_sprintf(uint16_t ssid, const sns_sensor *sensor,
                              sns_message_priority prio, const char *file,
                              uint32_t line, const char *format, ...);
#endif /* SNS_FILE_SERVICE_DEBUG_ENABLE */

/*=============================================================================
  Internal Dependencies
  ===========================================================================*/
extern volatile bool rpcd_available;
extern volatile bool sim_rpcd_override;

/* ======================== Version History ===================================
 *  Version  comments
 *  1        Initial version
 */
#define SNS_FILE_SERVICE_VERSION (1)

/*=============================================================================
  Type Definitions
  ===========================================================================*/

typedef void *sns_service_status_func_arg;
typedef void (*sns_service_status_cb)(sns_service_status_func_arg, bool);

typedef struct sns_file_service_internal
{
  sns_service service;                        /*!< Base service structure. */
  struct sns_file_service_internal_api *api;  /*!< Pointer to the file sevice 
                                               *   internal API structure. 
                                               */
} sns_file_service_internal;

typedef struct sns_file_service_internal_api
{
  uint32_t struct_len; /*!< Length of sns_file_service_internal_api structure. 
                        */

  /**
   * @brief Registers callback to be called when service status changes.
   *
   * @param[in] cb  The callback function to register.
   * @param[in] arg The user argument to pass to the callback.
   *
   * @return
   *  - SNS_RC_SUCCESS  The callback is registered.
   *  - SNS_RC_FAILED   Could not register the callback.
   *
   */
  sns_rc (*register_status_callback)(sns_service_status_cb cb,
                                     sns_service_status_func_arg arg);

  /**
   * @brief Opens the file whose name is the string pointed to by filename and
   * assosiates a stream with it.
   *
   * @param[in] filename  Absolute path of file to open.
   * @param[in] mode      What open mode to use.
   *
   * @return 
   *  - FILE * The associated file stream.
   *
   */
  FILE *(*fopen)(const char *restrict filename, const char *restrict mode);

  /**
   * @brief Attempts to write the buffer pointed to by ptr whose size is 
   * specified by size upto n elements specifed by count, to the file_handle.
   *
   * @param[in] ptr Pointer to the buffer.
   * @param[in] size Size of the data type.
   * @param[in] count Number of elements to be written.
   * @param[in] stream The file stream to which to write.
   *
   * @return 
   *  - size_t The number of elements successfully written.
   *
   */
  size_t (*fwrite)(void const *ptr, size_t size, size_t count, FILE *stream);

  /**
   * @brief Attempts to read size n elements specified by count from the buffer
   *  ptr, each size bytes long, from the file_handle.
   *
   * @param[in] ptr Pointer to the buffer.
   * @param[in] size Size of the data type.
   * @param[in] count Number of elements to be read.
   * @param[in] stream The file stream from which to read.
   *
   * @return 
   *  - size_t  The number of elements successfully read.
   *
   */
  size_t (*fread)(void *ptr, size_t size, size_t count, FILE *stream);

  /**
   * @brief Closes the file pointed by stream.
   *
   * @param[in] stream The file stream.
   *
   * @return 
   *  - 0   If successful.
   *  - EOF Otherwise.
   *
   */
  int (*fclose)(FILE *stream);

  /**
   * @brief Sets the file position indicator for the given stream to 'offset' 
   * bytes from 'whence'
   *
   * @param[in] stream   The file stream.
   * @param[in] offset   Number of bytes from 'whence'.
   * @param[in] whence   Starting location - SEEK_SET, SEEK_CUR, or SEEK_END.
   *
   * @return 
   *  - 0 On success. 
   *  - -1 On error.
   *
   */
  int (*fseek)(FILE *stream, long long offset, int whence);

  /**
   * @brief Writes data from the given 'buffer' of length 'len' to 'file'  
   * replacing existing contents of 'file' making sure original contents of  
   * 'file' is intact if write operation were to fail; writes to 'temp_dir' 
   * first if renaming it to 'file' is possible.
   *
   * @param[in] file     Absolute path of file to write to
   * @param[in] temp_dir Directory to wrte temporary file
   * @param[in] buffer   Data to write to 'file'
   * @param[in] len      Number of bytes in 'buffer'
   *
   * @return 
   *  - 0 on success. 
   *  - -1 on error and error would be set appropriately.
   *
   */
  int (*fwritesafe)(const char *file, const char *temp_dir, void const *buffer,
                    size_t len);

  /**
   * @brief Gets information on a file/directory.
   *
   * @param[in] path    Absolute path of the file/directory.
   * @param[in] buffer  Destination for the information.
   *
   * @return 
   *  - 0 on success.
   *  - -1 on error and error would be set appropriately.
   *
   */
  int (*stat)(const char *path, struct stat *buffer);

  /**
   * @brief Deletes a file.
   *
   * @param[in] path Absolute path of the file to delete.
   *
   * @return 
   *  - 0 on success. 
   *  - -1 on error and error would be set appropriately.
   *
   */
  int (*unlink)(const char *path);

  /**
   * @brief secure database intialization.
   *
   * @return
   *  - SNS_RC_SUCCESS  If secure db initialization successful.
   *  - SNS_RC_FAILED   If secure db initialization fails.
   *
   */
  sns_rc (*init)(void);

} sns_file_service_internal_api;

/*=============================================================================
  Function Declarations
  ===========================================================================*/
/**
 * @brief Get internal file service.
 * 
 * @return 
 *  - sns_file_service_internal*  Returns the internal file service.
 * 
 */
sns_file_service_internal *sns_fs_get_internal_service(void);

/**
 @brief Get the status of rpcd_available.
 * 
 * @return 
 *  - True  If rpcd_available is available.
 *  - False If rpcd_available is not available.
 * 
 */
bool sns_fs_is_up(void);
