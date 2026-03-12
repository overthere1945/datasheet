/**============================================================================
  @file sns_timer.c

  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_register.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_timer_sensor.h"

/*=============================================================================
  External Variable Declarations
  ===========================================================================*/

extern const sns_sensor_instance_api sns_timer_sensor_instance_api;
extern const sns_sensor_api timer_sensor_api;

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_timer_register(sns_register_cb const *register_api)
{
  return register_api->init_sensor(sizeof(sns_timer_state), &timer_sensor_api,
                                   &sns_timer_sensor_instance_api);
}
