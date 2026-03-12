/** =============================================================================
 *  @file see_salt_sleep.cpp
 *
 *  @brief sleep_and_awake() called by main thread to sleep for
 *  milliseconds then if necessary wakeup the apps processor
 *  from suspend mode and resume.
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  All rights reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 *  ===========================================================================
 */
#include <iostream>
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstdint>

/**
 * @brief sleep_and_awake() called by main thread to sleep for milliseconds then
 *        if necessary, wakeup the apps processor from suspend mode
 * @param milliseconds
 */
void sleep_and_awake( uint32_t milliseconds)
{
   int fd = timerfd_create(CLOCK_BOOTTIME_ALARM, 0);
   if (fd == -1)
   {
      float secs = milliseconds / 1000.0;
      int sleep_secs = static_cast<int>(secs);

      uint32_t microseconds = static_cast<int>((secs - sleep_secs) * 1000000);
      std::cout << "WARNING: Using sleep() as a fallback. App may sleep for longer durations if the CPU goes to sleep." << std::endl;
      ::sleep(sleep_secs);
      usleep(microseconds); // Sleep for the remaining microseconds
   }
   else
   {
      struct timespec now;
      struct itimerspec new_value;
      uint64_t expired;
      ssize_t s;
      uint32_t secs = milliseconds / 1000; // sleep seconds
      long nsecs = ( milliseconds - (secs * 1000)) * 1000000; // sleep nanoseconds

      try
      {
         if (clock_gettime(CLOCK_BOOTTIME_ALARM, &now) == -1)
            throw std::runtime_error("timerfd clock_gettime");

         new_value.it_interval.tv_sec = 0;
         new_value.it_interval.tv_nsec = 0;
         new_value.it_value.tv_sec = now.tv_sec + secs;
         new_value.it_value.tv_nsec = now.tv_nsec + nsecs;
         if ( new_value.it_value.tv_nsec > 1000000000 ) {
            new_value.it_value.tv_nsec -= 1000000000;
            new_value.it_value.tv_sec++;
         }

         if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
            throw std::runtime_error("timerfd_settime");

         s = read(fd, &expired, sizeof(uint64_t));
         if (s != sizeof(uint64_t))
            throw std::runtime_error("timerfd read");
      }
      catch (std::runtime_error& e)
      {
         if (close(fd) != 0) {
            std::string msg(e.what());
            throw std::runtime_error(msg + " | timerfd clock_gettime");
         }
         throw;
      }

      if (close(fd) != 0)
         throw std::runtime_error("timerfd close");
   }
}

