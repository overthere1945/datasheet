#pragma once

/** ===========================================================================
 * @file
 * sns_island_config.h
 *
 * @brief
 * This file contains sensor island configuration.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

/*==============================================================================
  Include Files
  ============================================================================*/

#include "uSleep_islands.h"

/*=============================================================================
  Macros
  ============================================================================*/

/*----------------------------------------------------------------------------
  Island Pool names
  ----------------------------------------------------------------------------*/

/*
 * The name of island pool for island memory allocation in "qsh" memory segment
 */
#define QSH_ISLAND_POOL                   "SSC_ISLAND_POOL"

/*
 * The name of island pool for island memory allocation in "ssc" memory segment
 */
#define SSC_ISLAND_POOL                   "SSC_ISLAND_POOL"

/*
 * The name of island pool for island memory allocation in "qshtech" memory segment
 */
#define QSHTECH_ISLAND_POOL               "SSC_ISLAND_POOL"

/*----------------------------------------------------------------------------
  Island mode configuration
  ----------------------------------------------------------------------------*/
/*
 * Sensor framework island mode configuration.
 * This is applicable to code placed in "qsh" memory segment.
 */

#define QSH_USLEEP_ISLAND                 ( USLEEP_ISLAND_ALL_SNS )

/*
 * Sensor island mode configuration.
 * This is applicable to code placed in "ssc" memory segment
 */

#define SNS_USLEEP_ISLAND                 ( USLEEP_ISLAND_ALL_SNS )

/*
 * QSHTECH island mode configuration.
 * This is applicable to code placed in "qshtech" memory segment
 */
#define QSHTECH_USLEEP_ISLAND             ( USLEEP_ISLAND_ALL_SNS )


/*=============================================================================
  Type Definitions
  ============================================================================*/
