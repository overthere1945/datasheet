/*============================================================================
  @file qsh_island_test.c

  @brief
  API definitions to debug qsh island test.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

#if defined(SSC_TARGET_X86)
#include <stdio.h>
#endif
#include <stdbool.h>

#include "qsh_island_hexagon.h"
#include "qsh_island_test.h"
#include "sns_assert.h"
#include "sns_diag.pb.h"
#include "sns_fw_diag_types.h"
#include "sns_fw_diag_service.h"
#include "sns_island_util.h"
#include "sns_memmgr.h"
#include "sns_osa_lock.h"
#include "sns_printf_int.h"
#include "sns_time.h"

/*============================================================================
  type definitions
  ============================================================================*/

typedef struct qsh_island_test_debug_t
{
  sns_time start;
  sns_time end;
  uint64_t cookie;
  uint64_t test_time_us;
  uint64_t test_island_time_us;
  uint64_t island_time_start;
  uint32_t test_island_percentage;
  uint32_t test_island_exits;
  bool test_in_progress;

} qsh_island_test_debug_t;

typedef struct qsh_island_log_t
{
  qsh_island_test_debug_t *island_test;
  bool enable_island_test;

} qsh_island_test_t;

/*============================================================================
  Static Data
  ============================================================================*/

qsh_island_test_t qsh_island_test SNS_ISLAND_DATA;

/*============================================================================
  Static Functions
  ============================================================================*/

/*============================================================================
  public Functions
  ============================================================================*/

SNS_ISLAND_CODE
void qsh_island_test_validate(uint64_t cookie, uint64_t island_time)
{
  if(true == qsh_island_test.enable_island_test)
  {
    if(false == qsh_island_test.island_test->test_in_progress)
    {
      qsh_island_test.island_test->test_in_progress = true;
      qsh_island_test.island_test->cookie = cookie;
      qsh_island_test.island_test->start = sns_get_system_time();
      qsh_island_test.island_test->island_time_start = island_time;
      qsh_island_test.island_test->test_island_exits = 0;
    }
    else
    {
      if(qsh_island_test.island_test->cookie == cookie)
      {
        sns_time test_time_ticks;
        qsh_island_test.island_test->end = sns_get_system_time();
        qsh_island_test.island_test->test_in_progress = false;

        test_time_ticks = qsh_island_test.island_test->end -
                          qsh_island_test.island_test->start;

        qsh_island_test.island_test->test_time_us =
            (sns_get_time_tick_resolution_in_ps() * test_time_ticks) / 1000000;

        qsh_island_test.island_test->test_island_time_us =
            island_time - qsh_island_test.island_test->island_time_start;

        qsh_island_test.island_test->test_island_percentage =
            (qsh_island_test.island_test->test_island_time_us * 100) /
            qsh_island_test.island_test->test_time_us;

        SNS_UPRINTF(HIGH, sns_fw_printf, "island test: island_percentage=(%d)",
                    qsh_island_test.island_test->test_island_percentage);
        SNS_UPRINTF(HIGH, sns_fw_printf, "island test: island_exits=(%d)",
                    qsh_island_test.island_test->test_island_exits);

#if defined(SSC_TARGET_X86)
        float island_pct_f =
            (qsh_island_test.island_test->test_island_time_us * 100.0) /
            qsh_island_test.island_test->test_time_us;
        FILE *file_ptr = fopen("./sim_island_analysis.txt", "w");
        if(NULL != file_ptr)
        {
          fprintf(file_ptr, "Island Percentage: %.3f\n", island_pct_f);
          fprintf(file_ptr, "Island time (us) : %llu\n",
                  qsh_island_test.island_test->test_island_time_us);
          fprintf(file_ptr, "Test time (us) : %llu\n",
                  qsh_island_test.island_test->test_time_us);
          fprintf(file_ptr, "Island Exits : %u\n",
                  qsh_island_test.island_test->test_island_exits);
          fflush(file_ptr);
          fclose(file_ptr);
        }
#endif
        SNS_ASSERT(ISLAND_TEST_PASS_PERCENTAGE <=
                   qsh_island_test.island_test->test_island_percentage);
      }
    }
  }
}
/*----------------------------------------------------------------------------*/
SNS_ISLAND_CODE
void qsh_island_test_update_island_exit_count(void)
{
#if defined(SSC_TARGET_X86)
  if(qsh_island_test.enable_island_test &&
     NULL != qsh_island_test.island_test &&
     qsh_island_test.island_test->test_in_progress)
  {
    uint64_t time_since_start_ms =
        sns_get_time_tick_resolution_in_ps() *
        (sns_get_system_time() - qsh_island_test.island_test->start) /
        1000000000;
    if(time_since_start_ms > ISLAND_STABILISATION_PERIOD_MS)
    {
      qsh_island_test.island_test->test_island_exits++;
      SNS_UPRINTF(HIGH, sns_fw_printf, "island exit count %d ",
                  qsh_island_test.island_test->test_island_exits);
    }
  }
#endif
}
/*----------------------------------------------------------------------------*/
SNS_ISLAND_CODE void
sns_island_configure_island_test_debug(bool enable_island_test)
{
  if(true == enable_island_test)
  {
    if(NULL == qsh_island_test.island_test)
    {
      qsh_island_test.island_test =
          sns_malloc(SNS_HEAP_ISLAND, sizeof(qsh_island_test_debug_t));

      if(NULL != qsh_island_test.island_test)
      {
        qsh_island_test.enable_island_test = true;
      }
    }
  }
}
/*----------------------------------------------------------------------------*/
