#pragma once
/*=============================================================================
  @file sns_registry_sensor.h

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_island_util.h"
#include "sns_list.h"
#include "sns_osa_thread.h"

/*=============================================================================
  Type Definitions
  ===========================================================================*/

/**
 * Parameters needed to write a JSON string to file.
 */
typedef struct sns_registry_group_write_cfg
{
  /* Name of the group for this JSON; string used in file name */
  char *name;
  /* The JSON string to write to file */
  void *json;
  /* The length of the JSON string */
  uint32_t length;
} sns_registry_group_write_cfg;

typedef struct sns_registry_sensor_state
{
  /* Registry thread */
  sns_osa_thread *thread;
  /* Island client handle */
  sns_island_client_handle island_client;
  /* List of sns_registry_group_write_cfg */
  sns_list write_list;
} sns_registry_sensor_state;

/*=============================================================================
  Public Function Declarations
  ===========================================================================*/

/**
 * Add a registry group JSON strings to the list of JSONs to be written.
 *
 * @note This function simply creates a local list of writes;
 * file access is performed only in sns_registry_group_write.
 *
 * @param[i] name Name of the source JSON
 * @param[i] name Name of the group to be used for file name
 * @param[i] json The JSON string containing the file text
 * @param[i] length The length of the JSON string
 * @param[i] sensor Registry Sensor handle to be used to enqueue writes
 *
 * @return
 *  SNS_RC_SUCCESS
 *  SNS_RC_NOT_AVAILABLE - Unable to add group to list of groups
 */
sns_rc sns_registry_group_enqueue(char const *restrict source,
                                  char const *restrict name,
                                  char *restrict json, uint32_t length,
                                  void *sensor);
