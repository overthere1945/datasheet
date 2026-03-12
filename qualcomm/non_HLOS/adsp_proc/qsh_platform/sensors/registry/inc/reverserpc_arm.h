#pragma once
/*=============================================================================
  @file reverserpc_arm.h

  File reverse rpc wrappers for use by the Sensors Registry.

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

#ifndef _REVERSERPC_ARM_H
#define _REVERSERPC_ARM_H
/// @file reverserpc_arm.idl
///
#include <AEEStdDef.h>
#include "AEEStdDef.h"
#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE
#ifdef __cplusplus
extern "C"
{
#endif
#if !defined(__QAIC_STRING1_OBJECT_DEFINED__) && !defined(__STRING1_OBJECT__)
#define __QAIC_STRING1_OBJECT_DEFINED__
#define __STRING1_OBJECT__
  typedef struct _cstring1_s
  {
    char *data;
    int dataLen;
  } _cstring1_t;

#endif /* __QAIC_STRING1_OBJECT_DEFINED__ */
  __QAIC_HEADER_EXPORT int __QAIC_HEADER(sns_registry_get_property)(
      const char *prop_name, char *prop_value,
      int prop_valueLen) __QAIC_HEADER_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif //_REVERSERPC_ARM_H
