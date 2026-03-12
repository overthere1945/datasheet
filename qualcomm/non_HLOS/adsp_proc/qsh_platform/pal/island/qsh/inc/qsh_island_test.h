#pragma once
/*=============================================================================
  @file qsh_island_test.h

  Header file for the island test utility

@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*============================================================================
  INCLUDES
  ============================================================================*/

/*============================================================================
  Macros
  ============================================================================*/

#define ISLAND_TEST_PASS_PERCENTAGE 75

// particular test will be calculated
#define ISLAND_STABILISATION_PERIOD_MS 4000

/*============================================================================
  Types
  ============================================================================*/

/*============================================================================
  Functions
  ============================================================================*/

/*!
  -----------------------------------------------------------------------------
  qsh_island_test_update_island_exit_count()
  -----------------------------------------------------------------------------

  @brief API to update island exit count used for island test.

  @return
  -----------------------------------------------------------------------------
*/
void qsh_island_test_update_island_exit_count(void);

/*!
  -----------------------------------------------------------------------------
  qsh_island_test_validate()
  -----------------------------------------------------------------------------

 * @brief This function logs island transitions for debug
 *
 * @param[i] cookie        island log cookie
 * @param[i] island_time   Time spent in island (us)

  @return
  -----------------------------------------------------------------------------
*/

void qsh_island_test_validate(uint64_t cookie, uint64_t island_time);

/*----------------------------------------------------------------------------*/
