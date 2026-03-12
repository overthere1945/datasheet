/*
 * Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <inttypes.h>
#include "sensors_log.h"
#include "sensors_timer.h"
#ifdef __ANDROID_API__
#include "utils/SystemClock.h"
#else
#include <time.h>
#endif
using namespace std;

sensors_timer::sensors_timer(notify_callback notify_cb):
    _is_timer_created(false),
    _timeout_secs(0),
    _timeout_nsecs(0)
{
  _notify_cb = notify_cb;
  create();
  sns_logi("%s" , __func__);
}

sensors_timer::~sensors_timer(){
  destroy();
  sns_logi("%s" , __func__);
}

void sensors_timer::notify_user(union sigval arg)
{
  sensors_timer* conn = (sensors_timer*)arg.sival_ptr;
  struct timespec end_monotonic_ts;
  struct timespec end_bootclock_ts;

  if ( 0 != clock_gettime(CLOCK_MONOTONIC, &end_monotonic_ts)) {
    sns_loge("failed to get CLOCK_MONOTONIC");
  }
  if ( 0 != clock_gettime(CLOCK_BOOTTIME , &end_bootclock_ts)) {
    sns_loge("failed to get CLOCK_BOOTTIME");
  }
  intmax_t monotonic_clock_diff = (end_monotonic_ts.tv_sec - conn->_start_monotonic_ts.tv_sec);
  intmax_t boot_clock_diff      = (end_bootclock_ts.tv_sec - conn->_start_bootclock_ts.tv_sec);
  sns_logd("Time diff : %jd Sec w.r.t CLOCK_MONOTONIC" ,monotonic_clock_diff);
  sns_logd("Time diff : %jd Sec w.r.t CLOCK_BOOTTIME" ,boot_clock_diff);

  conn->_notify_cb();
}

bool sensors_timer::set_timeout(intmax_t nsecs){
  if(nsecs > 0){
    _timeout_secs = nsecs/nsec_per_sec;
    _timeout_nsecs = nsecs%nsec_per_sec;
    return true;
  }
  else
    return false;
}

void sensors_timer::create() {
  struct sigevent signal_event;
  int ret = 0;
  signal_event.sigev_notify = SIGEV_THREAD;
  signal_event.sigev_value.sival_ptr = (void *)this;
  signal_event.sigev_notify_function = notify_user;
  signal_event.sigev_notify_attributes = NULL;

  ret = timer_create(CLOCK_MONOTONIC, &signal_event, &_timer_id);
  if (ret == -1){
    sns_loge("sensors_timer not created\n");
    _is_timer_created = false;
    return;
  }
  _is_timer_created = true;
}

void sensors_timer::destroy() {
  if (_is_timer_created) {
    timer_delete(_timer_id);
    _is_timer_created = false;
  }
}

bool sensors_timer::start(){
  if (_is_timer_created){
    if(_timeout_secs || _timeout_nsecs)
    {
      int ret = 0;
      struct itimerspec timer_var;
      timer_var.it_value.tv_sec = _timeout_secs;
      timer_var.it_value.tv_nsec = _timeout_nsecs;
      timer_var.it_interval.tv_sec = 0;
      timer_var.it_interval.tv_nsec = 0;

      ret = timer_settime(_timer_id, 0, &timer_var, 0);
      if (ret == -1) {
        sns_loge("sensors_timer not set\n");
        return false;
      }

      if ( 0 != clock_gettime(CLOCK_MONOTONIC, &_start_monotonic_ts)) {
        sns_loge("failed to get CLOCK_MONOTONIC");
      }
      if ( 0 != clock_gettime(CLOCK_BOOTTIME , &_start_bootclock_ts)) {
        sns_loge("failed to get CLOCK_BOOTTIME");
      }
    }
  }
  else{
    sns_loge("sensors_timer not created\n");
    return false;
  }
#ifdef __ANDROID_API__
  _start_ts_nsec = android::elapsedRealtimeNano();
#else
  struct timespec current_start_ts_nsec;
  clock_gettime(CLOCK_REALTIME, &current_start_ts_nsec);
  _start_ts_nsec =(uint64_t)(current_start_ts_nsec.tv_sec*nsec_per_sec+current_start_ts_nsec.tv_nsec);
#endif
  return true;
}

bool sensors_timer::stop(double &delta){
#ifdef __ANDROID_API__
  _stop_ts_nsec = android::elapsedRealtimeNano();
#else
  struct timespec current_stop_ts_nsec;
  clock_gettime(CLOCK_REALTIME, &current_stop_ts_nsec);
  _stop_ts_nsec =(uint64_t)(current_stop_ts_nsec.tv_sec*nsec_per_sec+current_stop_ts_nsec.tv_nsec);
#endif
  delta = (double)((_stop_ts_nsec - _start_ts_nsec)/nsec_per_sec);

  if(_is_timer_created){
    struct itimerspec time_spec = {};
    int ret = timer_settime(_timer_id, 0, &time_spec, 0);
    if (ret == -1) {
      sns_loge("sensors_timer not disarmed \n");
      return false;
    }
  }
  else{
      sns_loge("sensors_timer not created\n");
      return false;
    }
  return true;
}
