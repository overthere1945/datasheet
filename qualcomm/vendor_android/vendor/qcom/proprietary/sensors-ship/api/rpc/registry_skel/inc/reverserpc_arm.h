/** ============================================================================
 * @file
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/
#ifndef _REVERSERPC_ARM_H
#define _REVERSERPC_ARM_H
/// @file reverserpc_arm.idl
///
#include <AEEStdDef.h>
#include "AEEStdDef.h"
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

#if !defined(__QAIC_STRING1_OBJECT_DEFINED__) && !defined(__STRING1_OBJECT__)
/*==============================================================================
  Type Definitions
============================================================================*/
#define __QAIC_STRING1_OBJECT_DEFINED__
#define __STRING1_OBJECT__
typedef struct _cstring1_s {
   char* data;
   int dataLen;
} _cstring1_t;

#endif /* __QAIC_STRING1_OBJECT_DEFINED__ */

/*==============================================================================
  Public Functions
==============================================================================*/
/**
 * @brief Retrieves property value for the specified property name.
 *
 * @note It is exposed via fastrpc skel utility and can be called from Q6
 *
 * @param[in] prop_name        property name to get the value
 * @param[inout] prop_value    to store the prop value to return value back
 * @param[in] prop_valueLen    buffer length to store prop_value
 *
 * @return 0 success and -1 for failure
 */
__QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_registry_get_property)(char* prop_name, char* prop_value, uint32_t prop_valueLen) __QAIC_HEADER_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif /* _REVERSERPC_ARM_H */
