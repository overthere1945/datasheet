#pragma once

/** ===========================================================================
 * @file
 *
 * @brief
 * This file contains sensor clock level information
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/**
 * PRAM wakeup rate threshold to vote for different PRAM clock levels
 */
#define PRAM_WR_MIN_THRESHOLD_HZ      (400)
#define PRAM_WR_LOW_THRESHOLD_HZ      (700)
#define PRAM_WR_MID_THRESHOLD_HZ      (700)
#define PRAM_WR_HIGH_THRESHOLD_HZ     (700)

/**
 *  Primary CPU clock levels
 */
#define PRIMARY_CPU_CLK_HZ_DEFAULT    (0)
#define PRIMARY_CPU_CLK_HZ_MIN        (200 * 1000000)
#define PRIMARY_CPU_CLK_HZ_LOW        (400 * 1000000)
#define PRIMARY_CPU_CLK_HZ_MID        (600 * 1000000)
#define PRIMARY_CPU_CLK_HZ_HIGH       (800 * 1000000)
#define PRIMARY_CPU_CLK_HZ_MAX        (1500 * 1000000)

/**
 * Secondary CPU clock levels
 */
#define SECONDARY_CPU_CLK_HZ_DEFAULT  (19  * 1000000)
#define SECONDARY_CPU_CLK_HZ_MIN      (102 * 1000000)
#define SECONDARY_CPU_CLK_HZ_LOW      (204 * 1000000)
#define SECONDARY_CPU_CLK_HZ_MID      (307 * 1000000)
#define SECONDARY_CPU_CLK_HZ_HIGH     (307 * 1000000)
#define SECONDARY_CPU_CLK_HZ_MAX      (307 * 1000000)

/**
 * SMEM clock levels
 */
#define SMEM_CLK_HZ_DEFAULT          (4   * 1000000)
#define SMEM_CLK_HZ_MIN              (153 * 1000000)
#define SMEM_CLK_HZ_LOW              (204 * 1000000)
#define SMEM_CLK_HZ_MID              (307 * 1000000)
#define SMEM_CLK_HZ_HIGH             (307 * 1000000)
#define SMEM_CLK_HZ_MAX              (307 * 1000000)
