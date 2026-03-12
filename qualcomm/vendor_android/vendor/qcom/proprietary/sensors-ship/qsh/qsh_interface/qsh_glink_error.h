/*
 * Copyright (c) 2021, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once
#define MAX_GLINK_ERROR_STR_LEN 30


struct glink_error : public runtime_error
{
    glink_error(int error_code, const std::string& what = "") :
        runtime_error(error_code_to_string(error_code, what)) { }

    static string error_code_to_string(int code, std::string what)
    {
      char errorno_msg[MAX_GLINK_ERROR_STR_LEN];
      char *p;
      p = strerror_r(code, errorno_msg, MAX_GLINK_ERROR_STR_LEN);
      string msg(p);
      return "qsh_glink_ERROR " + what + " : " + msg + " (" + to_string(code) + ")";
    }
};

