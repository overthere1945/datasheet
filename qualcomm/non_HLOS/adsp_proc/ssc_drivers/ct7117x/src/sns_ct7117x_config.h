#pragma once
/*
*	sns_ct7117x_config.h
*/

#include "sns_interface_defs.h"

// <Registry Sensors.SEE-LITE sensor>
#ifndef CT7117X_CONFIG_ENABLE_SEE_LITE
#define CT7117X_CONFIG_ENABLE_SEE_LITE   0
#endif

#if CT7117X_CONFIG_ENABLE_SEE_LITE
#define CT7117X_ATTRIBUTE_DISABLED 1
#define CT7117X_DISABLE_REGISTRY   1
#define CT7117X_POWERRAIL_DISABLED 1
#endif

#ifndef CT7117X_ATTRIBUTE_DISABLED
#define CT7117X_ATTRIBUTE_DISABLED 0
#endif

#ifndef CT7117X_DISABLE_REGISTRY
#define CT7117X_DISABLE_REGISTRY   0
#endif

#ifndef CT7117X_POWERRAIL_DISABLED
#define CT7117X_POWERRAIL_DISABLED 0
#endif

#define RAIL_1              _CT7117X_RAIL_1
#define RAIL_2              _CT7117X_RAIL_2
#define NUM_OF_RAILS        _CT7117X_NUM_RAILS

#define SLAVE_ADDRESS       _CT7117X_I2C_ADDR_0 // ambient temperature
#define SLAVE_ADDRESS_1     _CT7117X_I2C_ADDR_1 // skin temperature
#define BUS_TYPE            _CT7117X_BUS_TYPE
#define BUS_INSTANCE        _CT7117X_BUS_INSTANCE
#define BUS_MIN_FREQ_KHZ    _CT7117X_BUS_FREQ_MIN
#define BUS_MAX_FREQ_KHZ    _CT7117X_BUS_FREQ_MAX

#define CT7117X_DEFAULT_REG_CFG_RAIL_ON             SNS_RAIL_ON_NPM
#define CT7117X_DEFAULT_REG_CFG_ISDRI               0
#define CT7117X_DEFAULT_REG_CFG_SUPPORT_SYN_STREAM  0
#define CT7117X_DEFAULT_RES_IDX                     2

#define ct7117x_com_port_cfg_init_def {      \
    .bus_type          = BUS_TYPE,           \
    .slave_control     = SLAVE_ADDRESS,      \
    .reg_addr_type     = SNS_REG_ADDR_8_BIT, \
    .min_bus_speed_KHz = BUS_MIN_FREQ_KHZ,   \
    .max_bus_speed_KHz = BUS_MAX_FREQ_KHZ,   \
    .bus_instance      = BUS_INSTANCE        \
}


