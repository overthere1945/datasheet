/** ============================================================================
 * @file
 *
 * @brief FastRPC IDL interface for Clients to send Client processor time offset to
 * Remote Proc State Sensor running on the DSP.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

#ifndef _SNS_REMOTE_PROC_STATE_H
#define _SNS_REMOTE_PROC_STATE_H

/*==============================================================================
  Include Files
============================================================================*/
#include <AEEStdDef.h>
#include <remote.h>
#include <string.h>
#include <stdlib.h>


#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif /* __QAIC_HEADER */

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif /* __QAIC_HEADER_EXPORT */

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif /* __QAIC_HEADER_ATTRIBUTE */

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif /* __QAIC_IMPL */

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif /* __QAIC_IMPL_EXPORT */

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif /* __QAIC_IMPL_ATTRIBUTE */
#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
  Type Definitions
============================================================================*/
/**
 * Client processor enum types supported by remote proc state sensor
 */
enum sns_client_proc_types {
   SNS_CLIENT_PROC_UNKNOWN,
   SNS_CLIENT_PROC_SSC,
   SNS_CLIENT_PROC_APSS,
   SNS_CLIENT_PROC_ADSP,
   SNS_CLIENT_PROC_MDSP,
   SNS_CLIENT_PROC_CDSP,
   _32BIT_PLACEHOLDER_sns_client_proc_types = 0x7fffffff
};
typedef enum sns_client_proc_types sns_client_proc_types;


/*==============================================================================
  Public Functions
  ============================================================================*/

/**
 * @brief Opens the handle in the specified domain.  If this is the first
 * handle, this creates the session.  Typically this means opening
 * the device, aka open("/dev/adsprpc-smd"), then calling ioctl
 * device APIs to create a PD on the DSP to execute our code in,
 * then asking that PD to dlopen the .so and dlsym the skel function.
 *
 * @param[in] uri  <interface>_URI"&_dom=aDSP"
 *                 <interface>_URI is a QAIC generated uri, or
 *                 "file:///<sofilename>?<interface>_skel_handle_invoke&_modver=1.0"
 *                 If the _dom parameter is not present, _dom=DEFAULT is assumed
 *                 but not forwarded.
 *                 @note Reserved uri keys:
 *                       [0]    : first unamed argument is the skel invoke function
 *                       _dom   : execution domain name, _dom=mDSP/aDSP/DEFAULT
 *                       _modver: module version, _modver=1.0
 *                       _*     : any other key name starting with an _ is reserved
 *                       Unknown uri keys/values are forwarded as is.
 * @param[inout] h  resulting handle
 * @return 0 on success
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_remote_proc_state_open)(const char* uri, remote_handle64* h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Closes a handle.  If this is the last handle to close, the session
 * is closed as well, releasing all the allocated resources.
 *
 * @param[in] h the handle to close
 * @return 0 on success, should always succeed
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_remote_proc_state_close)(remote_handle64 h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief This function is a synchronous API to send Client processor time offset to remote proc
 * state sensor running on the DSP.
 *
 * @param[in] proc_type    Client processor type
 * @param[in] time_offset  Time offset between client processor time domain & QSH time
 *                         domain in nano seconds
 *
 * @return AEEResult
 */
__QAIC_HEADER_EXPORT AEEResult __QAIC_HEADER(sns_remote_proc_state_set_time_offset)(remote_handle64 _h, sns_client_proc_types proc_type, int64_t time_offset) __QAIC_HEADER_ATTRIBUTE;
#ifndef sns_remote_proc_state_URI
#define sns_remote_proc_state_URI "file:///libsns_remote_proc_state_skel.so?sns_remote_proc_state_skel_handle_invoke&_modver=1.0"
#endif /* sns_remote_proc_state_URI */
#ifdef __cplusplus
}
#endif
#endif /* _SNS_REMOTE_PROC_STATE_H */
