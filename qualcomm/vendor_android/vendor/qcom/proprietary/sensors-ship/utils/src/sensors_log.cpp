/*
 * Copyright (c) 2017, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include "sensors_log.h"
#ifdef __ANDROID_API__
#include <android/log.h>
#else
#include <syslog.h>
#endif
#include <stdarg.h>
#include <stdio.h>

namespace sensors_log
{

struct log_config
{
    log_level level = INFO;
    const char *tag = "sensors";
    bool stderr_logging = false;
};

static log_config config;

log_level get_level()
{
    return config.level;
}

void set_level(log_level level)
{
    config.level = level;
}

const char *get_tag()
{
    return config.tag;
}

void set_tag(char const *tag)
{
    config.tag = tag;
}

bool get_stderr_logging()
{
    return config.stderr_logging;
}

void set_stderr_logging(bool val)
{
    config.stderr_logging = val;
}

}

void sns_log_message(int level, const char *fmt, ...){
  va_list args;
  va_start(args, fmt);

  if (sensors_log::config.level >= level) {
#ifdef __ANDROID_API__
    static int android_sensor_map[5] = {ANDROID_LOG_WARN, ANDROID_LOG_ERROR, ANDROID_LOG_INFO, ANDROID_LOG_DEBUG, ANDROID_LOG_VERBOSE};
    __android_log_vprint(android_sensor_map[level], sensors_log::get_tag(), fmt, args);
    if (sensors_log::config.stderr_logging == true) {
      fprintf(stderr, "%s:%d", fmt, "\n", __FUNCTION__, __LINE__, args);
    }
#else
    static int log_level_map[5] = {LOG_WARNING, LOG_ERR, LOG_INFO, LOG_DEBUG, LOG_NOTICE};
    vsyslog(log_level_map[level], fmt, args);
    if (sensors_log::config.stderr_logging == true) {
      vfprintf(stderr, fmt, args);
    }
#endif
  }
  va_end(args);
}
