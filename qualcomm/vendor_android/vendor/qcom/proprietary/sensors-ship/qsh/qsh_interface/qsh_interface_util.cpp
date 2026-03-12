/*
 * Copyright (c) 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <cutils/properties.h>
#include <string>
#include "qsh_interface_util.h"
#include "sensors_log.h"

bool __attribute__((weak)) is_qsh_glink_supported(){
  static bool is_aon_support_set = false;
  static bool is_aon_support_flag = false;
  if (!is_aon_support_set) {
    char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
    sns_logd("weak is_qsh_glink_supported");
    int aon_len = property_get("ro.vendor.qc_aon_presence", prop_value, NULL);
    sns_logd("aon_len: %d, ro.vendor.qc_aon_presence: %d \n", aon_len, atoi(prop_value));
    if(atoi(prop_value) == 0)
    {
      is_aon_support_flag = false;
    } else {
      is_aon_support_flag = true;
    }
    is_aon_support_set = true;
    sns_logd("is_aon_support_flag = %d, is_aon_support_set = %d", is_aon_support_flag, is_aon_support_set);
  }
  return is_aon_support_flag;
}
