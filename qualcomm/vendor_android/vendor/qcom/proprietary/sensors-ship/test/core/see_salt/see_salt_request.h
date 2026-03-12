/** =============================================================================
 *  @file see_salt_request.h
 *
 *  @brief class definitions see_std_request
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  All rights reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 *  =============================================================================
 */

#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// sns_std.proto:
// message sns_std_request {
//   message batch_spec {
//     required uint32 batch_period = 1;
//     optional uint32 flush_period = 2;
//     optional bool flush_only = 3 [default = false];
//     optional bool max_batch = 4 [default = false];
//   }
//   optional batch_spec batching = 1;
//   optional bytes payload = 2;
//   optional bool is_passive = 3 [default = false];
//   message client_permissions
//   {
//     repeated sns_tech technologies = 1;
//   }
//   optional client_permissions permissions = 4;
// }
// ---------------------------------------------------------------------------
enum see_payload_types_e {
   SEE_PAYLOAD_OMITTED = 0,
   SEE_PAYLOAD_STD_SENSOR,
   SEE_PAYLOAD_SELF_TEST,
   SEE_PAYLOAD_RESAMPLER,
   SEE_PAYLOAD_DIAG_SENSOR_TRIGGER_REQ,
   SEE_PAYLOAD_SET_DISTANCE_BOUND,
   SEE_PAYLOAD_BASIC_GESTURES,
   SEE_PAYLOAD_MULTISHAKE,
   SEE_PAYLOAD_PSMD,
   SEE_PAYLOAD_SIM_START_REQ,
   SEE_PAYLOAD_DELTA_ANGLE_REQ,
   SEE_PAYLOAD_ODP_CONFIG
};

enum see_tech_e {
  SEE_TECH_UNINITIALIZED = 0,
  SEE_TECH_SENSORS = 1,
  SEE_TECH_AUDIO = 2,
  SEE_TECH_CAMERA = 3,
  SEE_TECH_LOCATION = 4,
  SEE_TECH_MODEM = 5,
  SEE_TECH_BLUETOOTH = 6,
  SEE_TECH_WIFI = 7,
  SEE_TECH_CONNECTIVITY = 8,
};

class see_std_request {
private:
   bool _has_batch_period;
   int  _batch_period;      // microseconds
   bool _has_flush_period;
   int  _flush_period;      // microseconds

   bool _has_flush_only;
   bool _flush_only;
   bool _has_max_batch;
   bool _max_batch;
   bool _has_is_passive;
   bool _is_passive;

   bool _has_resampler_type;
   see_resampler_rate _resampler_type;
   bool _has_resampler_filter;
   bool _resampler_filter;

   bool _has_threshold_type;
   see_threshold_type _threshold_type;
   bool _has_threshold_value;
   std::vector<std::string> _threshold_value;

   bool _has_client_permissions;
   std::vector <see_tech_e> _client_permissions;

   bool _has_payload;
   see_payload_types_e _payload_type;
   union {
      see_std_sensor_config *_std_sensor_config;
      see_self_test_config *_self_test_config;
      see_diag_log_trigger_req *_diag_log_trigger_req;
      see_set_distance_bound *_set_distance_bound;
      see_basic_gestures_config * _basic_gestures_config;
      see_multishake_config * _multishake_config;
      see_psmd_config * _psmd_config;
      see_sim_start_req * _sim_start_req;
      see_delta_angle_req * _delta_angle_req;
      see_odp_on_change_config * _odp_on_change_config;
   } _payload;

public:
   see_std_request() {
      _has_batch_period = false;
      _has_flush_period = false;
      _has_flush_only = false;
      _has_max_batch = false;
      _has_is_passive = false;
      _has_payload = false;
      _has_resampler_type = false;
      _has_resampler_filter = false;
      _has_threshold_type = false;
      _has_threshold_value = false;
      _has_client_permissions = false;
      _payload_type = SEE_PAYLOAD_OMITTED;
   }

   void set_batch_spec(int batch_period, int flush_period) {
      set_batch_period(batch_period);
      set_flush_period(flush_period);
   }
   void set_batch_period(int batch_period) {
      _has_batch_period = true;
      _batch_period = batch_period;
   }
   bool has_batch_period() { return _has_batch_period; }
   int  get_batch_period() { return _batch_period; }

   void set_flush_period(int flush_period) {
      _has_flush_period = true;
      _flush_period = flush_period;
   }
   bool has_flush_period() { return _has_flush_period; }
   int  get_flush_period() { return _flush_period; }

   void set_flush_only(bool flush_only) {
      _has_flush_only = true;
      _flush_only = flush_only;
   }
   bool has_flush_only() { return _has_flush_only; }
   bool get_flush_only() { return _flush_only; }

   void set_max_batch(bool max_batch) {
      _has_max_batch = true;
      _max_batch = max_batch;
   }
   bool has_max_batch() { return _has_max_batch; }
   bool get_max_batch() { return _max_batch; }

   void set_is_passive(bool is_passive) {
      _has_is_passive = true;
      _is_passive = is_passive;
   }
   bool has_is_passive() { return _has_is_passive; }
   bool get_is_passive() { return _is_passive; }

   void set_resampler_type(see_resampler_rate resampler_type) {
       _has_resampler_type = true;
       _resampler_type = resampler_type;
   }
   bool has_resampler_type() { return _has_resampler_type; }
   see_resampler_rate get_resampler_type() { return _resampler_type; }

   void set_resampler_filter(bool resampler_filter) {
       _has_resampler_filter = true;
       _resampler_filter = resampler_filter;
   }
   bool has_resampler_filter() { return _has_resampler_filter; }
   bool get_resampler_filter() { return _resampler_filter; }

   void set_threshold_type(see_threshold_type thresold_type) {
       _has_threshold_type = true;
       _threshold_type = thresold_type;
   }
   bool has_threshold_type() { return _has_threshold_type; }
   see_threshold_type get_threshold_type() { return _threshold_type; }

   void set_threshold_value(std::vector<std::string> threshold_value) {
       _has_threshold_value = true;
       _threshold_value = threshold_value;
   }
   bool has_threshold_value() { return _has_threshold_value; }
   std::vector<std::string> get_threshold_value() { return _threshold_value; }

   void set_client_permissions(const std::vector<see_tech_e>& client_permissions) {
       _has_client_permissions = true;
       _client_permissions = client_permissions;
   }
   bool has_client_permissions() { return _has_client_permissions; }
   std::vector<see_tech_e> get_client_permissions() { return _client_permissions; }

   bool has_payload() { return _has_payload;}

   int get_payload_type() { return (int) _payload_type;}

   /**
    * @brief see_std_sensor_config
    */
   void set_payload(see_std_sensor_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_STD_SENSOR;
      _payload._std_sensor_config = payload;
   }
   see_std_sensor_config *get_payload_std_sensor_config() {
      return _payload._std_sensor_config;
   }
   bool is_payload_std_sensor_config() {
      return _payload_type == SEE_PAYLOAD_STD_SENSOR;
   }
   /**
    * @brief see_self_test_config
    */
   void set_payload(see_self_test_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_SELF_TEST;
      _payload._self_test_config = payload;
   }
   see_self_test_config *get_payload_self_test_config() {
      return _payload._self_test_config;
   }
   bool is_payload_self_test_config() {
      return _payload_type == SEE_PAYLOAD_SELF_TEST;
   }
   /**
    * @brief see_diag_log_trigger_req
    */
   void set_payload(see_diag_log_trigger_req *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_DIAG_SENSOR_TRIGGER_REQ;
      _payload._diag_log_trigger_req = payload;
   }
   see_diag_log_trigger_req *get_payload_diag_log_trigger_req() {
      return _payload._diag_log_trigger_req;
   }
   bool is_payload_diag_log_trigger_req() {
      return _payload_type == SEE_PAYLOAD_DIAG_SENSOR_TRIGGER_REQ;
   }
   /**
    * @brief see_set_distance_bound
    */
   void set_payload(see_set_distance_bound *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_SET_DISTANCE_BOUND;
      _payload._set_distance_bound = payload;
   }
   see_set_distance_bound *get_payload_set_distance_bound() {
      return _payload._set_distance_bound;
   }
   bool is_payload_set_distance_bound() {
      return _payload_type == SEE_PAYLOAD_SET_DISTANCE_BOUND;
   }
   /**
    * @brief see_basic_gestures_config
    */
   void set_payload( see_basic_gestures_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_BASIC_GESTURES;
      _payload._basic_gestures_config = payload;
   }
   bool is_payload_basic_gestures_config() {
      return _payload_type == SEE_PAYLOAD_BASIC_GESTURES;
   }
   /**
    * @brief see_multishake_config
    */
   void set_payload( see_multishake_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_MULTISHAKE;
      _payload._multishake_config = payload;
   }
   bool is_payload_multishake_config() {
      return _payload_type == SEE_PAYLOAD_MULTISHAKE;
   }
   /**
    * @brief see_psmd_config
    */
   void set_payload(see_psmd_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_PSMD;
      _payload._psmd_config = payload;
   }
   bool is_payload_psmd_config() {
      return _payload_type == SEE_PAYLOAD_PSMD;
   }

   /**
    * @brief see_sim_start_req
    */
   void set_payload(see_sim_start_req *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_SIM_START_REQ;
      _payload._sim_start_req = payload;
   }
   see_sim_start_req *get_payload_sim_start_req() {
      return _payload._sim_start_req;
   }
   bool is_payload_sim_start_req() {
      return _payload_type == SEE_PAYLOAD_SIM_START_REQ;
   }

   /**
    * @brief see_delta_angle_req
    */
   void set_payload(see_delta_angle_req *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_DELTA_ANGLE_REQ;
      _payload._delta_angle_req = payload;
   }
   see_delta_angle_req *get_payload_delta_angle_req() {
      return _payload._delta_angle_req;
   }
   bool is_payload_delta_angle_req() {
      return _payload_type == SEE_PAYLOAD_DELTA_ANGLE_REQ;
   }

   /**
    * @brief see_odp_on_change_config
    */
   void set_payload(see_odp_on_change_config *payload) {
      _has_payload = true;
      _payload_type = SEE_PAYLOAD_ODP_CONFIG;
      _payload._odp_on_change_config = payload;
   }
   see_odp_on_change_config *get_payload_odp_on_change_config() {
      return _payload._odp_on_change_config;
   }
   bool is_payload_odp_on_change_config() {
      return _payload_type == SEE_PAYLOAD_ODP_CONFIG;
   }
};

std::string get_client_permission_symbolic(see_tech_e);

enum see_msg_id_e {
   MSGID_ATTR_REQ = 1,
   MSGID_FLUSH_REQ = 2,
   MSGID_DISABLE_REQ = 10, // SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ
   MSGID_RESAMPLER_CONFIG = 512,
   MSGID_GRAVITY_CONFIG = 512,
   MSGID_GYRO_ROT_MATRIX_CONFIG = 512,
   MSGID_BASIC_GESTURES_CONFIG = 512,
   MSGID_MULTISHAKE_CONFIG = 512,
   MSGID_PSMD_CONFIG = 512,
   MSGID_SNS_CAL_RESET = 512,
   MSGID_STD_SENSOR_CONFIG = 513, // SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG
   MSGID_ON_CHANGE_CONFIG = 514,
   MSGID_SELF_TEST_CONFIG = 515,
   MSGID_SIM_START_REQ = 515,
   MSGID_ODP_CONFIG = 515,
   MSGID_SET_DISTANCE_BOUND = 516,
   MSGID_EVENT_GATED_CONFIG = 518,
   MSGID_DIAG_SENSOR_TRIGGER_REQ = 520,
   MSGID_DELTA_ANGLE_REQ = 520,
   MSGID_GET_CM_ACTIVE_CLIENT_REQ = 521,
};

enum see_std_client_processor_e {
   SEE_STD_CLIENT_PROCESSOR_SSC = 0,
   SEE_STD_CLIENT_PROCESSOR_APSS = 1,
   SEE_STD_CLIENT_PROCESSOR_ADSP = 2,
   SEE_STD_CLIENT_PROCESSOR_MDSP = 3,
   SEE_STD_CLIENT_PROCESSOR_CDSP = 4,
   SEE_STD_CLIENT_PROCESSOR_COUNT = 5
};

enum see_client_delivery_e {
   SEE_CLIENT_DELIVERY_WAKEUP = 0,
   SEE_CLIENT_DELIVERY_NO_WAKEUP = 1,
};

class see_client_request_message {
public:
   see_client_request_message(sens_uid *suid,
                              see_msg_id_e msg_id,
                              see_std_request* request) {
      _suid.low = suid->low;
      _suid.high = suid->high;
      _msg_id = msg_id;
      _request = request;
   }
   sens_uid *get_suid() { return &_suid;};
   see_msg_id_e get_msg_id() { return _msg_id;}
   see_std_request *get_request() { return _request;}
   std::string get_client_symbolic( see_std_client_processor_e);
   std::string get_wakeup_symbolic( see_client_delivery_e);

   /**
    * @brief suspend_config
    */
   void set_suspend_config(see_std_client_processor_e client,
                           see_client_delivery_e wakeup) {
       _client = client;
       _wakeup = wakeup;
   }
   void get_suspend_config(see_std_client_processor_e &client,
                           see_client_delivery_e &wakeup) {
       client = _client;
       wakeup = _wakeup;
   }

   void set_processor(see_std_client_processor_e client) {
      _client = client;
   }

   void set_wakeup(see_client_delivery_e wakeup){
      _wakeup = wakeup;
   }

private:
   sens_uid _suid;
   see_msg_id_e _msg_id;

   see_std_client_processor_e _client = SEE_STD_CLIENT_PROCESSOR_APSS;
   see_client_delivery_e _wakeup = SEE_CLIENT_DELIVERY_WAKEUP;

   see_std_request* _request;

};
