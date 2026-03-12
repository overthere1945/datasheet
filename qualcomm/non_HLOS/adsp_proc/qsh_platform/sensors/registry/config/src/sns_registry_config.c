/*=============================================================================
  @file sns_registry_config.c

  Configuration options for the Sensors Registry parser.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/

#include "sns_registry_parser.h"
#include "sns_types.h"

/*=============================================================================
  Config Variable Definitions
  ===========================================================================*/

char const current_version_file[] = "sns_reg_version";
char const current_sensors_list[] = "sensors_list.txt";
char const parsed_file_list_file[] = "parsed_file_list.csv";
char const corrupted_file_list_file[] = "corrupted_file_list.csv";
char const config_group_name[] = "sns_reg_config";

// PEND: config_dir should be set to "" once all LA branches have been updated
#if defined(SSC_TARGET_HEXAGON)
#ifndef SNS_TARGET_OVERWRITE_REGISTRY_PATH
char const config_dir[] = "/vendor/etc/sensors/config";
char const registry_config_file[] = "/vendor/etc/sensors/sns_reg_config";
#else
char const config_dir[] = SNS_TARGET_CONFIG_DIR;
char const registry_config_file[] = SNS_TARGET_REGISTRY_CONFIG;
#endif
#elif defined(SSC_TARGET_X86)
char const config_dir[] = "registry/config/common";
char const registry_config_file[] = "registry/config/sns_reg_config";
#elif defined(SSC_TARGET_ASPEN_SWM)
#ifndef SNS_TARGET_OVERWRITE_REGISTRY_PATH
char const config_dir[] = "/vendor/etc/sensors/config";
char const registry_config_file[] = "/vendor/etc/sensors/sns_reg_config";
#else
char const config_dir[] = SNS_TARGET_CONFIG_DIR;
char const registry_config_file[] = SNS_TARGET_REGISTRY_CONFIG;
#endif
#endif
