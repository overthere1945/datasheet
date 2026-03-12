/** =============================================================================
 *  @file salt_usta.cpp
 *
 *  @brief salt interface with USTA: libUSTANative.so
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  All rights reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 *  =============================================================================
 */

#include <string>
#include <sstream>
#include <iostream>

#include "see_salt.h"

using namespace std;
#include "sensor_cxt.h"
#include "usta_rc.h"
#include "salt_usta.h"

// forward declaration
static std::string usta_rc_to_string(usta_rc rc);
static std::string get_rate_type_symbolic(see_resampler_rate rate_type);
static std::string get_threshold_type_symbolic(see_threshold_type threshold_type);
static std::string get_delta_angle_symbolic(see_delta_angle_ori_type_e ori_type);

static bool usta_logging = false;

static string unknown_axis("MULTISHAKE_AXIS_UNKNOWN_AXIS");
static string type_stationary("SNS_PSMD_TYPE_STATIONARY");

// enable or disable usta logging
void set_usta_logging(bool enable) {
  usta_logging = enable;  // 1 == enabled, 0 == disabled
}

// vector holds SensorCxt instance per see_salt_instance_num_num;
static std::vector<SensorCxt*> sns_cxts;

static SensorCxt* usta_get_instance(see_salt* psalt) {
  // note: first salt_instance_num == 1
  int salt_instance_num = psalt->get_instance_num();

  if (salt_instance_num > (int)sns_cxts.size()) {
    SensorCxt* sns_cxt_instance = SensorCxt::getInstance();
    bool disabled = !usta_logging;
    sns_cxt_instance->update_logging_flag(disabled);
    sns_cxts.push_back(sns_cxt_instance);
    // assert sns_cxts.size() == salt_instance_num
  }
  return sns_cxts[salt_instance_num - 1];
}

void usta_destroy_instance(see_salt* psalt) {
  if (psalt) {
    SensorCxt* sns_cxt_instance = usta_get_instance(psalt);
    int salt_instance_num = psalt->get_instance_num();
    if (salt_instance_num > 0) {
      sns_cxts[salt_instance_num - 1] = nullptr;
    }
    if (sns_cxt_instance) {
      delete sns_cxt_instance;
    }
  }
}

salt_rc usta_get_sensor_list(see_salt* psalt,
                             vector<sensor_info>& sensor_list) {
  usta_rc rc = (usta_rc)SALT_RC_NULL_ARG;
  if (psalt) {
    SensorCxt* sns_cxt_instance = usta_get_instance(psalt);
    if (sns_cxt_instance) {
      rc = sns_cxt_instance->get_sensor_list(sensor_list);
    }
  }
  return (salt_rc)rc;
}

salt_rc usta_get_request_list(see_salt* psalt,
                              unsigned int handle) {
  vector<msg_info> req_msgs;
  usta_rc rc = (usta_rc)SALT_RC_NULL_ARG;

  if (psalt) {
    SensorCxt* sns_cxt_instance = usta_get_instance(psalt);
    if (sns_cxt_instance) {
      rc = sns_cxt_instance->get_request_list(handle, req_msgs);
    }

    if (psalt->get_debugging()) {
      cout << "msg_info" << endl;
      for (size_t i = 0; i < req_msgs.size(); i++) {
        cout << "  ." << i << ". " << "msg_name: " << req_msgs[i].msg_name;
        cout << ", msgid: " << req_msgs[i].msgid << endl;
        for (size_t j = 0; j < req_msgs[i].fields.size(); j++) {
          field_info field = req_msgs[i].fields[j];
          cout << "      field.name: " << field.name << endl;
          if (field.is_user_defined_msg) {
            for (size_t k = 0; k < field.nested_msgs.size(); k++) {
              nested_msg_info nested = field.nested_msgs[k];
              cout << "            nested.msg_name: " << nested.msg_name;
              cout << endl;
              for (size_t m = 0; m < nested.fields.size(); m++) {
                field_info nfield = nested.fields[m];
                cout << "                  .nfield.name: ";
                cout << nfield.name << endl;
              }
            }
          }
        }
      }
      cout << endl;
    }
  }
  return (salt_rc)rc;
}

salt_rc usta_get_sttributes(see_salt* psalt,
                            unsigned int handle,
                            string& attribute_list) {
  usta_rc rc = (usta_rc)SALT_RC_NULL_ARG;
  if (psalt) {
    SensorCxt* sns_cxt_instance = usta_get_instance(psalt);
    if (sns_cxt_instance) {
      rc = sns_cxt_instance->get_attributes(handle, attribute_list);
    }
  }
  return (salt_rc)rc;
}

/**
 * @brief call libUSTANative.so SensorCxt::send_request(...)
 * @param handle - index of sensor
 * @param client_req
 * @param client_request_info
 * @return client id
 */
int usta_send_request(see_salt* psalt,
                      unsigned int handle,
                      see_client_request_message& client_req,
                      see_client_request_info client_request_info) {
  if (!psalt) {
    return SALT_RC_NULL_ARG;
  }

  see_std_request* std_req = client_req.get_request();

  /* init client_req_msg_fields*/
  // perhaps change name client_msg to client_msg_fields
  client_req_msg_fileds client_msg;
  see_std_client_processor_e client;
  see_client_delivery_e wakeup;
  client_req.get_suspend_config(client, wakeup);
  client_msg.client_type = client_req.get_client_symbolic(client);
  client_msg.wakeup_type = client_req.get_wakeup_symbolic(wakeup);

  if (std_req->has_batch_period()) {
    int batch_period = std_req->get_batch_period();
    client_msg.batch_period = to_string(batch_period);
  }
  if (std_req->has_flush_period()) {
    int flush_period = std_req->get_flush_period();
    client_msg.flush_period = to_string(flush_period);
  }
  if (std_req->has_flush_only()) {
    int flush_only = std_req->get_flush_only();
    client_msg.flush_only = to_string(flush_only);
  }
  if (std_req->has_max_batch()) {
    int max_batch = std_req->get_max_batch();
    client_msg.max_batch = to_string(max_batch);
  }
  if (std_req->has_is_passive()) {
    int is_passive = std_req->get_is_passive();
    client_msg.is_passive = to_string(is_passive);
  }
  if (std_req->has_resampler_type()) {
    see_resampler_rate rtype = std_req->get_resampler_type();
    client_msg.resampler_type = get_rate_type_symbolic(rtype);
  }
  if (std_req->has_resampler_filter()) {
    bool rfilter = std_req->get_resampler_filter();
    if (rfilter) {
      client_msg.resampler_filter = "true";
    } else {
      client_msg.resampler_filter = "false";
    }
  }
  if (std_req->has_threshold_type()) {
    see_threshold_type ttype = std_req->get_threshold_type();
    client_msg.threshold_type = get_threshold_type_symbolic(ttype);
  }
  if (std_req->has_threshold_value()) {
    client_msg.threshold_value = std_req->get_threshold_value();
  }

  if (std_req->has_client_permissions()) {
    const std::vector<see_tech_e> client_permissions = std_req->get_client_permissions();
    const size_t len = client_permissions.size();
    for (size_t idx = 0; idx < len; ++idx) {
      client_msg.client_permissions.push_back(get_client_permission_symbolic(client_permissions[idx]));
    }
  }

  send_req_msg_info msg_info;
  see_msg_id_e msg_id = client_req.get_msg_id();
  msg_info.msgid = to_string(msg_id);

  if (std_req->is_payload_std_sensor_config()) {
    see_std_sensor_config* std_sensor_config =
        std_req->get_payload_std_sensor_config();

    send_req_field_info field_info;
    float rate = std_sensor_config->get_sample_rate();
    field_info.name = "sample_rate";
    field_info.value.push_back(to_string(rate));

    msg_info.msg_name = "sns_std_sensor_config";
    msg_info.fields.push_back(field_info);
  } else if (std_req->is_payload_self_test_config()) {
    see_self_test_config* self_test_config =
        std_req->get_payload_self_test_config();

    send_req_field_info field_info;
    see_self_test_type_e test_type = self_test_config->get_test_type();
    string s = self_test_config->get_test_type_symbolic(test_type);
    field_info.name = "test_type";
    field_info.value.push_back(s);

    msg_info.msg_name = "sns_physical_sensor_test_config";
    msg_info.fields.push_back(field_info);
  } else if (std_req->is_payload_diag_log_trigger_req()) {
    see_diag_log_trigger_req* diag_log_trigger_req =
        std_req->get_payload_diag_log_trigger_req();

    msg_info.msg_name = "sns_diag_log_trigger_req";

    if (diag_log_trigger_req->has_cookie()) {
      send_req_field_info cookie_field_info;
      cookie_field_info.name = "cookie";
      uint64_t cookie = diag_log_trigger_req->get_cookie();
      cookie_field_info.value.push_back(to_string(cookie));
      msg_info.fields.push_back(cookie_field_info);
    }

    send_req_field_info log_type_field_info;
    log_type_field_info.name = "log_type";
    string s = diag_log_trigger_req->get_log_type_symbolic();
    log_type_field_info.value.push_back(s);
    msg_info.fields.push_back(log_type_field_info);
  } else if (std_req->is_payload_set_distance_bound()) {
    see_set_distance_bound* set_distance_bound = std_req->get_payload_set_distance_bound();

    // distance_bound field
    send_req_field_info distance_bound_breach_field_info;
    float distance_bound = set_distance_bound->get_distance_bound();
    distance_bound_breach_field_info.name = "distance_bound";
    distance_bound_breach_field_info.value.push_back(to_string(distance_bound));
    msg_info.fields.push_back(distance_bound_breach_field_info);

    std::vector<std::pair<std::string, std::string>> _states_speed = set_distance_bound->get_states_speeds_values();
    for (int i = 0; i < _states_speed.size(); i++) {
      // speed field in "speeds" message
      send_req_field_info speed_field_info;
      speed_field_info.name = "speed";
      std::string speed_val = _states_speed[i].second; // speed value in m/s
      speed_field_info.value.push_back(speed_val);

      // state field in "speeds" message
      send_req_field_info state_field_info;
      state_field_info.name = "state";
      std::string motion_state_str = _states_speed[i].first; // motion_state
      state_field_info.value.push_back(motion_state_str);

      // speeds nested message
      send_req_nested_msg_info speeds_msg_info;
      speeds_msg_info.msg_name = "speeds";
      speeds_msg_info.fields.push_back(state_field_info);
      speeds_msg_info.fields.push_back(speed_field_info);

      // speed_bound field in sns_set_distance_bound message
      send_req_field_info speed_bound_field_info;
      speed_bound_field_info.name = "speed_bound";
      speed_bound_field_info.nested_msgs.push_back(speeds_msg_info);
      msg_info.fields.push_back(speed_bound_field_info);
    }

    msg_info.msg_name = "sns_set_distance_bound";
  } else if (std_req->is_payload_basic_gestures_config()) {
    send_req_field_info field_info;
    field_info.name = "";
    msg_info.msg_name = "sns_basic_gestures_config";
    msg_info.fields.push_back(field_info);
  } else if (std_req->is_payload_multishake_config()) {
    send_req_field_info field_info;
    field_info.name = "shake_axis";
    field_info.value.push_back(unknown_axis);
    msg_info.msg_name = "sns_multishake_config";
    msg_info.fields.push_back(field_info);
  } else if (std_req->is_payload_psmd_config()) {
    send_req_field_info field_info;
    field_info.name = "type";
    field_info.value.push_back(type_stationary);
    msg_info.msg_name = "sns_psmd_config";
    msg_info.fields.push_back(field_info);
  } else if (std_req->is_payload_sim_start_req()) {
    see_sim_start_req* sim_start_req =
        std_req->get_payload_sim_start_req();

    send_req_field_info dlf_field_info;
    string s = sim_start_req->get_dlf_file();
    dlf_field_info.name = "dlf_file";
    dlf_field_info.value.push_back(s);
    msg_info.fields.push_back(dlf_field_info);

    send_req_field_info reps_field_info;
    uint32_t reps = sim_start_req->get_repetitions();
    reps_field_info.name = "repetitions";
    reps_field_info.value.push_back(to_string(reps));
    msg_info.fields.push_back(reps_field_info);

    msg_info.msg_name = "sns_sim_start_req";
  } else if (std_req->is_payload_delta_angle_req()) {
    see_delta_angle_req* delta_angle_req =
        std_req->get_payload_delta_angle_req();

    send_req_field_info ori_field_info;
    see_delta_angle_ori_type_e ori_type = delta_angle_req->get_ori_type();
    string s = get_delta_angle_symbolic(ori_type);
    ori_field_info.name = "ori_type";
    ori_field_info.value.push_back(s);
    msg_info.fields.push_back(ori_field_info);

    send_req_field_info radians_field_info;
    float radians = delta_angle_req->get_radians();
    radians_field_info.name = "radians";
    radians_field_info.value.push_back(to_string(radians));
    msg_info.fields.push_back(radians_field_info);

    send_req_field_info latency_field_info;
    uint32_t latency = delta_angle_req->get_latency_us();
    latency_field_info.name = "latency_us";
    latency_field_info.value.push_back(to_string(latency));
    msg_info.fields.push_back(latency_field_info);

    if (delta_angle_req->has_quat_output()) {
      send_req_field_info quat_output_field_info;
      int value = delta_angle_req->get_quat_output_enable();
      quat_output_field_info.name = "quat_output_enable";
      quat_output_field_info.value.push_back(to_string(value));
      msg_info.fields.push_back(quat_output_field_info);
    }

    if (delta_angle_req->has_quat_history()) {
      send_req_field_info quat_history_field_info;
      int value = delta_angle_req->get_quat_history_enable();
      quat_history_field_info.name = "quat_history_enable";
      quat_history_field_info.value.push_back(to_string(value));
      msg_info.fields.push_back(quat_history_field_info);
    }

    if (delta_angle_req->has_hysteresis_usec()) {
      send_req_field_info hysteresis_field_info;
      int value = delta_angle_req->get_hysteresis_usec();
      hysteresis_field_info.name = "hysteresis_usec";
      hysteresis_field_info.value.push_back(to_string(value));
      msg_info.fields.push_back(hysteresis_field_info);
    }

    msg_info.msg_name = "sns_delta_angle_config";
  } else if (std_req->is_payload_odp_on_change_config()) {
    see_odp_on_change_config* odp_on_change_config =
        std_req->get_payload_odp_on_change_config();

    if (odp_on_change_config->has_user_attribute_filter()) {
      send_req_field_info user_attribute_filter_field_info;
      int value = odp_on_change_config->get_user_attribute_filter();
      user_attribute_filter_field_info.name = "user_attribute_filter";
      user_attribute_filter_field_info.value.push_back(to_string(value));
      msg_info.fields.push_back(user_attribute_filter_field_info);
    }

    msg_info.msg_name = "sns_odp_on_change_config";
  } else if (std_req->has_payload()) {
    int payload_type = std_req->get_payload_type();
    cout << "usta_send_request() unknown payload type: "
        << to_string(payload_type) << endl;
  }

  string logfile_suffix = "see_salt.json";
  string usta_mode("C" + to_string(psalt->get_instance_num()));

  SensorCxt* sns_cxt_instance = usta_get_instance(psalt);
  client_request request_info;

  if (client_request_info.req_type == SEE_CREATE_CLIENT) {
    request_info.req_type = CREATE_CLIENT;
    register_cb cb;
    cb.event_cb =
        (sensor_event_cb)(client_request_info.cb_ptr.sensor_event_cb_func_inst);
    request_info.cb_ptr = cb;
  } else {
    request_info.req_type = USE_CLIENT_CONNECT_ID;
    request_info.client_connect_id = client_request_info.client_connect_id;
  }

  int clientId = sns_cxt_instance->send_request(request_info,
                                                handle,
                                                msg_info,
                                                client_msg,
                                                logfile_suffix);

  if (clientId < 0) {
    cout << "usta_send_request failed with return value: " << clientId << endl;
  }

  return clientId;
}

static std::string usta_rc_to_string(usta_rc rc) {
  if (rc == USTA_RC_SUCCESS) {
    return string("USTA_RC_SUCCESS");
  } else if (rc == USTA_RC_FAILED) {
    /* operation failed ; cannot continue */
    return string("USTA_RC_FAILED");
  } else if (rc == USTA_RC_DESC_FAILED) {
    /* descriptor unavailable */
    return string("USTA_RC_DESC_FAILED");
  } else if (rc == USTA_RC_NO_MEMORY) {
    /* Memory Allocation Failed */
    return string("USTA_RC_NO_MEMORY");
  } else if (rc == USTA_RC_NO_PROTO) {
    /* proto files not found in desccriptor pool */
    return string("USTA_RC_NO_PROTO");
  } else if (rc == USTA_RC_NO_REQUEST_MSG) {
    /* No request messages found */
    return string("USTA_RC_NO_REQUEST_MSG");
  } else if (rc == USTA_RC_ATTRIB_FAILED) {
    /* requested attribute not found */
    return string("USTA_RC_ATTRIB_FAILED");
  } else if (rc == USTA_RC_REQUIIRED_FILEDS_NOT_PRESENT) {
    /*Required fileds are missing in the proto files*/
    return string("USTA_RC_ATTRIB_FAILED");
  } else if (rc == (usta_rc)SALT_RC_NULL_ARG) {
    /*Required argument is null*/
    return string("SALT_RC_NULL_ARG");
  } else {
    return string(" unknown rc " + to_string(rc));
  }
}

/**
 * @brief get resampler rate_type as a string
 * @param rate_type enum
 *
 * @return rate_type as string
 */
static std::string get_rate_type_symbolic(see_resampler_rate rate_type) {
  if (rate_type == SEE_RESAMPLER_RATE_FIXED) {
    return "SNS_RESAMPLER_RATE_FIXED";
  } else if (rate_type == SEE_RESAMPLER_RATE_MINIMUM) {
    return "SNS_RESAMPLER_RATE_MINIMUM";
  }
  return "";
}

/**
 * @brief get threshold type as a string
 * @param threshold_type enum
 *
 * @return threshold_type as string
 */
static std::string get_threshold_type_symbolic(see_threshold_type threshold_type) {
  if (threshold_type == SEE_THRESHOLD_TYPE_RELATIVE_VALUE) {
    return "SNS_THRESHOLD_TYPE_RELATIVE_VALUE";
  } else if (threshold_type == SEE_THRESHOLD_TYPE_RELATIVE_PERCENT) {
    return "SNS_THRESHOLD_TYPE_RELATIVE_PERCENT";
  } else if (threshold_type == SEE_THRESHOLD_TYPE_ABSOLUTE) {
    return "SNS_THRESHOLD_TYPE_ABSOLUTE";
  } else if (threshold_type == SEE_THRESHOLD_TYPE_ANGLE) {
    return "SNS_THRESHOLD_TYPE_ANGLE";
  }
  return "";
}

/**
 * @brief delta_angle_ori_type as string
 * @param see_delta_angle_ori_type
 *
 * @return delta_angle_ori_type as string
 */
static std::string get_delta_angle_symbolic(see_delta_angle_ori_type_e ori_type) {
  if (ori_type == SEE_DELTA_ANGLE_ORIENTATION_PITCH) return "SNS_DELTA_ANGLE_ORIENTATION_PITCH";
  else if (ori_type == SEE_DELTA_ANGLE_ORIENTATION_ROLL) {
    return "SNS_DELTA_ANGLE_ORIENTATION_ROLL";
  } else if (ori_type == SEE_DELTA_ANGLE_ORIENTATION_HEADING) {
    return "SNS_DELTA_ANGLE_ORIENTATION_HEADING";
  } else if (ori_type == SEE_DELTA_ANGLE_ORIENTATION_TOTAL) {
    return "SNS_DELTA_ANGLE_ORIENTATION_TOTAL";
  }
  return "";
}
