/*
 * Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once
#include <time.h>
#include <functional>
#include <signal.h>
#include <cstdint>



class sensors_timer{
public:
  using notify_callback = std::function<void()>;
  /**
   * @brief creates a POSIX timer.
   *
   * @param:  in value which points to a function pointer
   *          to be called when timer expires.
   * All @param are mandatory.
   */
  sensors_timer(notify_callback notify_cb);

  /**
   * @brief deletes the created timer.
   */
  ~sensors_timer();

  /**
   * @brief starts the timer, if it is created.
   *
   * @out: true upon success & false upon failure.
   */
  bool start();

  /**
   * @brief stops the timer, if it is created
   *        and computes the time elapsed.
   *
   * @param: out, delta gives the time taken between start and stop in seconds.
   *
   * @out: true upon success & false upon failure.
   */
  bool stop(double &delta);

  /**
    * @brief set the timeout value in nanoseconds
    *
    * @param: in, timeout value in nanoseconds after which timer expires.
    *         Should be non-zero.
    *
    * @out: true upon success & false upon failure.
    */
  bool set_timeout(intmax_t nsecs);
private:
  void create();
  void destroy();
  static void notify_user(union sigval arg);

  timer_t           _timer_id;
  bool              _is_timer_created;
  struct timespec   _start_monotonic_ts;
  struct timespec   _start_bootclock_ts;
  intmax_t          _timeout_secs;
  intmax_t          _timeout_nsecs;
  uint64_t          _start_ts_nsec;
  uint64_t          _stop_ts_nsec;
  notify_callback   _notify_cb;

  static const uint64_t nsec_per_sec = 1000000000ull;
};
