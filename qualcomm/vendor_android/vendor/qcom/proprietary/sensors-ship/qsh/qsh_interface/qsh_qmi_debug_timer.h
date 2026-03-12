/*
 * Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once
#include "sensors_timer.h"
#include<memory>

using namespace std;

class qsh_qmi_debug_timer{
public:
  qsh_qmi_debug_timer();
  ~qsh_qmi_debug_timer();
private:
  uint32_t get_timer_input_sec();
  bool support_debug_timer();
  unique_ptr<sensors_timer> _timer;
  sensors_timer::notify_callback notify_cb;
  double _delta;
  bool _start_ret;
  bool _stop_ret;
  bool _time_set;

  static const char SENSORS_DEBUG_TIMER_INPUT[];
  static const char SENSORS_DEBUG_TIMER_SUPPORT[];
  static intmax_t last_notify_time_sec;
  static const uint64_t nsec_per_sec = 1000000000ull;
};


