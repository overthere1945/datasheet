/*============================================================================
  Copyright (c) 2017,2020, 2023 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor_common.h

  @brief
   which will be accessible to all files.
============================================================================*/

#pragma once
#include <inttypes.h>
#include <string>
#include <cstring>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "usta_rc.h"
//#include "qsh_common.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/dynamic_message.h>

using namespace google::protobuf::io;
using namespace google::protobuf;
using namespace google::protobuf::compiler;


#define UNUSED_VAR(var) var
#ifdef __ANDROID_API__
#define LOG_FOLDER_PATH "/data/vendor/sensors/"
#else
#define LOG_FOLDER_PATH "/etc/sensors/"
#endif
#define PROTO_DELIM '?'
