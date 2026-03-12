/**
 * @file sns_interrupt.c
 *
@copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * $Id:
 * //components/dev/qsh.platform/0.0/johnniea.qsh.platform.0.0.format_qshplatform/sensors/interrupt/hexagon/sns_interrupt.c#1
 *$ $DateTime: 2025/08/18 05:06:31 $ $Change: 65977982 $
 *
 **/
#include "sns_interrupt_sensor.h"
#include "sns_interrupt_sensor_instance.h"
#include "sns_rc.h"
#include "sns_register.h"

/** Public Function Definitions. */

sns_rc sns_register_interrupt(sns_register_cb const *register_api)
{
  register_api->init_sensor(sizeof(interrupt_state), &interrupt_sensor_api,
                            &interrupt_sensor_instance_api);

  return SNS_RC_SUCCESS;
}
