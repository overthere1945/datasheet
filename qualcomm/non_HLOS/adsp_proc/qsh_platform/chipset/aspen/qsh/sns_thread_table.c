/** ===========================================================================
 * @file
 * sns_thread_table.c
 *
 * @brief
 * his file has definition of Sensor thread configuation table .
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include <string.h>

#include "sns_thread_config.h"
#include "sns_thread_table.h"
#include "sns_types.h"

/*=============================================================================
  Macros
  ===========================================================================*/

#ifdef SNS_ISLAND_INCLUDE_DIAG
#define SNS_DIAG_OSA_MEM_TYPE SNS_OSA_MEM_TYPE_ISLAND
#else
#define SNS_DIAG_OSA_MEM_TYPE SNS_OSA_MEM_TYPE_NORMAL
#endif

#ifdef SNS_ISLAND_INCLUDE_UART
#define SNS_UART_OSA_MEM_TYPE SNS_OSA_MEM_TYPE_ISLAND_SSC
#else
#define SNS_UART_OSA_MEM_TYPE SNS_OSA_MEM_TYPE_NORMAL
#endif

#define SNS_BASE_THREAD_PRIORITY   0x20
#define SNS_IRQ_THREAD_PRIORITY    0x1E //(Same Priority as GPI)

// If SSC island heap size is zero, this condition will replace the Mem type with Island heap
#if !SNS_ISLAND_SSC_HEAP_ALLOC
#define SNS_OSA_MEM_TYPE_ISLAND_SSC SNS_OSA_MEM_TYPE_ISLAND
#endif

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/*=============================================================================
  Constant Data Definitions
  ============================================================================*/
/*
 *  Sensor thread configuration table. [Sorted by Thread priority]
 */
static const sns_thread_config sns_thread_table[] =
{
//  Name                      Stack         Kernel_stack   Priority                           Mem type                     Watchdog timeout
  { SNS_IRQ_THREAD,           (2 * 1024),   (2 * 1024),    SNS_IRQ_THREAD_PRIORITY,           SNS_OSA_MEM_TYPE_ISLAND_SSC, SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_LOW_MEM_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x00,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_ES_LOW_MEM_THREAD,    (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x00,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_TIMER_THREAD,         (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x01,     SNS_OSA_MEM_TYPE_ISLAND_SSC, SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_SIG_I_THREAD,         (6 * 1024),   (6 * 1024),    SNS_BASE_THREAD_PRIORITY+0x02,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_RPS_THREAD,           (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x58,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SEE_H_0_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x60,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_SEE_H_1_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x60,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_CCD_SERVICE_THREAD,   (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x60,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_SIM_DATA_THREAD,      (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x60,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_HI_TIMEOUT_SEC },
  { SNS_CCD_SWI_READ_THREAD,  (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x61,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_QMI_CM_THREAD,        (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x61,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_QSOCKET_CM_THREAD_I,  (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x61,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_QSOCKET_CM_THREAD_N,  (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x61,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SEE_L_0_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x62,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SEE_L_1_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x62,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SEE_L_2_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x62,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SEE_L_3_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x62,     SNS_OSA_MEM_TYPE_ISLAND,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SIM_DLF_THREAD,       (4 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x62,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_REGISTRY_THREAD,      (6 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x69,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_SIG_NI_THREAD,        (6 * 1024),   (6 * 1024),    SNS_BASE_THREAD_PRIORITY+0x69,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_MQ_SRV_THREAD,        (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x69,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_MQ_CLT_THREAD,        (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0x69,     SNS_OSA_MEM_TYPE_NORMAL,     SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_ASYNC_UART_THREAD,    (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0xC8,     SNS_UART_OSA_MEM_TYPE,       SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
  { SNS_DIAG_THREAD,          (2 * 1024),   (2 * 1024),    SNS_BASE_THREAD_PRIORITY+0xcf,     SNS_DIAG_OSA_MEM_TYPE,       SNS_THREAD_PRIO_LO_TIMEOUT_SEC },
};

/*=============================================================================
  Public function Definitions
  ============================================================================*/

sns_thread_config *sns_osa_get_thread_config(char *thread_name)
{
  sns_thread_config *thread_config = NULL;
  for(int i = 0; i < ARR_SIZE(sns_thread_table); i++)
  {
    if(0 == strcmp(&sns_thread_table[i].name[0], thread_name))
    {
      thread_config = (sns_thread_config *)&sns_thread_table[i];
      break;
    }
  }
  return thread_config;
}
/*----------------------------------------------------------------------------*/
