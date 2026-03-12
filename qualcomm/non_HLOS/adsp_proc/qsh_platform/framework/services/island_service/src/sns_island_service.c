/**
 *  sns_island_service.c
 *
 * @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/** Include Files */

#include <stdbool.h>
#include <stdint.h>
#include "sns_island.h"
#include "sns_island_service.h"

#define SNS_ISLAND_CODE SNS_SECTION(".text.sns")
#define SNS_ISLAND_DATA SNS_SECTION(".data.sns")

static sns_island_service island_service SNS_ISLAND_DATA;

/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc sensor_island_exit(sns_island_service *this,
                                          struct sns_sensor const *sensor)
{
  UNUSED_VAR(this);

  if(SNS_ISLAND_EXIT() == SNS_RC_SUCCESS)
  {
    SNS_PRINTF(MED, sns_fw_printf, "sensor_island_exit sensor = 0x%X", sensor);
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc sensor_instance_island_exit(
    sns_island_service *this, struct sns_sensor_instance const *instance)
{
  UNUSED_VAR(this);

  if(SNS_ISLAND_EXIT() == SNS_RC_SUCCESS)
  {
    SNS_PRINTF(MED, sns_fw_printf,
               "sensor_instance_island_exit instance = 0x%X", instance);
  }
  return SNS_RC_SUCCESS;
}
/*----------------------------------------------------------------------------*/

SNS_ISLAND_CODE sns_rc island_log_trace(sns_island_service *this,
                                        uint64_t user_defined_id)
{
  UNUSED_VAR(this);
  // TODO add registry support to disable this API for test purpose
  return sns_island_generate_and_commit_state_log(user_defined_id);
}
/*----------------------------------------------------------------------------*/

sns_island_service_api island_service_api SNS_ISLAND_DATA = {
    .struct_len = sizeof(sns_island_service_api),
    .sensor_island_exit = &sensor_island_exit,
    .sensor_instance_island_exit = &sensor_instance_island_exit,
    .island_log_trace = &island_log_trace};
/*----------------------------------------------------------------------------*/

sns_service *sns_island_service_init(void)
{
  /** Island util must be initialized before sns_island service init */
  island_service.api = &island_service_api;
  island_service.service.type = SNS_ISLAND_SERVICE;
  island_service.service.version = 1;
  return (sns_service *)&island_service;
}