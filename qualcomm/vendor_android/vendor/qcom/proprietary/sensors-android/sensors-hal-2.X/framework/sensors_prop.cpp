/*
 * Copyright (c) 2017-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cutils/properties.h>
#include <unordered_map>
#include "sensors_prop.h"

#define SENSORS_HAL_PROP_SUPPORT_DIRECT_CHANNEL "persist.vendor.sensors.support_direct_channel"
#define SENSORS_HAL_PROP_TRACE "persist.vendor.sensors.debug.trace"
#define SENSORS_HAL_PROP_STATS "persist.vendor.sensors.debug.stats"
#define SENSORS_HAL_PROP_DEBUG "persist.vendor.sensors.debug.hal"

using namespace std;

char sensors_prop::get_log_level()
{
    char debug_prop[PROPERTY_VALUE_MAX];
    int len;
    len = property_get(SENSORS_HAL_PROP_DEBUG, debug_prop, "i");
    sns_logi("log_level: %c", debug_prop[0]);
    return debug_prop[0];
}

bool sensors_prop::is_trace_enabled(){
  char trace_debug[PROPERTY_VALUE_MAX];
  property_get(SENSORS_HAL_PROP_TRACE, trace_debug, "false");
  sns_logd("trace_debug: %s",trace_debug);
  if (!strncmp("true", trace_debug,4)) {
    sns_logi("support_trace_debug : %s",trace_debug);
    return true;
  }
  return false;
}

bool sensors_prop::get_direct_channel_config(){
  char property[PROPERTY_VALUE_MAX];
  property_get(SENSORS_HAL_PROP_SUPPORT_DIRECT_CHANNEL, property, "true");
  sns_logd("support_direct_channel: %s",property);
  if (!strncmp("false", property,5)) {
    sns_logi("support_direct_channel : %s", property);
    return false;
  }
  return true;
}

bool sensors_prop::get_stats_support_flag(){
  char stats_debug[PROPERTY_VALUE_MAX];
  property_get(SENSORS_HAL_PROP_STATS, stats_debug, "false");
  sns_logd("latency_debug: %s",stats_debug);
  if (!strncmp("true", stats_debug,4)) {
    sns_logi("support_latency_debug : %s",stats_debug);
    return true;
  }
  return false;
}

