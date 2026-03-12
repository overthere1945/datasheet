/** =============================================================================
 *  @file see_salt_payloads.h
 *
 *  @brief class definitions see payloads
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  All rights reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 *  =============================================================================
 */

#pragma once
#include <cstdint>
// ----------------------------------
// sns_std_sensor.proto:
// message sns_std_sensor_config {
//    required float sample_rate = 1;
// }
// ----------------------------------
class see_std_sensor_config {
 public:
  see_std_sensor_config(float sample_rate) { _sample_rate = sample_rate;}
  float get_sample_rate() { return _sample_rate;}
 private:
  float _sample_rate;  // hz
};

// ------------------------------------------------------
// sns_physical_sensor_test.proto:
// message sns_physical_sensor_test_config {
//   required sns_physical_sensor_test_type test_type = 1;
// }
// ------------------------------------------------------
enum see_self_test_type_e {
  SELF_TEST_TYPE_SW = 0,
  SELF_TEST_TYPE_HW = 1,
  SELF_TEST_TYPE_FACTORY = 2,
  SELF_TEST_TYPE_COM = 3
};

class see_self_test_config {
 public:
  see_self_test_config(see_self_test_type_e test_type) {
    _test_type = test_type;
  }
  see_self_test_type_e get_test_type() { return _test_type;}
  std::string get_test_type_symbolic(see_self_test_type_e test_type);

 private:
  see_self_test_type_e _test_type;
};

// ----------------------------------------------------
// sns_diag_sensor.proto
// message sns_diag_sensor_log_trigger_req {
//   optional fixed64 cookie = 1;
//   required sns_diag_triggered_log_type log_type = 2;
// }
// ----------------------------------------------------
enum see_diag_trigger_log_type {
  SEE_LOG_TYPE_ISLAND_LOG = 1,
  SEE_LOG_TYPE_MEMORY_USAGE_LOG = 2
};

class see_diag_log_trigger_req {
 public:
  see_diag_log_trigger_req(see_diag_trigger_log_type log_type) {
    _log_type = log_type;
    _has_cookie = false;
    _cookie = 0;
  }
  see_diag_log_trigger_req(see_diag_trigger_log_type log_type,
                           uint64_t cookie) {
    _log_type = log_type;
    _has_cookie = true;
    _cookie = cookie;
  }
  see_diag_trigger_log_type get_log_type() { return _log_type;}
  bool has_cookie() { return _has_cookie;}
  uint64_t get_cookie() { return _cookie;}
  std::string get_log_type_symbolic();

 private:
  see_diag_trigger_log_type _log_type;
  bool _has_cookie;
  uint64_t _cookie;
};

// ----------------------------------------------------
// sns_distance_bound.proto
//message sns_set_distance_bound
//{
//  required float distance_bound = 1;
//  message speeds {
//    required sns_distance_bound_motion_state state = 1;
//    required  float speed = 2;
//  }
//  repeated speeds speed_bound = 2;
//}
// ----------------------------------------------------

class see_set_distance_bound {
 public:
  see_set_distance_bound(float distance_bound, std::vector<std::pair<std::string, std::string>> states_speeds) {
    _distance_bound = distance_bound;
    _states_speeds = states_speeds;
  }

  float get_distance_bound() { return _distance_bound; }
  std::vector<std::pair<std::string, std::string>> get_states_speeds_values() { return _states_speeds; }
 private:
  float _distance_bound;
  std::vector<std::pair<std::string, std::string>> _states_speeds;
}; 

// ----------------------------------------------------
// sns_basic_gestures.proto
// note: we're not setting any fields.
// message sns_basic_gestures_config  {
//   optional float sleep           = 1;
//   optional float push_threshold  = 2;
//   optional float pull_threshold  = 3;
//   optional float shake_threshold = 4;
//   optional bytes event_mask    = 5;
// }
// ----------------------------------------------------
class see_basic_gestures_config {
 public:
  see_basic_gestures_config() { }
};

// ----------------------------------------------------
// sns_multishake.proto
// note: we're setting repeated = 0
// message sns_multishake_config {
//  repeated multishake_axis shake_axis = 1
// }
// ----------------------------------------------------
class see_multishake_config {
 public:
  see_multishake_config() { }
};

// ----------------------------------------------------
// sns_psmd.proto
// message sns_psmd_config {
//   required sns_psmd_type type = 1;
// }
// ----------------------------------------------------
enum see_psmd_type {
  SEE_PSMD_TYPE_STATIONARY = 0,
  SEE_PSMD_TYPE_MOTION = 1
};

class see_psmd_config {
 public:
  see_psmd_config(see_psmd_type type) { _type = type;}
  see_psmd_type get_type() { return _type;}
 private:
  see_psmd_type _type;
};

// ----------------------------------------------------
// sns_sim.proto {
// message sns_sim_start_req {
//   required string dlf_file = 1;
//   optional uint32 repetitions = 2 [default = 1];
// }
// ----------------------------------------------------
class see_sim_start_req {
 public:
  see_sim_start_req(std::string _dlf_file, uint32_t _repetitions) {
    dlf_file = _dlf_file;
    repetitions = _repetitions;
  }
  std::string get_dlf_file() { return dlf_file; }
  uint32_t get_repetitions() { return repetitions; }

 private:
  std::string dlf_file;
  uint32_t repetitions;
};

// ------------------------------------------------------
// defined as per sns_resampler.proto:
// ------------------------------------------------------
enum see_resampler_rate {
  SEE_RESAMPLER_RATE_FIXED = 0,
  SEE_RESAMPLER_RATE_MINIMUM = 1
};

// ------------------------------------------------------
// defined in sns_threshold.proto:
// ------------------------------------------------------
enum see_threshold_type {
  SEE_THRESHOLD_TYPE_RELATIVE_VALUE = 0,
  SEE_THRESHOLD_TYPE_RELATIVE_PERCENT = 1,
  SEE_THRESHOLD_TYPE_ABSOLUTE = 2,
  SEE_THRESHOLD_TYPE_ANGLE = 3
};
// ------------------------------------------------------
// defined in sns_delta_angle.proto:
// ------------------------------------------------------
enum see_delta_angle_ori_type_e {
  SEE_DELTA_ANGLE_ORIENTATION_PITCH = 1,
  SEE_DELTA_ANGLE_ORIENTATION_ROLL = 2,
  SEE_DELTA_ANGLE_ORIENTATION_HEADING = 3,
  SEE_DELTA_ANGLE_ORIENTATION_TOTAL = 4
};
// ------------------------------------------------------
// message sns_delta_angle_config {
//  required sns_delta_angle_ori_type ori_type = 1;
//  required float radians = 2;
//  required uint32 latency_us = 3;
//  optional bool quat_output_enable = 4;
//  optional bool quat_history_enable = 5;
// }
// ------------------------------------------------------
class see_delta_angle_req {
 public:
  see_delta_angle_req(see_delta_angle_ori_type_e _ori_type,
                      float _radians, uint32_t _latency_us) {
    ori_type = _ori_type;
    radians = _radians;
    latency_us = _latency_us;
    has_quat_output_enable = false;
    has_quat_history_enable = false;
  }
  void set_quat_output_enable(bool value) {
    has_quat_output_enable = true;
    quat_output_enable = value;
  }
  void set_quat_history_enable(bool value) {
    has_quat_history_enable = true;
    quat_history_enable = value;
  }
  void set_hysteresis_usec(uint32_t value) {
    has_hysteresis_us = true;
    hysteresis_us = value;
  }
  see_delta_angle_ori_type_e get_ori_type() { return ori_type;}
  float get_radians() { return radians;}
  uint32_t get_latency_us() { return latency_us;}
  bool has_quat_output() { return has_quat_output_enable;}
  bool get_quat_output_enable() { return quat_output_enable;}
  bool has_quat_history() { return has_quat_history_enable;}
  bool get_quat_history_enable() { return quat_history_enable;}
  bool has_hysteresis_usec() { return has_hysteresis_us;}
  uint32_t get_hysteresis_usec() { return hysteresis_us;}

 private:
  see_delta_angle_ori_type_e ori_type;
  float radians;
  uint32_t latency_us;
  bool has_quat_output_enable;
  bool quat_output_enable;
  bool has_quat_history_enable;
  bool quat_history_enable;
  bool has_hysteresis_us;
  uint32_t hysteresis_us;
};

// ------------------------------------------------------
// message sns_odp_on_change_config {
//  optional fixed32 user_attribute_filter = 1;
// }
// ------------------------------------------------------
class see_odp_on_change_config {
 public:
  bool has_user_attribute_filter() { return _has_user_attribute_filter;}
  uint32_t get_user_attribute_filter() { return _user_attribute_filter;}
  void set_user_attribute_filter(uint32_t user_attribute_filter) {
    _user_attribute_filter = user_attribute_filter;
    _has_user_attribute_filter = true;
  }
 private:
  uint32_t _user_attribute_filter;
  bool _has_user_attribute_filter;
};