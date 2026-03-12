/*
 * Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cstring>
#include "qsh_qmi_debug_timer.h"
#include "sensors_log.h"
#include "sensors_ssr.h"
#ifdef __ANDROID_API__
#include <cutils/properties.h>
#include "utils/SystemClock.h"
#else
#include "properties.h"
#include <time.h>
#endif

const char qsh_qmi_debug_timer::SENSORS_DEBUG_TIMER_INPUT[] ="persist.vendor.sensors.debug.timer.inputsec";
const char qsh_qmi_debug_timer::SENSORS_DEBUG_TIMER_SUPPORT[] = "persist.vendor.sensors.debug.timer";
intmax_t qsh_qmi_debug_timer::last_notify_time_sec = 0;

uint32_t qsh_qmi_debug_timer::get_timer_input_sec(){
  char property[PROP_VALUE_MAX];
  uint32_t time = 0;
  property_get(SENSORS_DEBUG_TIMER_INPUT, property, "12");
  time = atoi(property);
  sns_logd("qmi debug timer value in secs : %u",time);
  return time;
}

bool qsh_qmi_debug_timer::support_debug_timer()
{
  char debug_prop[PROP_VALUE_MAX];
  property_get(SENSORS_DEBUG_TIMER_SUPPORT, debug_prop, "false");
  sns_logd("debug_prop: %s",debug_prop);
  if (!strncmp("true", debug_prop, 4)) {
    sns_logi("support_debug_timer: %s",debug_prop);
    return true;
  }
  return false;
}

qsh_qmi_debug_timer::qsh_qmi_debug_timer(){
  static const bool _enable_timer = support_debug_timer();
  if(false == _enable_timer){
    return;
  }
  static const intmax_t secs = get_timer_input_sec();

  notify_cb = [](){
#ifdef __ANDROID_API__
    intmax_t current_time_sec = (android::elapsedRealtime())/1000;
#else
    struct timespec _current_time_sec;
    clock_gettime(CLOCK_REALTIME, &_current_time_sec);
    intmax_t current_time_sec = (intmax_t)_current_time_sec.tv_sec;
#endif
    if((current_time_sec - last_notify_time_sec) > secs) {
      last_notify_time_sec = current_time_sec;
      sns_logi("Triggering SSR");
      trigger_ssr();
    }
    else{
      sns_loge("ssr trigger can't happen before %jd secs",secs);
    }
  };

  _timer = unique_ptr<sensors_timer>(new sensors_timer(notify_cb));
  if(nullptr == _timer) {
   return;
  }
  _time_set = _timer->set_timeout(secs*nsec_per_sec);
  _start_ret = _timer->start();

  if(!_start_ret || !_time_set){
    sns_logi("Debug timer could not start!");
  }
}

qsh_qmi_debug_timer::~qsh_qmi_debug_timer(){
  if(_timer){
    _stop_ret = _timer->stop(_delta);
    if(!_stop_ret){
      sns_loge("Debug timer could not be stopped!");
    }
    sns_logd("Time taken to complete :: %lf secs",_delta);
    sns_logd("%s" , __func__);
  }
}
