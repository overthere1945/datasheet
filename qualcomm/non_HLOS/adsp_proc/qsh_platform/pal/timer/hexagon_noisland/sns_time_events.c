/** ===========================================================================
  @file sns_time_rcevt.c

  @brief rcevt wrapper APIs.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include "rcevt_qurt.h"

#include "sns_osa_thread_signal.h"

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/
void *sns_time_events_register(char *const rcevt_name,
                         sns_osa_thread_signal *const thread_signal,
                         uint32_t const signal_mask)
{
  return rcevt_register_name_qurt(
      rcevt_name, (qurt_anysignal_t *)(thread_signal), signal_mask);
}

/*----------------------------------------------------------------------------*/
void sns_time_events_unregister(void *handle,
                          sns_osa_thread_signal *const thread_signal,
                          uint32_t const signal_mask)
{
  if(NULL != handle)
  {
    (void)rcevt_unregister_handle_qurt(
        handle, (qurt_anysignal_t *)thread_signal, signal_mask);
    handle = NULL;
  }
}
