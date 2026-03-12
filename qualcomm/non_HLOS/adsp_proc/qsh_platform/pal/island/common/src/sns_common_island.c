/*=============================================================================
  @file sns_common_island.c

  Header file for the island hexagon utility

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Includes
  ===========================================================================*/

#include "sns_common_island.h"
#include "sns_mem_util.h"
#include "sns_island.h"
#include "sns_memmgr.h"
#include "sns_sensor.h"
#include "sns_types.h"
#include "sns_rc.h"

/*=============================================================================
  Functions
  ===========================================================================*/

ISLAND_CODE
sns_island_active_sensor_item *
sns_island_get_active_sensor(sns_sensor *const sensor,
                             sns_isafe_list *active_list)
{
  sns_isafe_list_iter iter;
  sns_island_active_sensor_item *sensor_item = NULL;

  // find sensor from the active sensor list
  for(sns_isafe_list_iter_init(&iter, active_list, true);
      NULL != sns_isafe_list_iter_curr(&iter);
      sns_isafe_list_iter_advance(&iter))
  {
    sns_island_active_sensor_item *item = NULL;

    item = (sns_island_active_sensor_item *)sns_isafe_list_iter_get_curr_data(
        &iter);

    if(item->sensor == sensor)
    {
      sensor_item = item;
      break;
    }
  }

  return sensor_item;
}
