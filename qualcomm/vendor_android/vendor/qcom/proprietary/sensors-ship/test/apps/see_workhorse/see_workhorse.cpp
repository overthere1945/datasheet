/** =============================================================================
 *  @file see_workhorse.cpp
 *
 *  @brief Android command line application to stream
 *  physical or virtual sensor @ rate for duration
 *
 *  @copyright Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  All rights reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 *  ===========================================================================
 */

#include <iostream>
#include <iomanip>    // std::setprecision
#include <sstream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <time.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif

#include "see_salt.h"
#include "display_active.h"
#include <util_wakelock.h>

using namespace std;

/* forward declarations */
bool parse_orientation (string value);
void parse_sample_rate (string value);
void parse_wakeup (string value);
bool parse_processor (string value);
bool parse_thresholds_type (string value);
bool parse_distance_bound_motion_state (vector <string> values);
uint32_t get_user_attribute_filter(string& user_attribute_filter);
bool parse_client_permissions(const string& parse_value);
float get_speed_value_from_motion_state (string motion_state);

void event_cb (string sensor_event, unsigned int sensor_handle,
               unsigned int client_connect_id);
sensor* choose_target_sensor (see_salt *psalt, vector <sens_uid *>& sens_uids);
void display_target_sensor (sensor *psensor);
bool resolve_symbolic_sample_rate (sensor *psensor);
salt_rc run (see_salt *psalt, sensor *psensor,
             sens_uid *cal_suid, string cal_sensor);
static int common_exit (salt_rc saltrc, see_salt *psalt);
static void append_log_time_stamp (ostringstream& outss);
static void writeline (ostringstream& outss);
void write_result_description (string passfail);

// SENSORTEST-3076
static std::map <std::string, float> distance_bound_state_to_speed_mapping = {
  { "SNS_DB_MS_STATIONARY", 0.0f },
  { "SNS_DB_MS_WALK", 1.1f },
  { "SNS_DB_MS_RUN", 2.5f },
  { "SNS_DB_MS_BIKE", 5.55f },
  { "SNS_DB_MS_VEHICLE", 16.67f }
};

/* data */
static mutex mtx;              // SNS-660
static condition_variable cv;  // SNS-660
int client_id = -1;
int reserved_client_id = -1;
bool show_usage = false;

static void do_sleep (float duration);
void sleep_and_awake (uint32_t milliseconds);

static string newline ("\n");
static string cmdline ("");
static ostringstream oss;

#define SYMBOLIC_MIN_ODR   1
#define SYMBOLIC_MAX_ODR   2

static sens_uid *diag_sensor_suid = nullptr;
static sens_uid *delta_angle_suid = nullptr;
static util_wakelock *wake_lock = nullptr; // SENSORTEST-1684

struct re_arm {
      bool now = false; // true == rearm requested
      sens_uid *suid;
      string target_sensor = "";
      string suid_low = "";
      string suid_high = "";
      re_arm () {}
};

static re_arm rearm;

struct app_config {
      bool  is_valid = true;
      char  data_type[64]; // type: accel, gyro, mag, ...

      bool  has_name = false;
      char  name[64];

      bool  has_rigid_body = false;
      char  rigid_body[32];

      bool  has_hw_id = false;
      int   hw_id;

      bool  has_on_change = false;
      bool  on_change;

      bool  has_batch_period = false;
      float batch_period;   // seconds

      bool  has_flush_period = false;
      float flush_period;   // seconds

      bool  has_flush_only = false;
      bool  flush_only;

      bool  has_max_batch = false;
      bool  max_batch;

      bool  has_is_passive = false;
      bool  is_passive;

      bool  has_debug = false;
      bool  debug;

      bool  has_island_log = false;
      bool  island_log;

      bool  has_memory_log = false;
      bool  memory_log;

      bool  has_usta_log = false;
      bool  usta_log;

      bool  has_calibrated = false;
      bool  calibrated;
      bool  cal_reset_req = false;

      bool  has_flush_request_interval = false;
      float flush_request_interval;

      bool  has_distance_bound = false;
      float distance_bound = 1.0; // meters

      bool has_motion_state = false;
      vector <string> motion_state;

      bool  has_processor = false;
      see_std_client_processor_e processor = SEE_STD_CLIENT_PROCESSOR_APSS;

      bool  has_wakeup = false;
      see_client_delivery_e wakeup = SEE_CLIENT_DELIVERY_WAKEUP;

      bool  has_delay = false;
      float delay;

      bool  has_flush_log = false;
      int   flush_log;

      bool  display_events = false;

      bool  rearm = false;
      bool  has_rearm_delay = false;
      float rearm_delay = 0; // seconds

      int  times = 1;       // number of times
      float times_delay = 0.25; // seconds

      int symbolic_sample_rate = 0;
      float sample_rate = 10.0; // hz

      float duration = 10.0; // seconds

      bool  has_stream_type = false;
      int   stream_type;

      bool use_usleep = false;

      bool has_force = false;
      string force;

      bool has_get_active = false;
      int  get_active_value;

      bool has_prevent_apps_suspend = false;

      bool has_show_sensors = false;
      bool show_sensors;

      // sns_delta_angle_config
      bool has_orientation = false;
      see_delta_angle_ori_type_e orientation = SEE_DELTA_ANGLE_ORIENTATION_PITCH;

      bool has_radians = false;
      float radians;

      bool has_latency_us = false;
      int latency_us;

      bool has_quat_output_enable = false;
      bool quat_output_enable;

      bool has_quat_history_enable = false;
      bool quat_history_enable;

      bool has_hysteresis_usec = false;
      int hysteresis_usec;

      // Resampler parameters
      bool has_resampler_config = false;
      see_resampler_rate  rate_type = SEE_RESAMPLER_RATE_MINIMUM;
      bool  filter = false;

      // Threshold parameters
      bool has_threshold_config = false;
      vector <string> thresholds;
      see_threshold_type threshold_type = SEE_THRESHOLD_TYPE_RELATIVE_VALUE;

      // Client permissions parameters
      bool has_client_permissions = false;
      vector <see_tech_e> client_permissions;

      // User attribute filter
      bool  has_user_attribute_filter = false;
      string user_attribute_filter;

      app_config () {}
};

static app_config config;

void init_see_std_request (see_std_request *pstd_request) {
  if (config.has_batch_period) {
    oss << "+ batch_period: " << to_string(config.batch_period);
    oss << " seconds";
    writeline(oss);
    pstd_request->set_batch_period(config.batch_period * 1000000);             // microsec
  }
  if (config.has_flush_period) {
    oss << "+ flush_period: " << to_string(config.flush_period);
    oss << " seconds";
    writeline(oss);
    pstd_request->set_flush_period(config.flush_period * 1000000);             // microsec
  }
  if (config.has_flush_only) {
    oss << "+ flush_only: " << to_string(config.flush_only);
    writeline(oss);
    pstd_request->set_flush_only(config.flush_only);
  }
  if (config.has_max_batch) {
    oss << "+ max_batch: " << to_string(config.max_batch);
    writeline(oss);
    pstd_request->set_max_batch(config.max_batch);
  }
  if (config.has_is_passive) {
    oss << "+ is_passive: " << to_string(config.is_passive);
    writeline(oss);
    pstd_request->set_is_passive(config.is_passive);
  }
  if(config.has_client_permissions) {
    oss << "+ client_permissions: {";
    const size_t len = config.client_permissions.size();
    for(size_t idx = 0; idx < len; ++idx) {
        oss << get_client_permission_symbolic(config.client_permissions[idx]);
        if(idx < len - 1) {
            oss << ", ";
        }
    }
    oss << "}";
    writeline(oss);
    pstd_request->set_client_permissions(config.client_permissions);
  }
}

void init_resampler_threshold_configs (see_std_request *pstd_request) {
  if (config.has_resampler_config) {
    pstd_request->set_resampler_type(config.rate_type);
    pstd_request->set_resampler_filter(config.filter);
  }

  if (config.has_threshold_config) {
    pstd_request->set_threshold_type(config.threshold_type);
    pstd_request->set_threshold_value(config.thresholds);
  }
}

void init_processor_wakeup (see_client_request_message& request, bool diag_sensor = false) {
  if (config.has_processor) {
    request.set_processor(config.processor);
    oss << "+ processor: "
      << request.get_client_symbolic(config.processor);
    writeline(oss);
  }
  if (diag_sensor) {
    request.set_wakeup(SEE_CLIENT_DELIVERY_NO_WAKEUP);
    oss << "+ no_wakeup: "
      << request.get_wakeup_symbolic(SEE_CLIENT_DELIVERY_NO_WAKEUP);
    writeline(oss);
  }
  else if (config.has_wakeup) {
    request.set_wakeup(config.wakeup);
    oss << "+ wakeup: "
      << request.get_wakeup_symbolic(config.wakeup);
    writeline(oss);
  }
}

salt_rc send_request (see_salt *psalt, see_client_request_message& request,
                      bool use_reserved_client_id = false) {
  if (psalt == nullptr) {
    oss << "FAIL null psalt ptr";
    writeline(oss);
    return SALT_RC_FAILED;
  }

  salt_rc rc = SALT_RC_FAILED;
  see_client_request_info request_info;

  int& c_id = use_reserved_client_id ? reserved_client_id : client_id;

  if (c_id == -1) {
    request_info.req_type = SEE_CREATE_CLIENT;
    see_register_cb cb;
    if (use_reserved_client_id) {
      cb.sensor_event_cb_func_inst = (sensor_event_cb_func)display_active;
    }
    else {
      cb.sensor_event_cb_func_inst = (sensor_event_cb_func)event_cb;
    }
    request_info.cb_ptr = cb;

    c_id = psalt->send_request(request, request_info);

    if (c_id >= 0) {
      rc = SALT_RC_SUCCESS;
    }
    else {
      rc = SALT_RC_FAILED;
    }
  }
  else {
    request_info.req_type = SEE_USE_CLIENT_CONNECT_ID;
    request_info.client_connect_id = c_id;

    int retval = psalt->send_request(request, request_info);

    if (retval == c_id) {
      rc = SALT_RC_SUCCESS;
    }
    else {
      rc = SALT_RC_FAILED;
    }
  }

  return rc;
}

salt_rc disable_sensor (see_salt *psalt, sens_uid *suid, string target,
                        bool use_reserved_client_id = false) {
  oss << "disable_sensor( " << target << ")";
  writeline(oss);
  see_std_request std_request;
  see_client_request_message request(suid,
                                     MSGID_DISABLE_REQ,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request, use_reserved_client_id);
  oss << "disable_sensor() complete rc " << to_string(rc);

  writeline(oss);
  return rc;
}

salt_rc flush_sensor (see_salt *psalt, sens_uid *suid, string target) {
  oss << "flush_sensor( " << target << ")";
  writeline(oss);
  see_std_request std_request;
  see_client_request_message request(suid,
                                     MSGID_FLUSH_REQ,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "flush_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

/**
 * does_rigid_body_match() is intended to find the mag_cal that matches
 * the rigid_body of selected mag sensor, or the gyro_cal matching the
 * selected gyro sensor.
 *
 * @param psensor
 *
 * @return true when the _cal sensor matches config.rigid_body
 */
bool does_rigid_body_match (sensor *psensor) {
  if (psensor && psensor->has_rigid_bodies()) {
    vector <string> rigid_bodies;
    psensor->get_rigid_bodies(rigid_bodies);

    for (auto i = 0; i < rigid_bodies.size(); i++) {
      if (config.rigid_body == rigid_bodies[i]) {
        return true;
      }
    }
    return false;
  }
  return true;
}

// return pointer to target_sensor's suid
sens_uid* get_sensor_suid (see_salt *psalt, string target) {
  oss << "get_sensor_suid( " << target << ")";
  writeline(oss);
  vector <sens_uid *> sens_uids;
  psalt->get_sensors(target, sens_uids);
  if (sens_uids.size() == 0) {
    return nullptr;
  }
  size_t index = 0;
  if (target.find("_cal") != string::npos) {
    for (; index < sens_uids.size(); index++) {
      sens_uid *candidate_suid = sens_uids[index];
      sensor *psensor = psalt->get_sensor(candidate_suid);
      if (!does_rigid_body_match(psensor)) {
        continue;
      }
      oss << "+ found " << target << " " << config.rigid_body;
      writeline(oss);
      break;
    }
  }
  index = (index < sens_uids.size()) ? index : sens_uids.size() - 1;
  oss << "+ " << target;
  oss << " suid = [high " << hex << sens_uids[index]->high;
  oss << ", low " << hex << sens_uids[index]->low << "]";
  writeline(oss);

  return sens_uids[index];
}

salt_rc trigger_diag_sensor_log (see_salt *psalt,
                                 see_diag_trigger_log_type log_type,
                                 uint64_t cookie) {
  if (diag_sensor_suid == nullptr) {
    cout << "Skipping optional request to diag_sensor" << endl;
    return SALT_RC_SUCCESS;
  }

  oss << "trigger_diag_sensor_log( " << dec << log_type << ")";
  oss << " cookie: " << dec << cookie;
  writeline(oss);

  if (log_type == SEE_LOG_TYPE_MEMORY_USAGE_LOG) {
    do_sleep(2);                 // hoping ssc goes idle, as per Dilip email
  }

  see_diag_log_trigger_req trigger(log_type, cookie);
  see_std_request std_request;
  std_request.set_payload(&trigger);
  see_client_request_message request(diag_sensor_suid,
                                     MSGID_DIAG_SENSOR_TRIGGER_REQ,
                                     &std_request);
  init_processor_wakeup(request, true);
  salt_rc rc = send_request(psalt, request, true);
  oss << "trigger_diag_sensor_log() complete rc " << to_string(rc);
  writeline(oss);

  if (rc == SALT_RC_SUCCESS) {
    sleep_and_awake(100);             // required to properly flush diag trigger log
    rc = disable_sensor(psalt, diag_sensor_suid, "diag_sensor", true);
    reserved_client_id = -1;
  }

  return rc;
}

// SENSORTEST-1554
salt_rc diag_sensor_get_active (see_salt *psalt) {
  if (diag_sensor_suid == nullptr) {
    cout << "Skipping optional request to diag_sensor" << endl;
    return SALT_RC_SUCCESS;
  }

  oss << "diag_sensor_get_active()";
  writeline(oss);

  register_diag_sensor_event_cb(psalt, config.display_events);       // SENSORTEST-2110

  see_std_request std_request;       // SENSORTEST-1661
  see_client_request_message request(diag_sensor_suid,
                                     MSGID_GET_CM_ACTIVE_CLIENT_REQ,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request, true);
  oss << "diag_sensor_get_active() complete rc " << to_string(rc);
  writeline(oss);

  if (rc == SALT_RC_SUCCESS) {
    rc = disable_sensor(psalt, diag_sensor_suid, "diag_sensor", true);
    reserved_client_id = -1;
  }

  return rc;
}

salt_rc on_change_sensor (see_salt *psalt, sens_uid *suid, string target) {
  oss << "on_change_sensor( " << target << ")";
  writeline(oss);

  see_std_request std_request;
  init_see_std_request(&std_request);

  see_msg_id_e msg_id = MSGID_ON_CHANGE_CONFIG;
  if (config.has_force && config.force == "gated") {
    msg_id = (see_msg_id_e)518;
  }

  see_client_request_message request(suid,
                                     msg_id,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "on_change_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc distance_bound_sensor (see_salt *psalt, sens_uid *suid) {
  oss << "distance_bound_sensor()";
  writeline(oss);

  see_std_request std_request;
  init_see_std_request(&std_request);

  std::vector<std::pair<std::string, std::string>> states_speeds;

  if (config.has_motion_state && config.motion_state.size() > 0) {
    for (int i = 0; i < config.motion_state.size(); i++) {
      string state = config.motion_state[i];
      string speed = to_string(get_speed_value_from_motion_state(state));
      states_speeds.push_back(make_pair(state, speed));
    }
  }
  else {
    cout << "FAIL Unsupported motion state found." << endl;
  }

  if (config.debug) {
    for (int i = 0; i < states_speeds.size(); i++) {
      cout << to_string(i) << " " << states_speeds[i].first << ": " << states_speeds[i].second << endl;
    }
  }

  see_set_distance_bound distance_bound_req = see_set_distance_bound(config.distance_bound, states_speeds);
  oss << "using -distance_bound = " << distance_bound_req.get_distance_bound() << endl;
  oss << "using distance_bound motion states: ";
  for (int i = 0; i < config.motion_state.size(); i++) {
    oss << config.motion_state[i] << ", ";
  }
  writeline(oss);

  std_request.set_payload(&distance_bound_req);
  see_client_request_message request(suid,
                                     MSGID_SET_DISTANCE_BOUND,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "distance_bound_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc basic_gestures_sensor (see_salt *psalt, sens_uid *suid) {
  oss << "basic_gestures_sensor()";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request(&std_request);

  see_basic_gestures_config config;
  std_request.set_payload(&config);
  see_client_request_message request(suid,
                                     MSGID_BASIC_GESTURES_CONFIG,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "basic_gestures_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc multishake_sensor (see_salt *psalt, sens_uid *suid) {
  oss << "multishake_sensor()";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request(&std_request);

  see_multishake_config config;
  std_request.set_payload(&config);
  see_client_request_message request(suid,
                                     MSGID_MULTISHAKE_CONFIG,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "multishake_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc psmd_sensor (see_salt *psalt, sens_uid *suid) {
  oss << "psmd_sensor()";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request(&std_request);

  see_psmd_config config(SEE_PSMD_TYPE_STATIONARY);
  std_request.set_payload(&config);
  see_client_request_message request(suid,
                                     MSGID_PSMD_CONFIG,
                                     &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "psmd_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc stream_sensor (see_salt *psalt, see_msg_id_e msg_id,
                       sens_uid *suid, string target) {
  oss << "stream_sensor( " << target << ")";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request(&std_request);
  init_resampler_threshold_configs(&std_request);

  if (config.has_force && config.force == "gated") {
    msg_id = (see_msg_id_e)518;
  }

  oss << "+ sample_rate: " << to_string(config.sample_rate) << " hz";
  writeline(oss);
  see_std_sensor_config sample_rate(config.sample_rate);       // hz
  std_request.set_payload(&sample_rate);
  see_client_request_message request(suid, msg_id, &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "config_stream_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

/* delta_angle_config */
salt_rc delta_angle_config (see_salt *psalt, sens_uid *suid) {
  oss << "delta_angle_config()";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request(&std_request);

  if (!(config.has_orientation && config.has_radians
        && config.has_latency_us)) {
    oss << "FAIL delta_angle_config missing orientation | radians | latency"
      << endl;
    return SALT_RC_NULL_ARG;
  }

  see_delta_angle_req angle_req = see_delta_angle_req(config.orientation,
                                                      config.radians,
                                                      config.latency_us);
  if (config.has_quat_output_enable) {
    angle_req.set_quat_output_enable(config.quat_output_enable);
  }
  if (config.has_quat_history_enable) {
    angle_req.set_quat_history_enable(config.quat_history_enable);
  }
  if (config.has_hysteresis_usec) {
    angle_req.set_hysteresis_usec(config.hysteresis_usec);
  }

  std_request.set_payload(&angle_req);
  see_msg_id_e msg_id = MSGID_DELTA_ANGLE_REQ;
  see_client_request_message request(suid, msg_id, &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "delta_angle_complete() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc odp_sensor(see_salt *psalt, sens_uid *suid) {
  oss << "odp_sensor()";
  writeline(oss);
  see_std_request std_request;
  init_see_std_request( &std_request);
  see_odp_on_change_config odp_config;

  if (config.has_user_attribute_filter) {
    uint32_t user_attribute_filter = get_user_attribute_filter(config.user_attribute_filter);
    odp_config.set_user_attribute_filter(user_attribute_filter);
  }

  std_request.set_payload(&odp_config);
  see_client_request_message request( suid,
                                      MSGID_ODP_CONFIG,
                                      &std_request);
  init_processor_wakeup(request);
  salt_rc rc = send_request(psalt, request);
  oss << "odp_sensor() complete rc " << to_string(rc);
  writeline(oss);
  return rc;
}

salt_rc send_sns_cal_reset_req( see_salt *psalt, sens_uid *suid)
{
    oss << "send_sns_cal_reset_req()";
    writeline(oss);
    see_std_request std_request;
    init_see_std_request( &std_request);
    see_client_request_message request( suid,
                                        MSGID_SNS_CAL_RESET,
                                        &std_request);
    init_processor_wakeup(request);
    salt_rc rc = send_request(psalt, request);
    oss << "send_sns_cal_reset_req() complete rc " << to_string(rc);
    writeline(oss);
    return rc;
}

void usage (void) {
  cout << "usage: see_workhorse\n"
          " [-sensor=<sensor_type>]\n"
          " [-name=<name>] [-on_change=<0 | 1>] [-stream_type=<stream_type>]\n"
          " [-rigid_body=<display | keyboard>] [-hw_id=<number>]\n"
          " [-delay=<seconds>] [-is_passive=<0 | 1>]\n"
          " [-prevent_apps_suspend=<0 | 1>]\n"
          " [-sample_rate=<min | max | digits>] [-rearm=<0 | 1>]\n"
          " [-rearm_delay=<seconds>]\n"
          " [-batch_period=<seconds>] [-flush_period=<seconds>]\n"
          " [-max_batch=<0 | 1>] [-flush_only=<0 | 1>]\n"
          " [-flush_request_interval=<seconds>]\n"
          " [-usta_log=<0 | 1>] [-usleep=<0 | 1>] [-calibrated=<0 | 1>] [-cal_reset] \n"
          " [-distance_bound=<meters>] [motion_state=<number>,...]\n"
          " [-force=streaming | on_change | gated]\n"
          " [-wakeup=<0 | 1>] [-processor=<ssc | apss | adsp | mdsp | cdsp>]\n"
          " [-display_events=<0 | 1>]\n"
          " [-rate_type=<fixed | minimum>] [-filter=<0 | 1>]\n"
          " [-thresholds=<number>,...] [-thresholds_type=relative | percent | absolute | angle]\n"
          " [-user_attribute_filter=location_type | age | fitness | activity | interests]\n"
          " -duration=<seconds> [-times=<number>] [-times_delay=<seconds>] [-debug=<0 | 1>]\n"
          " [-show_sensors=1]\n";

  if (diag_sensor_suid != nullptr) {
  cout << " [-get_active |-get_active=2] [-memory_log=<0 | 1>]\n"
          " [-island_log=<0 | 1>] [-flush_log=number]\n";
  }

  if (delta_angle_suid != nullptr) {
  cout << " [-orientation=<pitch | roll | heading | total>] [-radians=<number>]"
          " [-latency_us=<number>] [-quat_output_enable=<0 | 1>] [-quat_history_enable=<0 | 1>]\n";
  }

  cout << " [-help]\n"
          " where: <sensor_type>  :: accel | gyro | ...\n"
          "        <stream_type>  :: 0 streaming | 1 on_change, 2 single_output\n"
          "        <motion_state> :: 1 -> stationary | 2 -> move | 3 -> fiddle | 4 -> pedestrian | 5 -> vehicle | 6 -> walk | \n"
          "                          7 -> run | 8 -> bike | 9 -> max\n";
}

/**
 * @brief parse command line argument of the form keyword=value
 * @param parg[i] - command line argument
 * @param key[io] - gets string to left of '='
 * @param value[io] - sets string to right of '='
 */
bool get_key_value (char *parg, string& key, string& value) {
  char *pkey = parg;
  key = string(pkey, strlen(parg));       // SENSORTEST-1554

  while (char c = *parg) {
    if (c == '=') {
      key = string(pkey, parg - pkey);
      value = string(++parg);
      return true;
    }
    parg++;
  }
  return false;
}

/**
* Parse command line arguments
*
* @param argc
* @param argv
*
* @return int 0 == successful, <> 0 failed
*/
int parse_arguments (int argc, char *argv[]) {
  // command line args:
  string sensor_eq("-sensor");
  string name_eq("-name");
  string rigid_body_eq("-rigid_body");
  string on_change_eq("-on_change");
  string hw_id_eq("-hw_id");
  string is_passive_eq("-is_passive");
  string sample_rate_eq("-sample_rate");
  string batch_period_eq("-batch_period");
  string flush_request_interval_eq("-flush_request_interval");
  string flush_period_eq("-flush_period");
  string flush_only_eq("-flush_only");
  string max_batch_eq("-max_batch");
  string island_log_eq("-island_log");
  string memory_log_eq("-memory_log");
  string client_permissions_eq("-client_permissions");
  string usta_log_eq("-usta_log");
  string duration_eq("-duration");
  string debug_eq("-debug");
  string calibrated_eq("-calibrated");
  string cal_reset_eq("-cal_reset");
  string distance_bound_eq("-distance_bound");
  string motion_state_eq("-motion_state");
  string wakeup_eq("-wakeup");
  string processor_eq("-processor");
  string display_events_eq("-display_events");
  string times_eq("-times");
  string times_delay_eq("-times_delay");
  string delay_eq("-delay");
  string rearm_eq("-rearm");
  string rearm_delay_eq("-rearm_delay");
  string flush_log_eq("-flush_log");
  string stream_type_eq("-stream_type");
  string usleep_eq("-usleep");
  string force_eq("-force");
  string get_active_eq("-get_active");
  string prevent_apps_suspend_eq("-prevent_apps_suspend");
  string show_sensors_eq("-show_sensors");
  string orientation_eq("-orientation");       // delta_angle_config
  string radians_eq("-radians");
  string latency_us_eq("-latency_us");
  string quat_output_enable_eq("-quat_output_enable");
  string quat_history_enable_eq("-quat_history_enable");
  string hysteresis_usec_eq("-hysteresis_usec");

  // Resampler arguments
  string rate_type_eq("-rate_type");
  string filter_eq("-filter");

  // Threshold arguments
  string fixed_eq("-fixed");
  string thresholds_eq("-thresholds");
  string thresholds_type_eq("-thresholds_type");

  string user_attribute_filter_eq("-user_attribute_filter");

  for (int i = 1; i < argc; i++) {
    string key;
    string value;
    if (get_key_value(argv[i], key, value)) {
      if (sensor_eq == key) {
        strlcpy(config.data_type, value.c_str(),
                sizeof(config.data_type));
      }
      else if (name_eq == key) {
        config.has_name = true;
        strlcpy(config.name, value.c_str(), sizeof(config.name));
      }
      else if (rigid_body_eq == key) {
        config.has_rigid_body = true;
        strlcpy(config.rigid_body, value.c_str(),
                sizeof(config.rigid_body));
      }
      else if (on_change_eq == key) {
        config.has_on_change = true;
        config.on_change = atoi(value.c_str());
      }
      else if (hw_id_eq == key) {
        config.has_hw_id = true;
        config.hw_id = atoi(value.c_str());
      }
      else if (is_passive_eq == key) {
        config.has_is_passive = true;
        config.is_passive = atoi(value.c_str());
      }
      else if (max_batch_eq == key) {
        config.has_max_batch = true;
        config.max_batch = atoi(value.c_str());
      }
      else if (flush_request_interval_eq == key) {
        config.has_flush_request_interval = true;
        config.flush_request_interval = atof(value.c_str());
      }
      else if (sample_rate_eq == key) {
        parse_sample_rate(value);
      }
      else if (batch_period_eq == key) {
        config.has_batch_period = true;
        config.batch_period = atof(value.c_str());
      }
      else if (flush_period_eq == key) {
        config.has_flush_period = true;
        config.flush_period = atof(value.c_str());
      }
      else if (flush_only_eq == key) {
        config.has_flush_only = true;
        config.flush_only = atoi(value.c_str());
      }
      else if (duration_eq == key) {
        config.duration = atof(value.c_str());
      }
      else if (debug_eq == key) {
        config.has_debug = true;
        config.debug = atoi(value.c_str());
      }
      else if (island_log_eq == key) {
        config.has_island_log = true;
        config.island_log = atoi(value.c_str());
      }
      else if (memory_log_eq == key) {
        config.has_memory_log = true;
        config.memory_log = atoi(value.c_str());
      }
      else if (client_permissions_eq == key) {
        parse_client_permissions(value);
      }
      else if (usta_log_eq == key) {
        config.has_usta_log = true;
        config.usta_log = atoi(value.c_str());
      }
      else if (calibrated_eq == key) {
        config.has_calibrated = true;
        config.calibrated = atoi(value.c_str());
      }
      else if (distance_bound_eq == key) {
        config.has_distance_bound = true;
        config.distance_bound = atof(value.c_str());
      }
      else if (motion_state_eq == key) {
        config.has_motion_state = true;
        stringstream csv(value);
        string element;
        while (getline(csv, element, ',')) {
          config.motion_state.push_back(element.c_str());
        }
        if (!parse_distance_bound_motion_state(config.motion_state)) {
          config.has_motion_state = false;
          config.is_valid = false;
        }
      }
      else if (times_eq == key) {
        config.times = atoi(value.c_str());
      }
      else if (times_delay_eq == key) {
        config.times_delay = atoi(value.c_str());
      }
      else if (delay_eq == key) {
        config.has_delay = true;
        config.delay = atof(value.c_str());
      }
      else if (flush_log_eq == key) {
        config.has_flush_log = true;
        config.flush_log = atoi(value.c_str());
      }
      else if (rearm_eq == key) {
        config.rearm = atoi(value.c_str());
      }
      else if (rearm_delay_eq == key) {
        config.has_rearm_delay = true;
        config.rearm_delay = atof(value.c_str());
      }
      else if (display_events_eq == key) {
        config.display_events = atoi(value.c_str());
      }
      else if (wakeup_eq == key) {
        parse_wakeup(value);
      }
      else if (processor_eq == key) {
        config.is_valid = parse_processor(value);
      }
      else if (stream_type_eq == key) {
        config.has_stream_type = true;
        config.stream_type = atoi(value.c_str());
        if (config.stream_type < 0 || config.stream_type > 2) {
          cout << "FAIL invalid stream_type value " << value.c_str() << endl;
          config.is_valid = false;
        }
      }
      else if (usleep_eq == key) {
        config.use_usleep = atoi(value.c_str());
      }
      else if (force_eq == key) {
        config.has_force = true;
        config.force = value;
      }
      else if (get_active_eq == key) {
        config.has_get_active = true;
        config.get_active_value = atoi(value.c_str());
      }
      else if (prevent_apps_suspend_eq == key) {
        config.has_prevent_apps_suspend = atoi(value.c_str());
      }
      else if (show_sensors_eq == key) {
        config.has_show_sensors = true;
        config.show_sensors = atoi(value.c_str());
      }
      else if (orientation_eq == key) {
        config.has_orientation = parse_orientation(value);
      }
      else if (radians_eq == key) {
        config.has_radians = true;
        config.radians = atof(value.c_str());
      }
      else if (latency_us_eq == key) {
        config.has_latency_us = true;
        config.latency_us = atoi(value.c_str());
      }
      else if (quat_output_enable_eq == key) {
        config.has_quat_output_enable = true;
        config.quat_output_enable = atoi(value.c_str());
      }
      else if (quat_history_enable_eq == key) {
        config.has_quat_history_enable = true;
        config.quat_history_enable = atoi(value.c_str());
      }
      else if (hysteresis_usec_eq == key) {
        config.has_hysteresis_usec = true;
        config.hysteresis_usec = atoi(value.c_str());
      }
      else if (rate_type_eq == key) {
        config.has_resampler_config = true;
        if (value == string("fixed")) {
          config.rate_type = SEE_RESAMPLER_RATE_FIXED;
        }
        else if (value == string("minimum")) {
          config.rate_type = SEE_RESAMPLER_RATE_MINIMUM;
        }
        else {
          config.is_valid = false;
          cout << "FAIL bad rate type value: " << value << endl;
        }
      }
      else if (fixed_eq == key) {
        config.has_resampler_config = true;
        if (atoi(value.c_str())) {
          config.rate_type = SEE_RESAMPLER_RATE_FIXED;
        }
        else {
          config.rate_type = SEE_RESAMPLER_RATE_MINIMUM;
        }
      }
      else if (filter_eq == key) {
        config.has_resampler_config = true;
        config.filter = atoi(value.c_str());
      }
      else if (thresholds_eq == key) {
        config.has_threshold_config = true;
        stringstream csv(value);
        string element;
        while (getline(csv, element, ',')) {
          config.thresholds.push_back(element.c_str());
        }
      }
      else if (thresholds_type_eq == key) {
        config.has_threshold_config = true;
        if (!parse_thresholds_type(value)) {
          config.is_valid = false;
        }
      }
      else if (user_attribute_filter_eq == key) {
        config.user_attribute_filter = value;
        config.has_user_attribute_filter = true;
      }
      else {
        config.is_valid = false;
        cout << "FAIL unrecognized arg " << argv[i] << endl;
      }
    }
    else if (get_active_eq == key) {
      config.has_get_active = true;
      config.get_active_value = -1;                   // single_shot
    }
    else if (cal_reset_eq == key) {
      config.cal_reset_req = true;
    }
    else if (0 == strncmp(argv[i], "-h", 2)) {
      show_usage = true;
      return 0;
    }
    else {
      config.is_valid = false;
      cout << "FAIL unrecognized arg " << argv[i] << endl;
    }
  }
  if (argc == 1) {
    cout << "FAIL missing args" << endl;
  }
  if (!config.is_valid || argc == 1) {
    show_usage = true;
    return 4;             // failed
  }
  return 0;       // successful parse
}

/* parse delta_angle_orientation*/
bool parse_orientation (string value) {
  bool rc = true;
  if (value == string("pitch")) {
    config.orientation = SEE_DELTA_ANGLE_ORIENTATION_PITCH;
  }
  else if (value == string("roll")) {
    config.orientation = SEE_DELTA_ANGLE_ORIENTATION_ROLL;
  }
  else if (value == string("heading")) {
    config.orientation = SEE_DELTA_ANGLE_ORIENTATION_HEADING;
  }
  else if (value == string("total")) {
    config.orientation = SEE_DELTA_ANGLE_ORIENTATION_TOTAL;
  }
  else {
    rc = false;
    config.is_valid = false;
    cout << "FAIL unrecognized orientation " << value << endl;
  }
  return rc;
}

/* parse input from -sample_rate=<min | max | number> */
void parse_sample_rate (string value) {
  if (value.compare("min") == 0) {
    config.symbolic_sample_rate = SYMBOLIC_MIN_ODR;
  }
  else if (value.compare("max") == 0) {
    config.symbolic_sample_rate = SYMBOLIC_MAX_ODR;
  }
  else {
    config.sample_rate = atof(value.c_str());
  }
}

/* parse input value from -wakeup=<value> */
void parse_wakeup (string value) {
  config.has_wakeup = true;
  config.wakeup = SEE_CLIENT_DELIVERY_WAKEUP;
  if (0 == atoi(value.c_str())) {
    config.wakeup = SEE_CLIENT_DELIVERY_NO_WAKEUP;
  }
}

/* parse input value from -processor=<value> */
bool parse_processor (string value) {
  bool rc = true;
  config.has_processor = true;
  if (value == string("ssc")) {
    config.processor =  SEE_STD_CLIENT_PROCESSOR_SSC;
  }
  else if (value == string("apss")) {
    config.processor =  SEE_STD_CLIENT_PROCESSOR_APSS;
  }
  else if (value == string("adsp")) {
    config.processor =  SEE_STD_CLIENT_PROCESSOR_ADSP;
  }
  else if (value == string("mdsp")) {
    config.processor =  SEE_STD_CLIENT_PROCESSOR_MDSP;
  }
  else if (value == string("cdsp")) {
    config.processor =  SEE_STD_CLIENT_PROCESSOR_CDSP;
  }
  else {
    rc = false;
    cout << "FAIL unrecognized processor " << value << endl;
  }
  return rc;
}

/**
* Parse input thresholds_type from -threslholds_type=[value]
*
* @param value
*
* @return true: success, false: fail
*/
bool parse_thresholds_type (string value) {
  if ("relative" == value) {
    config.threshold_type =  SEE_THRESHOLD_TYPE_RELATIVE_VALUE;
  }
  else if ("percent" == value) {
    config.threshold_type =  SEE_THRESHOLD_TYPE_RELATIVE_PERCENT;
  }
  else if ("angle" == value) {
    config.threshold_type =  SEE_THRESHOLD_TYPE_ANGLE;
  }
  else if ("absolute" == value) {
    config.threshold_type =  SEE_THRESHOLD_TYPE_ABSOLUTE;
  }
  else {
    oss << "bad thresholds_type value: " << value << endl;
    writeline(oss);
    return false;
  }
  return true;
}

/**
* Parse input motion_state from -motion_state=[value]
*
* @param value
*
* @return true: success, false: fail
*/
bool parse_distance_bound_motion_state (vector <string> values) {
  int i = 0;
  for (int it = 0; it < values.size(); it++) {
    if ("1" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_STATIONARY";
    }
    else if ("2" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_MOVE";
    }
    else if ("3" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_FIDDLE";
    }
    else if ("4" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_PEDESTRIAN";
    }
    else if ("5" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_VEHICLE";
    }
    else if ("6" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_WALK";
    }
    else if ("7" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_RUN";
    }
    else if ("8" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_BIKE";
    }
    else if ("9" == values[it]) {
      config.motion_state[i] = "SNS_DB_MS_MAX";
    }
    else {
      oss << "Bad motion_state value (" << values[it] << ") for distance_bound" << endl;
      writeline(oss);
      return false;
    }
    i++;
  }
  return true;
}

bool parse_client_permissions(const string& parse_value) {
  bool rc = true;

  stringstream csv(parse_value);
  string element;
  while (getline(csv, element, ',')) {
    if (element == string("uninitialized")) {
      config.client_permissions.push_back(SEE_TECH_UNINITIALIZED);
    }
    else if (element == string("sensors")) {
      config.client_permissions.push_back(SEE_TECH_SENSORS);
    }
    else if (element == string("audio")) {
      config.client_permissions.push_back(SEE_TECH_AUDIO);
    }
    else if (element == string("camera")) {
      config.client_permissions.push_back(SEE_TECH_CAMERA);
    }
    else if (element == string("location")) {
      config.client_permissions.push_back(SEE_TECH_LOCATION);
    }
    else if (element == string("modem")) {
      config.client_permissions.push_back(SEE_TECH_MODEM);
    }
    else if (element == string("bluetooth")) {
      config.client_permissions.push_back(SEE_TECH_BLUETOOTH);
    }
    else if (element == string("wifi")) {
      config.client_permissions.push_back(SEE_TECH_WIFI);
    }
    else if (element == string("connectivity")) {
      config.client_permissions.push_back(SEE_TECH_CONNECTIVITY);
    }
    else {
      rc = false;
      config.is_valid = false;
      cout << "FAIL unrecognized technology " << element << " passed in -client_permissions" << endl;
    }
  }

  if(rc && (config.client_permissions.size() > 0)) {
    config.has_client_permissions = true;
  }

  return rc;
}

/**
* Get the speed value corresponding to the distance_bound motion
* state value
*
* @param motion state
*
* @return speed value in m/s
*/
float get_speed_value_from_motion_state (string motion_state) {
  if (distance_bound_state_to_speed_mapping.find(motion_state)
      != distance_bound_state_to_speed_mapping.end()) {
    return distance_bound_state_to_speed_mapping[motion_state];
  }
  else return 0.0f;
}

/**
* Get the user attribute filter
*
* @param user_attribute_filter user attribute filter string
*
* @return user attribute filter mask
*/
uint32_t get_user_attribute_filter(string& user_attribute_filter) {
  stringstream csv(user_attribute_filter);
  string attribute;
  uint32_t mask = 0;
  while (getline(csv, attribute, ',')) {
    if (attribute == string("location_type"))
        mask |= 1 << 0;
    else if (attribute == string("age"))
        mask |= 1 << 1;
    else if (attribute == string("fitness"))
        mask |= 1 << 2;
    else if (attribute == string("activity"))
        mask |= 1 << 3;
    else if (attribute == string("interests"))
        mask |= 1 << 4;
  }

  return mask;
}

void get_clock_mono (struct timespec& start) {
  clock_gettime(CLOCK_MONOTONIC, &start);
}

void display_elapse (struct timespec& start,
                     struct timespec& end,
                     string& eyewitness) {
  long mtime, seconds, nseconds;
  seconds  = end.tv_sec  - start.tv_sec;
  nseconds = end.tv_nsec - start.tv_nsec;
  mtime = ((seconds)*1000 + nseconds / 1000000.0);

  oss << eyewitness << dec << mtime << " milliseconds";
  writeline(oss);
}

/**
 * event_cb function as registered with usta.
 * display these events and perform rearming as necessary
 *
 * @param events
 * @param sensor_handle
 * @param client_connect_id
 */
void event_cb (string sensor_event, unsigned int sensor_handle,
               unsigned int client_connect_id) {
  if (config.display_events) {
    cout << sensor_event << endl;
  }

  if (!config.rearm) {
    return;
  }

  size_t found_high = sensor_event.find(rearm.suid_high);
  size_t found_low = sensor_event.find(rearm.suid_low);

  // if found single_output event from rearm sensor
  if (found_high != string::npos && found_low != string::npos) {
    cout << "begin rearm..." << endl;
    unique_lock <mutex> lk(mtx);
    rearm.now = true;
    cv.notify_one();
  }
}

/**
 * Monitor the rearm.now variable for the duration of the test.
 * When notified and rearm.now is set, call on_change_sensor to rearm
 *
 * @param psalt
 * @param duration - seconds
 */
void monitor_and_rearm (see_salt *psalt, double duration) {
  oss << "sleep_and_rearm( " << duration << ") seconds";
  writeline(oss);

  /* compute when to timeout as now + test duration */
  auto now = std::chrono::system_clock::now();
  auto interval = 1000ms *duration;       // 1000ms * duration;
  auto then = now + interval;

  unique_lock <mutex> lk(mtx);
  while (true) {
    if (cv.wait_until(lk, then) == std::cv_status::timeout) {
      break;
    }

    if (rearm.now) {
      rearm.now = false;
      if (config.rearm) {
        if (config.has_rearm_delay) {
          do_sleep(config.rearm_delay); // seconds
        }
        oss << "rearming " << rearm.target_sensor;
        writeline(oss);
        on_change_sensor(psalt, rearm.suid, rearm.target_sensor);
      }
    }
  }
}

/**
 * @desc do_sleep - sleep_and_awake() can awake suspended apps processor
 * @param duration - seconds
 */
static void do_sleep (float duration) {
  oss << "sleep(" << duration << ") seconds";
  writeline(oss);
  if (config.use_usleep) {
    usleep((int)(duration * 1000000));
  }
  else {
    sleep_and_awake((int)(duration * 1000));
    oss << "awoke";
    writeline(oss);
  }
}

int main (int argc, char *argv[]) {
  cout << "see_workhorse version 2.13" << endl;

  // save and display commandline
  for (int i = 0; i < argc; i++) {
    oss << argv[i] << " ";
  }
  cmdline = oss.str();
  writeline(oss);

  // initialization for LE version
  config.distance_bound = 1.0;
  config.processor = SEE_STD_CLIENT_PROCESSOR_APSS;
  config.wakeup = SEE_CLIENT_DELIVERY_WAKEUP;
  config.times = 1;
  config.sample_rate = 10.0;
  config.duration = 10.0;
  config.use_usleep = false;

  salt_rc saltrc = SALT_RC_FAILED;
  int argparse_rc = parse_arguments(argc, argv);

  struct timespec start, end;
  get_clock_mono( start);

  if (config.has_usta_log) {
      set_usta_logging( config.usta_log); // enable or disable usta logging
  }

  if (config.has_delay) {
    oss << "delay startup by " << config.delay << " seconds";
    writeline(oss);
    do_sleep(config.delay); // implement startup delay
  }

  see_salt *psalt = see_salt::get_instance();
  if ( psalt == nullptr) {
      oss << "FAIL see_salt::get_instance() failed";
      writeline(oss);
      return common_exit(saltrc, nullptr);
  }

  if ( config.has_debug) {
      psalt->set_debugging( config.debug);
  }
  psalt->begin();

  get_clock_mono( end);
  string begin_witness("psalt->begin() took ");
  display_elapse( start, end, begin_witness);

  // get diag sensor suid
  diag_sensor_suid = get_sensor_suid(psalt, "diag_sensor"); // SENSORTEST-1554

  // get delta_angle sensor suid
  delta_angle_suid = get_sensor_suid(psalt, "delta_angle");

  // show app usage
  if (show_usage) {
      usage();

      if (argparse_rc) {
        return common_exit(SALT_RC_FAILED, psalt);
      } else {
        return common_exit(SALT_RC_SUCCESS, psalt);
      }
  }

  if (config.has_prevent_apps_suspend) {       // SENSORTEST-1684
    oss << "util_wakelock::get_instance()";
    writeline(oss);
    wake_lock = util_wakelock::get_instance();
    if (wake_lock == nullptr) {
      oss << "FAIL util_wakelock::get_instance() failed";
      writeline(oss);
      return common_exit(saltrc, nullptr);
    }

    char *wl_file = wake_lock->get_n_locks(1);
    oss << "acquire wakelock ";
    if (wl_file != nullptr) {
      oss << wl_file;
    }
    writeline(oss);

    if (config.data_type[0] == '\0') {
      do_sleep(config.duration);
      return common_exit(SALT_RC_SUCCESS, nullptr);
    }
  }

  if (config.has_get_active && config.get_active_value == -1) {
    saltrc = diag_sensor_get_active(psalt);             // single shot get active
    do_sleep(1);
    return common_exit(saltrc, psalt);
  }

  if (config.has_show_sensors && config.show_sensors) {
    oss << "get_sensors( sensor_types):";
    writeline(oss);
    vector <string> sensor_types;
    psalt->get_sensors(sensor_types);             // get vector of sensor names
    for (size_t i = 0; i < sensor_types.size(); i++) {
      oss << "    " << sensor_types[i];
      writeline(oss);
    }
    if (config.data_type[0] == 0) {
      return common_exit(SALT_RC_SUCCESS, psalt);
    }
  }

  /* get vector of all suids for target sensor */
  vector <sens_uid *> sens_uids;
  string target_sensor = string(config.data_type);
  oss << "lookup: " << target_sensor;
  writeline(oss);
  psalt->get_sensors(target_sensor, sens_uids);
  for (size_t i = 0; i < sens_uids.size(); i++) {
    sens_uid *suid = sens_uids[i];
    oss << "suid = [high " << hex << suid->high;
    oss << ", low " << hex << suid->low << "]";
    writeline(oss);
  }

  if (0 == sens_uids.size()) {
    if (target_sensor == "") oss << "FAIL " << "missing -sensor=<sensor_type>";
    else oss << "FAIL " << target_sensor << " not found";

    writeline(oss);
    return common_exit(saltrc, psalt);
  }

  /* choose target_suid based on type, name, on_change */
  sensor *psensor = choose_target_sensor(psalt, sens_uids);
  display_target_sensor(psensor);
  if (psensor == nullptr) {
    return common_exit(saltrc, psalt);
  }

  string plan_of_record = "is NOT POR.";
  if (psensor->has_por() && psensor->get_por()) {
    plan_of_record = "is POR.";
  }
  oss << plan_of_record;
  writeline(oss);

  if (config.symbolic_sample_rate) {
    if (!resolve_symbolic_sample_rate(psensor)) {
      return common_exit(saltrc, psalt);
    }
  }
  else if (psensor->has_max_rate()) {
    float max_rate = psensor->get_max_rate();
    oss << "has max_rate: " << dec << max_rate << " hz";
    writeline(oss);
  }

  // SENSORTEST-1652 rearm any single_output sensor
  if (config.rearm) {
    if (target_sensor == "motion_detect") {
      oss << "rearm not supported for motion_detect sensor";
      writeline(oss);
      return common_exit(saltrc, psalt);
    }
    if (!psensor->has_stream_type()) {
      oss << "rearm " << target_sensor << " missing stream_type";
      writeline(oss);
      return common_exit(saltrc, psalt);
    }
    if (!psensor->is_stream_type_single_output()) {
      oss << "rearm " << target_sensor << " is not single_output";
      writeline(oss);
      return common_exit(saltrc, psalt);
    }

    rearm.target_sensor = target_sensor;

    sens_uid *suid = psensor->get_suid();
    oss << hex << suid->high;
    rearm.suid_high = oss.str();
    oss.str("");
    oss << hex << suid->low;
    rearm.suid_low = oss.str();
    oss.str("");

    oss << "rearm " << target_sensor << " "
      << rearm.suid_high << " " << rearm.suid_low;
    writeline(oss);
  }

  // conditionally lookup calibration sensor
  sens_uid *cal_suid = NULL;
  string cal_sensor = "";
  if (config.has_calibrated && config.calibrated) {
    if (target_sensor == string("gyro") || target_sensor == string("mag")) {
      cal_sensor = target_sensor + "_cal";
      cal_suid = get_sensor_suid(psalt, cal_sensor);

      if (cal_suid == nullptr) {
        return common_exit(SALT_RC_FAILED, psalt);
      }
    }
  }

  // run the test
  saltrc = run(psalt, psensor, cal_suid, cal_sensor);

  return common_exit(saltrc, psalt);
}

static int common_exit (salt_rc saltrc, see_salt *psalt) {
  string passfail("PASS");
  int retcode = 0;
  if (saltrc != SALT_RC_SUCCESS) {
    passfail = "FAIL";
    retcode = 4;
  }
  write_result_description(passfail);
  oss << passfail << " " << cmdline;
  writeline(oss);

  cout << passfail << " " << cmdline << endl;

  if (psalt && config.has_flush_log) {       // SENSORTEST-1342
    for (int i = 0; i < config.flush_log; i++) {
      trigger_diag_sensor_log(psalt, SEE_LOG_TYPE_ISLAND_LOG, 0);
    }
  }

  if (psalt != nullptr) {       // SENSORTEST-1323/1491
    oss << "begin delete psalt";
    writeline(oss);
    delete psalt;
    oss << "end delete psalt";
    writeline(oss);
  }

  if (wake_lock != nullptr) {       // SENSORTEST-1684
    char *wl_file = wake_lock->put_n_locks(1);
    oss << "release wakelock ";
    if (wl_file != nullptr) {
      oss << wl_file;
    }
    writeline(oss);
  }

  return retcode;
}

/**
 * @brief choose_target sensor based on matching command line specifics for
 *        type, name, rigid_body, stream_type and/or on_change, and hw_id
 * @param psalt[i]
 * @param sens_uids[i] - vector of suids for -sensor=<datatype>
 * @return nullptr or sensor matching command line spec
 */
sensor* choose_target_sensor (see_salt *psalt, vector <sens_uid *>& sens_uids) {
  if (psalt == nullptr) {
    return nullptr;
  }
  vector <sensor *> eligible;   // SENSORTEST-1595

  for (size_t i = 0; i < sens_uids.size(); i++) {
    sens_uid *candidate_suid = sens_uids[i];
    sensor *psensor = psalt->get_sensor(candidate_suid);
    if (psensor != nullptr) {
      if (config.has_name) {
        if (psensor->get_name() != config.name) {
          continue;
        }
      }
      if (config.has_rigid_body) {
        if (!psensor->has_rigid_bodies()) {
          continue;
        }

        vector <string> rigid_bodies;
        psensor->get_rigid_bodies(rigid_bodies);
        bool got_valid_rb = false;
        for (auto i = 0; i < rigid_bodies.size(); i++) {
          if (config.rigid_body == rigid_bodies[i]) {
            got_valid_rb = true;
            break;
          }
        }
        if (!got_valid_rb) {
          continue;
        }
      }
      if (config.has_on_change) {
        if (!psensor->has_stream_type()) {
          continue;
        }
        if (config.on_change != psensor->is_stream_type_on_change()) {
          continue;
        }
      }
      if (config.has_stream_type) {
        if (!psensor->has_stream_type()) {
          continue;
        }
        if (config.stream_type == 0) {
          if (!psensor->is_stream_type_streaming()) {
            continue;
          }
        }
        else if (config.stream_type == 1) {
          if (!psensor->is_stream_type_on_change()) {
            continue;
          }
        }
        else if (config.stream_type == 2) {
          if (!psensor->is_stream_type_single_output()) {
            continue;
          }
        }
      }
      if (config.has_hw_id) {
        if (!psensor->has_hw_id()) {
          continue;
        }
        if (psensor->get_hw_id() != config.hw_id) {
          continue;
        }
      }
      eligible.push_back(psensor);                   // SENSORTEST-1595
    }
  }

  for (size_t i = 0; i < eligible.size(); i++) {
    sensor *psensor = eligible[i];

    vector <string> rigid_bodies;
    if (psensor->has_rigid_bodies()) {
      psensor->get_rigid_bodies(rigid_bodies);
    }

    // SENSORTEST-1088 default to rigid_body == display
    // SENSORTEST-1119 when rigid_body and hw_id are omitted
    // SENSORTEST-1496 rigid body default when name supplied
    // SENSORTEST-1595 but skip rigid body default when only 1 match
    if (eligible.size() > 1
        && config.has_rigid_body == false && config.has_hw_id == false) {
      if (psensor->has_rigid_bodies()) {
        bool got_display_rb = false;
        for (auto i = 0; i < rigid_bodies.size(); i++) {
          if (rigid_bodies[i] == "display") {
            got_display_rb = true;
            break;
          }
        }
        if (!got_display_rb) {
          continue;
        }
      }
    }

    if (!config.has_rigid_body) {               // setup dual gyro_cals or mag_cals
      if (psensor->has_rigid_bodies()) {
        string rigid_body = rigid_bodies[0];
        strlcpy(config.rigid_body, rigid_body.c_str(),
                sizeof(config.rigid_body));
      }
    }
    return psensor;
  }
  return nullptr;
}

/* returns true - successful, false - unable to resolve */
bool resolve_symbolic_sample_rate (sensor *psensor) {
  if (psensor == nullptr) {
    return false;
  }

  if (psensor->has_rates()) {
    string lit_rate("min_odr");
    vector <float> rates;
    psensor->get_rates(rates);
    if (config.symbolic_sample_rate == SYMBOLIC_MIN_ODR) {
      config.sample_rate = rates[0];                   // min odr
    }
    else {
      lit_rate = "max_odr";
      config.sample_rate = rates[rates.size() - 1];                   // max odr
    }
    oss << "using " << lit_rate << " sample_rate: "
      << config.sample_rate << " hz";
    writeline(oss);
    return true;
  }

  oss << "symbolic sample_rate supplied, but sensor has no rates";
  writeline(oss);
  return false;
}

void display_target_sensor (sensor *psensor) {
  ostringstream ss;

  ss << "-sensor=" << config.data_type << " ";
  if (config.has_name) {
    ss << "-name=" << config.name << " ";
  }
  if (config.has_rigid_body) {
    ss << "-rigid_body=" << config.rigid_body << " ";
  }
  else if (psensor != nullptr && psensor->has_rigid_bodies()) {
    ss << "-rigid_body=[";
    vector <string> rigid_bodies;
    psensor->get_rigid_bodies(rigid_bodies);
    for (auto i = 0; i < rigid_bodies.size(); i++) {
      ss << rigid_bodies[i].c_str();
      if (i != rigid_bodies.size() - 1) {
        ss << ", ";
      }
    }
    ss << "] ";
  }
  if (config.has_on_change) {
    ss << "-on_change=" << to_string(config.on_change) << " ";
  }
  if (config.has_stream_type) {
    ss << "-stream_type=" << to_string(config.stream_type) << " ";
  }
  if (config.has_hw_id) {
    ss << "-hw_id=" << to_string(config.hw_id) << " ";
  }
  if (psensor != nullptr) {
    sens_uid *suid = psensor->get_suid();
    ss << " found" << " suid = 0x" << hex << setfill('0')
      << setw(16) << suid->high << setw(16) << suid->low;
    writeline(ss);
  }
  else {
    oss << "FAIL " << ss.str() << " not found";
    writeline(oss);
  }
}
/**
 * @brief run the test
 * @param psalt
 * @param psensor
 * @param calibrated_suid
 * @param cal_sensor
 *
 * @return salt_rc
 */
salt_rc run (see_salt *psalt, sensor *psensor,
             sens_uid *cal_suid, string cal_sensor) {
  if (psalt == nullptr || psensor == nullptr) {
    return SALT_RC_FAILED;
  }

  salt_rc rc = SALT_RC_SUCCESS;
  int iteration = 0;
  sens_uid *suid = psensor->get_suid();
  if (suid == nullptr) {
    return SALT_RC_FAILED;
  }
  string target_sensor = string(config.data_type);

  while (rc == SALT_RC_SUCCESS
         && iteration < config.times) {
    iteration++;
    oss << "begin iteration: " << to_string(iteration);
    writeline(oss);

    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t cookie = (uint64_t)tv.tv_usec;
    std::string cal_sensor_suffix = "_cal";

    // conditionally trigger log memory.
    // hope used memory here == used memory at end
    if (config.has_memory_log && config.memory_log) {
      rc = trigger_diag_sensor_log(psalt, SEE_LOG_TYPE_MEMORY_USAGE_LOG, 0);
    }

    if (config.has_get_active) {             // SENSORTEST-1554
      diag_sensor_get_active(psalt);
      do_sleep(config.get_active_value);                   // 2 seconds before enable
    }

    // conditionally request calibration
    if (rc == SALT_RC_SUCCESS && cal_suid != NULL) {
      rc = on_change_sensor(psalt, cal_suid, cal_sensor);
    }

    // activate appropriate sensor as configured
    if (rc == SALT_RC_SUCCESS) {
      if ("distance_bound" == psensor->get_type()) {
        rc = distance_bound_sensor(psalt, suid);
      }
      else if ("basic_gestures" == psensor->get_type()) {
        rc = basic_gestures_sensor(psalt, suid);
      }
      else if ("multishake" == psensor->get_type()) {
        rc = multishake_sensor(psalt, suid);
      }
      else if ("psmd" == psensor->get_type()) {
        rc = psmd_sensor(psalt, suid);
      }
      else if ("delta_angle" == psensor->get_type()) {
        rc = delta_angle_config(psalt, suid);
      }
      else if ("on_device_personalization" == psensor->get_type()) {
        rc = odp_sensor(psalt, suid);
      }
      else if (config.cal_reset_req &&
               (psensor->get_type().size() > cal_sensor_suffix.size()) &&
               (psensor->get_type().compare
                (psensor->get_type().size() - cal_sensor_suffix.size(), cal_sensor_suffix.size(),
                 cal_sensor_suffix) == 0))
      {
        rc = send_sns_cal_reset_req(psalt, suid);
      }
      else if (config.has_force && config.force != "gated") {
        if (config.force == "streaming") {
          see_msg_id_e msg_id = MSGID_STD_SENSOR_CONFIG;
          rc = stream_sensor(psalt, msg_id, suid, target_sensor);
        }
        else if (config.force == "on_change") {
          rc = on_change_sensor(psalt, suid, target_sensor);
        }
        else {
          rc = SALT_RC_FAILED;
          oss << "unsupported '-force=" << config.force << "' argument";
          writeline(oss);
        }
      }
      else if (psensor->has_stream_type()
               && (psensor->is_stream_type_on_change()
                   || psensor->is_stream_type_single_output())) {
        rc = on_change_sensor(psalt, suid, target_sensor);
      }
      else {
        see_msg_id_e msg_id = MSGID_STD_SENSOR_CONFIG;
        rc = stream_sensor(psalt, msg_id, suid, target_sensor);
      }
    }

    if (rc == SALT_RC_SUCCESS) {
      try {
        if (config.has_island_log && config.island_log) {
          trigger_diag_sensor_log(psalt, SEE_LOG_TYPE_ISLAND_LOG, cookie);
        }

        if (config.has_get_active) {                         // SENSORTEST-1554
          do_sleep(config.get_active_value);                               // 2 seconds after enable
          diag_sensor_get_active(psalt);
        }

        float duration = config.duration;
        if (config.has_flush_request_interval) {
          while (1) {
            if (duration > config.flush_request_interval) {
              do_sleep(config.flush_request_interval);                                           // seconds
              rc = flush_sensor(psalt, suid, target_sensor);
              duration -= config.flush_request_interval;
              continue;
            }
            do_sleep(duration);
            break;
          }
        }
        else if (config.rearm
                 && rearm.target_sensor == target_sensor) {
          rearm.suid = suid;
          monitor_and_rearm(psalt, duration);                               //SNS-660
        }
        else {
          do_sleep(duration);
        }

        if (config.rearm
            && rearm.target_sensor == target_sensor) {
          config.rearm = false;                               // fix race condition exception
          rearm.suid = (sens_uid *)0;
        }

        if (config.has_get_active) {                         // SENSORTEST-1554
          diag_sensor_get_active(psalt);
          do_sleep(config.get_active_value);                               // 2 seconds before disable
        }

        if (config.has_island_log && config.island_log) {
          trigger_diag_sensor_log(psalt, SEE_LOG_TYPE_ISLAND_LOG, cookie);
        }

        rc = disable_sensor(psalt, suid, target_sensor);                         // stop sensor

      }
      catch (exception& e) {
        oss << "caught execption " << e.what();
        writeline(oss);
        rc = SALT_RC_FAILED;
      }
    }

    // conditionally disable calibration
    if (rc == SALT_RC_SUCCESS && cal_suid != NULL) {
      rc = disable_sensor(psalt, cal_suid, cal_sensor);
    }

    if (config.has_get_active) {             // SENSORTEST-1554
      do_sleep(config.get_active_value);                   // 2 seconds after disable
      diag_sensor_get_active(psalt);
    }

    if (rc == SALT_RC_SUCCESS) {
      if (config.has_memory_log && config.memory_log) {
        rc = trigger_diag_sensor_log(psalt, SEE_LOG_TYPE_MEMORY_USAGE_LOG, 0);
      }
    }

    // reset the connection, at the end of every iteration
    if (config.times > 1) {
      client_id = -1;

      // sleep for times_delay before next iteration
      if (iteration != config.times)
        do_sleep(config.times_delay);
    }

    oss << "end iteration: " << to_string(iteration);
    writeline(oss);
  }

  // SENSORTEST-1264
  do_sleep(1);         // hope to get disable/trigger packets logged

  return rc;
}

static void append_log_time_stamp (ostringstream& outss) {
  std::chrono::system_clock::time_point start_time;
  const auto end_time = std::chrono::system_clock::now();
  auto delta_time = end_time - start_time;
  const auto hrs = std::chrono::duration_cast <std::chrono::hours>
                   (delta_time);
  delta_time -= hrs;
  const auto mts = std::chrono::duration_cast <std::chrono::minutes>
                   (delta_time);
  delta_time -= mts;
  const auto secs = std::chrono::duration_cast <std::chrono::seconds>
                    (delta_time);
  delta_time -= secs;
  const auto ms = std::chrono::duration_cast <std::chrono::milliseconds>
                  (delta_time);
  outss  << std::setfill('0')
    << std::setw(2) << hrs.count() % 24 << ":"
    << std::setw(2) << mts.count() << ':'
    << std::setw(2) << secs.count() << '.'
    << std::setw(3) << ms.count() << ' '
    << std::setfill(' ') << std::left;
}

/**
* primary stdout writes timestamped line
*
* @param outss
*/
static void writeline (ostringstream& outss) {
  ostringstream tss;
  append_log_time_stamp(tss);
  cout << tss.str() << outss.str() << endl;
  outss.str("");       // reset outss to ""
}

/*
** append result_description to results_fn
*/
void write_result_description( string passfail)
{
    static string results_fn = LOG_PATH + string("sensor_test_result");

  ostringstream tss;
  append_log_time_stamp(tss);
  string timestamp = tss.str();

  int fd = open(results_fn.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd >= 0) {
    if (flock(fd, LOCK_EX) == 0) {
      string result = passfail + ": " + timestamp + cmdline + newline;
      ssize_t len = write(fd, result.c_str(), result.length());
      if (len != (ssize_t)result.length()) {
        cout << " FAIL write_result_description failed." << endl;
      }
    }
    else {
      cout << " FAIL flock failed." << endl;
    }
    close(fd);
  }
  else {
    cout << " FAIL open failed for " << results_fn << endl;
  }
}
