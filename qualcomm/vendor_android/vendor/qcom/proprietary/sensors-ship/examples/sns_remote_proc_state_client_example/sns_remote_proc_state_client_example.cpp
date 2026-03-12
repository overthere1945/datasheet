/******************************************************************************
* File: sns_remote_proc_state_client_example.cpp
*
* Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/

#include <inttypes.h>
#ifdef __ANDROID_API__
#include <utils/Log.h>
#else
#include <log.h>
#endif
#include "AEEStdErr.h"
#include "sns_remote_proc_state.h"
#ifdef __ANDROID_API__
#include "utils/SystemClock.h"
#endif
#include <time.h>

/*=============
    Macros
  =============*/

/* This is platform specific code, so make sure that we are compiling
   for correct architecture */
#if defined(__aarch64__)
  #define TARGET_ARM64
#elif defined(__arm__)
  #define TARGET_ARM
#else
  #error "target cpu architecture not supported"
#endif

/*==============
    Static Data
  ==============*/

static remote_handle64 remote_handle_fd = 0;
static remote_handle64 remote_proc_handle = -1;
static int64_t time_offset_ns = 0;
const uint64_t NSEC_PER_SEC = 1000000000ull;


/*============================
 * Helper functions
 * ==========================*/

/**
 * @brief    Open a remote fRPC handle in the specified domain
 */
void fastRPC_remote_handle_init(){
  printf("get_fastRPC_remote_handle Start\n");
  if (remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=adsp", &remote_handle_fd)) {
    printf("failed to open remote handle for sensorspd - adsp \n");
  }
  printf("get_fastRPC_remote_handle End\n");
}


/**
 * @brief open remote_proc session
 * @return remote_handle64 handle for communication to DSP
 **/
static remote_handle64 remote_proc_state_open(){
  int nErr = AEE_SUCCESS;
  char *uri = sns_remote_proc_state_URI "&_dom=adsp";
  remote_handle64 handle_l;

  if (AEE_SUCCESS == (nErr = sns_remote_proc_state_open(uri, &handle_l))) {
    printf("sns_remote_proc_state_open success for sensorspd - handle_l is %ud \n" , (unsigned int)handle_l);
  } else {
    printf("sns_remote_proc_state_open failed for sensorspd - handle_l is %ud \n" , (unsigned int)handle_l);
  }
  printf("test_remote_proc_state_open() End \n");

  return handle_l;
}


/**
 * @brief convert the qtimer tick value of nanoseconds
 * @param ticks
 * @return uint64_t qtimer time in nanoseconds
 */
static uint64_t qtimer_ticks_to_ns(uint64_t ticks)
{
#if defined(TARGET_ARM64)
    uint64_t qtimer_freq = 0;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (qtimer_freq));
#else
    uint32_t qtimer_freq = 0;
    asm volatile("mrc p15, 0, %[val], c14, c0, 0" : [qtimer_freq] "=r" (qtimer_freq));
#endif
    return uint64_t(double(ticks) * (double(NSEC_PER_SEC) / double(qtimer_freq)));
}


/**
 * @brief reads the current QTimer count value
 * @return uint64_t QTimer tick-count
 */
static uint64_t qtimer_get_ticks()
{
#if defined(TARGET_ARM64)
  unsigned long long val = 0;
  asm volatile("mrs %0, cntvct_el0" : "=r" (val));
  return val;
#else
  uint64_t val;
  unsigned long lsb = 0, msb = 0;
  asm volatile("mrrc p15, 1, %[lsb], %[msb], c14"
               : [lsb] "=r" (lsb), [msb] "=r" (msb));
  val = ((uint64_t)msb << 32) | lsb;
  return val;
#endif
}


/**
 * @brief calculate time offset in nanoseconds
 * @return int64_t time_offset value
 */
static int64_t calculate_time_offset()
{
  printf("calculate_time_offset start \n");
  intmax_t ts_offset = 0;
  uint64_t qtimer_ticks = 0;
  uint64_t qtimer_ns = 0;

  qtimer_ticks = qtimer_get_ticks();
  qtimer_ns = qtimer_ticks_to_ns( qtimer_ticks );

#ifdef __ANDROID_API__
  intmax_t current_time_nsec = android::elapsedRealtimeNano();
#else
  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  intmax_t current_time_nsec = (intmax_t)current_time.tv_nsec;
#endif
  ts_offset = current_time_nsec - (int64_t)qtimer_ns;

  printf("calculate_time_offset: qtimer_ns= %" PRIu64 " realtime_ns= %" PRId64 " current_offset=(%" PRId64 ") \n",
         qtimer_ns, current_time_nsec, ts_offset);

  return ts_offset;
}


/**
 * @brief   sends the time offset to remote proc state sensor running on the DSP.
 * @param   handle_l :  handle opened for communication with DSP
 * @param   time_offset_ns :  offset calculated in nanoseconds
 */
static void send_time_offset(remote_handle64 handle_l, int64_t time_offset_ns )
{

  int nErr = AEE_SUCCESS;
  sns_client_proc_types proc_type = SNS_CLIENT_PROC_APSS;

  printf("send_time_offset() Start \n");
  printf("send_time_offset() proc_type=(%d), time_offset_ns(%" PRId64 ") \n", (int)proc_type, time_offset_ns);

  if( AEE_SUCCESS != (nErr = sns_remote_proc_state_set_time_offset(handle_l, proc_type, time_offset_ns)))
  {
    printf("sns_remote_proc_state_set_time_offset() FAILED, RC 0x%d, handle_l %lu\n", nErr, handle_l);
  }
  else {
    printf("sns_remote_proc_state_set_time_offset() SUCCESS \n");
  }
  printf("send_time_offset() End \n");
}


/**
 * @brief   Closes the handle. If this is the last handle to close, the session is closed as well.
 * @param   handle_l :  handle opened for communication with DSP
 */
void remote_proc_state_close(remote_handle64 handle_l)
{
  int nErr = AEE_SUCCESS;

  ALOGE("-------------------\n  closing sns_remote_proc_state \n ---------------------");
  if (AEE_SUCCESS != (nErr = sns_remote_proc_state_close(handle_l))) {
    ALOGE("ERROR 0x%x: Failed to close handle\n", nErr);
  }

  if(0 != remote_handle_fd)
    remote_handle64_close(remote_handle_fd);

  return;
}


int main( void ) {
  /* ==============================
   * This is an example code and may be used as a reference for using the
   * fRPC APIs to communicate to the remote proc sensor present at DSP side.
   *
   * For being able to send time offset,
   * create a fRPC handle ->  create remote proc handle  ->  calculate the offset, send the offset  ->
   * close remote proc handle  ->  close fRPC handle
   * ==============================*/


  // initialize fRPC handle
  fastRPC_remote_handle_init();

  // init remote_proc_state_open handle
  remote_proc_handle = remote_proc_state_open();

  // calculate time offset
  time_offset_ns = calculate_time_offset();
  printf("current time offset is %ld \n", time_offset_ns);

  // send Time offset to DSP
  send_time_offset(remote_proc_handle, time_offset_ns );

  // close sns_remote_proc_state handle
  remote_proc_state_close(remote_proc_handle);

  return 0;
}
