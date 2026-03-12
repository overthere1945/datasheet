/*
 * Copyright (c) 2017-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once
#include <inttypes.h>
#include "sensors_log.h"

class sensors_prop {
public:

  bool is_trace_enabled();
  bool get_direct_channel_config();
  bool get_stats_support_flag();
  char get_log_level();
};


