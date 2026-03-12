#pragma once

/** ===========================================================================
 * @file
 * sns_interface_defs.h
 *
 * @brief
 * This file contains the physical sensor interface definitions.
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * ============================================================================*/

// For STENG1AX driver
  #define _STENG1AX_IRQ_NUM_0 0
  #define _STENG1AX_IRQ_NUM_1 0
  #define _STENG1AX_IRQ_NUM_2 0
  #define _STENG1AX_IRQ_NUM_3 0
  #define _STENG1AX_BUS_INSTANCE 4
  #define _STENG1AX_BUS_INSTANCE_1 2
  #define _STENG1AX_BUS_TYPE 3
  #define _STENG1AX_BUS_FREQ_MIN 400
  #define _STENG1AX_BUS_FREQ_MAX 12500
  #define _STENG1AX_I2C_ADDR_0 32
  #define _STENG1AX_I2C_ADDR_1 33
  #define _STENG1AX_RAIL_1 "ldoa13"
  #define _STENG1AX_NUM_RAILS 1
  #define _STENG1AX_I3C_ADDR_0 40
  #define _STENG1AX_I3C_ADDR_1 41
  #define _STENG1AX_I3C_ADDR_2 42
  #define _STENG1AX_I3C_ADDR_3 43
  #define _STENG1AX_IRQ_MODE 2

#define _CT7117X_NUM_RAILS                 1
#define _CT7117X_RAIL_1                    SNS_POWER_RAIL_1_NAME
#define _CT7117X_RAIL_2                    ""
#define _CT7117X_BUS_TYPE                  SNS_BUS_SPI
#define _CT7117X_BUS_INSTANCE              5
#define _CT7117X_BUS_FREQ_MIN              10000
#define _CT7117X_BUS_FREQ_MAX              12000
#define _CT7117X_I2C_ADDR_0                0x4C // ambient temperature
#define _CT7117X_I2C_ADDR_1                0x48 // skin temperature
#define _CT7117X_I3C_ADDR                  00
#define _CT7117X_IRQ_NUM                   102

