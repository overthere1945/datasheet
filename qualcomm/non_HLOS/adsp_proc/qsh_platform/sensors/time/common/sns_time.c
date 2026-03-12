/** ============================================================================
 * @file sns_time.c
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_register.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_time_sensor.h"

/*=============================================================================
  External Variable Declarations
  ===========================================================================*/

extern const sns_sensor_instance_api sns_time_sensor_instance_api;
extern const sns_sensor_api sns_time_api;

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_time_register(sns_register_cb const *register_api)
{
  register_api->init_sensor(sizeof(sns_time_sensor_state), &sns_time_api,
                            &sns_time_sensor_instance_api);

  return SNS_RC_SUCCESS;
}
