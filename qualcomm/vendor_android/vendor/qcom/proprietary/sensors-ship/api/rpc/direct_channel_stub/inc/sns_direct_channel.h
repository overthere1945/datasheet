/** ============================================================================
 * @file
 *
 * @brief Defines APIs for using the direct channel to send/receive sensor data
 * from DSP side.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

#ifndef _SNS_DIRECT_CHANNEL_H
#define _SNS_DIRECT_CHANNEL_H

/*==============================================================================
  Include Files
  ============================================================================*/
#include <AEEStdDef.h>
#include <remote.h>
#include <string.h>
#include <stdlib.h>


#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif /*__QAIC_HEADER */

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
  Public Functions
  ============================================================================*/

/**
 * @brief Opens the handle in the specified domain. If this is the first
 * handle, this creates the session.
 *
 * @note Typically this means opening
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
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_direct_channel_open)(const char* uri, remote_handle64* h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Closes a handle. If this is the last handle to close, the session
 * is closed as well, releasing all the allocated resources.
 *
 * @param[in] h  the handle to close
 * @return 0 on success
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_direct_channel_close)(remote_handle64 h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Creates a new direct channel handle on the specified direct channel session
 *
 * @param[in]     _h  direct channel session opened using sns_direct_channel_open()
 * @param[in]     settings  request message, specifying configurations
 * @param[in]     settingsLen  size of request message 'settings'
 * @param[inout]  channel_handle  direct channel handle for streaming sensor data
 *
 * @return 0 on success
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_direct_channel_create)(remote_handle64 _h, const unsigned char* settings, int settingsLen, int* channel_handle) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Sends configuration request for the specified session and channel
 *
 * @param[in]    _h  active direct channel session
 * @param[in]    channel_handle  direct channel handle to be used for streaming sensors
 * @param[in]    config_request  configuration request message to be sent
 * @param[in]    config_requestLen  size of the request message
 *
 * @return 0 on success
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_direct_channel_config)(remote_handle64 _h, int channel_handle, const unsigned char* config_request, int config_requestLen) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Deletes specified direct channel handle on the specified direct channel session
 *
 * @param[in]  _h  direct channel session
 * @param[in]  channel_handle  channel handle to be deleted
 *
 * @return 0 on success
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_direct_channel_delete)(remote_handle64 _h, int channel_handle) __QAIC_HEADER_ATTRIBUTE;

#ifndef sns_direct_channel_URI
#define sns_direct_channel_URI "file:///libsns_direct_channel_skel.so?sns_direct_channel_skel_handle_invoke&_modver=1.0"
#endif /* sns_direct_channel_URI */
#ifdef __cplusplus
}
#endif
#endif /* _SNS_DIRECT_CHANNEL_H */
