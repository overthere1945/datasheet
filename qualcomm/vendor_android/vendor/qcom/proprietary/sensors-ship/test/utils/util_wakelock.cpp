/*
 * Copyright (c) 2017-2021, 2023-2024 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <cstring>
#include <cstdlib>        // EXIT_SUCCESS/EXIT_FAILURE
#include <iostream>
#include <sstream>
#include <string>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "util_wakelock.h"
#include "sensors_log.h"

const char* UTIL_SENSORS_WACKLOCK_NAME = "SensorsTest_WAKEUP";
#define MAX_FILENAME_LEN 32
char unique_fn[MAX_FILENAME_LEN];
#define UTIL_POWER_WAKE_lOCK_ACQUIRE_INDEX 0
#define UTIL_POWER_WAKE_lOCK_RELEASE_INDEX 1
const char * const PATHS[] = {
    /*to hold wakelock*/
    "/sys/power/wake_lock",
    /*to release wakelock*/
    "/sys/power/wake_unlock",
};

// each wakelock instance gets a unique filename
void create_unique_filename()
{
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) < 0) {
        cout << "FAIL wakelock.clock_gettime() failed" << endl;
        exit(EXIT_FAILURE);
    }

    tp.tv_sec &= 0xffff; // unique ts for ~18 hours
    ostringstream oss;
    oss << "SensorsTest_" << tp.tv_sec << "_" << tp.tv_nsec;
    string fn = oss.str();
    strlcpy((char *)unique_fn, fn.c_str(), sizeof(unique_fn));
    UTIL_SENSORS_WACKLOCK_NAME = (char *)unique_fn;
}

util_wakelock* util_wakelock::_self= nullptr;

util_wakelock* util_wakelock::get_instance()
{
  if(nullptr != _self) {
    return _self;
  }
  else {
    _self = new util_wakelock();
    create_unique_filename();
    return _self;
  }
}

util_wakelock::util_wakelock()
{
  _is_held = false;
  _accu_count = 0;
  for (int i=0; i<FD_COUNT; i++) {
    _fds[i] = -1;
    int fd = open(PATHS[i], O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      sns_loge("fatal error opening \"%s\": %s\n", PATHS[i],
          strerror(errno));
    }
    _fds[i] = fd;
  }
}

util_wakelock::~util_wakelock()
{
  std::lock_guard<std::mutex> lock(_mutex);
  if(true == _is_held) {
    _accu_count = 0;
    release();
  }

  for (int i=0; i<FD_COUNT; i++) {
    if (_fds[i] >= 0)
      close(_fds[i]);
  }
}
void util_wakelock::wait_for_held()
{
  std::unique_lock<std::mutex> lock(_mutex);
  if(false == _is_held) {
    _cv.wait(lock);
  }
}
char * util_wakelock::get_n_locks(unsigned int _in_count)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (_in_count > std::numeric_limits<unsigned int>::max() - _accu_count) {
      sns_loge("_in_count %d will overflow max of _accu_count %u", _in_count, _accu_count);
      return nullptr;
  }
  if(_accu_count == 0 && false == _is_held)
    acquire();
  _accu_count = _accu_count + _in_count;
  return unique_fn;
}

char * util_wakelock::put_n_locks(unsigned int _in_count)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (_in_count > _accu_count) {
      sns_loge("_in_count %d is larger than _accu_count %u", _in_count, _accu_count);
      return nullptr;
  }
  _accu_count = _accu_count - _in_count;
  if(_accu_count == 0 && true ==_is_held)
    release();
  return unique_fn;
}
void util_wakelock::put_all() {
  std::lock_guard<std::mutex> lock(_mutex);
  if(true == _is_held) {
    _accu_count = 0;
    release();
  }
}

void util_wakelock::acquire()
{
  if (_fds[UTIL_POWER_WAKE_lOCK_ACQUIRE_INDEX] >= 0) {
    if (write( _fds[UTIL_POWER_WAKE_lOCK_ACQUIRE_INDEX],UTIL_SENSORS_WACKLOCK_NAME, strlen(UTIL_SENSORS_WACKLOCK_NAME)+1) > 0) {
      sns_logv("sucess wakelock acquire:%s", UTIL_SENSORS_WACKLOCK_NAME);
      _is_held = true;
      _cv.notify_one();
    } else {
      sns_loge("write fail wakelock acquire:%s", UTIL_SENSORS_WACKLOCK_NAME);
    }
  } else {
    sns_loge("Not able to open fd for wakeup:%s", UTIL_SENSORS_WACKLOCK_NAME);
  }
}

void util_wakelock::release()
{
  if (_fds[UTIL_POWER_WAKE_lOCK_RELEASE_INDEX] >= 0) {
    if (write( _fds[UTIL_POWER_WAKE_lOCK_RELEASE_INDEX],UTIL_SENSORS_WACKLOCK_NAME, strlen(UTIL_SENSORS_WACKLOCK_NAME)+1) > 0) {
      sns_logv("sucess release %s wakelock", UTIL_SENSORS_WACKLOCK_NAME);
      _is_held = false;
    } else {
      sns_loge("failed in write:%s wake unlock", UTIL_SENSORS_WACKLOCK_NAME);
    }
  } else {
    sns_loge("Not able to open fd for wakeup:%s", UTIL_SENSORS_WACKLOCK_NAME);
  }
}
