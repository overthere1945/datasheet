/** ============================================================================
 * @file
 *
 * @brief Defines APIs used to communicate to device mode sensors
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

#ifndef _SNS_DEVICE_MODE_H
#define _SNS_DEVICE_MODE_H
/// @file sns_device_mode.idl
///

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
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_device_mode_open)(const char* uri, remote_handle64* h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Closes a handle. If this is the last handle to close, the session
 * is closed as well, releasing all the allocated resources.
 *
 * @param[in] h  the handle to close
 * @return 0 on success, should always succeed
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_device_mode_close)(remote_handle64 h) __QAIC_HEADER_ATTRIBUTE;


/**
 * @brief Set the device mode
 *
 * @param[in]     h  handle opened using sns_device_mode_open()
 * @param[in]     vec  request message, specifying DEVICE_MODE_NUMBER & DEVICE_MODE_NUMBER_STATE
 * @param[in]     vecLen  size of request message
 * @param[inout]  result  outcome of the sent request
 *
 * @return 0 on success, should always succeed
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_device_mode_set)(remote_handle64 _h, const char* vec, int vecLen, uint32* result) __QAIC_HEADER_ATTRIBUTE;

#ifndef sns_device_mode_URI
#define sns_device_mode_URI "file:///libsns_device_mode_skel.so?sns_device_mode_skel_handle_invoke&_modver=1.0"
#endif /* sns_device_mode_URI */
#ifdef __cplusplus
}
#endif
#endif /* _SNS_DEVICE_MODE_H */
