/** ============================================================================
 * @file sns_time_sensor_instance.c
 *
 * @brief sns_time sensor instance implementation
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * ===========================================================================*/
/*=============================================================================
  Includes
  ===========================================================================*/
#include "sns_rc.h"
#include "sns_sensor.h"
#include "sns_sensor_instance.h"
#include "sns_types.h"

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

/* See sns_sensor_instance_api::notify_event */
static sns_rc sns_time_inst_notify_event(sns_sensor_instance *const this)
{
  UNUSED_VAR(this);
  return SNS_RC_SUCCESS;
}

/* See sns_sensor_instance_api::init */
static sns_rc sns_time_inst_init(sns_sensor_instance *const this,
                                 sns_sensor_state const *sensor_state)
{
  UNUSED_VAR(this);
  UNUSED_VAR(sensor_state);
  return SNS_RC_SUCCESS;
}

/* See sns_sensor_instance_api::deinit */
static sns_rc sns_time_inst_deinit(sns_sensor_instance *const this)
{
  UNUSED_VAR(this);
  return SNS_RC_SUCCESS;
}

/* See sns_sensor_instance_api::set_client_config */
static sns_rc sns_time_inst_set_client_config(sns_sensor_instance *const this,
                                              sns_request const *client_request)
{
  UNUSED_VAR(this);
  UNUSED_VAR(client_request);
  return SNS_RC_SUCCESS;
}

/*===========================================================================
  Public Data Definitions
  ===========================================================================*/

const sns_sensor_instance_api sns_time_sensor_instance_api = {
    .struct_len = sizeof(sns_sensor_instance_api),
    .init = &sns_time_inst_init,
    .deinit = &sns_time_inst_deinit,
    .set_client_config = &sns_time_inst_set_client_config,
    .notify_event = &sns_time_inst_notify_event};
