/* ===================================================================
** Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
** All rights reserved.
** Confidential and Proprietary - Qualcomm Technologies, Inc.
**
** FILE: see_selftest.cpp
** DESC: physical sensor self test
** ================================================================ */

#include <iostream>
#include <iomanip> // std::setprecision
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/file.h>
#include "see_salt.h"

using namespace std;

static int common_exit(int retcode, see_salt* psalt);
static void append_suid(ostringstream& outss, sens_uid* suid);
static void append_log_time_stamp(ostringstream& outss);
static void writeline(ostringstream& outss);
static void write_result_description(string passfail);
void event_cb(string sensor_event, unsigned int sensor_handle,
              unsigned int client_connect_id);
static string trim(const string& line);

int client_id = -1;

static string newline("\n");
static string cmdline("");
static string comma(",");
static string closing_brace("}");

static ostringstream oss;

#define SUCCESSFUL 0
#define FAILURE    4

struct app_config {
  bool  is_valid         = false;
  char  data_type[32];           // type: accel, gyro, mag, ...

  bool  has_name         = false;
  char  name[32];

  bool  has_on_change    = false;
  bool  on_change;

  bool  has_rigid_body   = false;
  char  rigid_body[32];

  bool  has_hw_id        = false;
  int   hw_id;

  bool  has_testtype     = false;
  int   testtype         = SELF_TEST_TYPE_FACTORY;

  bool  has_delay        = false;
  float delay;

  float duration         = 5.0;  // seconds

  bool  has_stream_type  = false;
  int   stream_type;

  bool  display_events   = false;

  app_config() {
  }
};

static app_config config;

salt_rc send_request(see_salt* psalt, see_client_request_message& request) {
  if (psalt == nullptr) {
    oss << "FAIL null psalt ptr";
    writeline(oss);
    return SALT_RC_FAILED;
  }

  salt_rc rc = SALT_RC_FAILED;
  see_client_request_info request_info;

  if (client_id == -1) {
    request_info.req_type = SEE_CREATE_CLIENT;

    see_register_cb cb;

      if (config.display_events) {
          cb.sensor_event_cb_func_inst = (sensor_event_cb_func)event_cb;
      }
      else {
          cb.sensor_event_cb_func_inst = NULL;
      }

    request_info.cb_ptr = cb;

    client_id = psalt->send_request(request, request_info);

    if (client_id >= 0) {
      rc = SALT_RC_SUCCESS;
    } else {
      rc = SALT_RC_FAILED;
    }
  } else {
    request_info.req_type = SEE_USE_CLIENT_CONNECT_ID;
    request_info.client_connect_id = client_id;

    int retval = psalt->send_request(request, request_info);

    if (retval == client_id) {
      rc = SALT_RC_SUCCESS;
    } else {
      rc = SALT_RC_FAILED;
    }
  }

  return rc;
}

void self_test_sensor(see_salt* psalt,
                      sens_uid* suid,
                      see_self_test_type_e testtype) {
  see_self_test_config payload(testtype);
  oss << "self_test_sensor() testtype " << to_string(testtype)
      << " - " << payload.get_test_type_symbolic(testtype);
  writeline(oss);

  see_std_request std_request;
  std_request.set_payload(&payload);
  see_client_request_message request(suid,
                                     MSGID_SELF_TEST_CONFIG,
                                     &std_request);
  salt_rc rc = send_request(psalt, request);
  oss << "self_test_sensor() rc " << to_string(rc);
  writeline(oss);
}

void disable_sensor(see_salt* psalt, sens_uid* suid, string target) {
  oss << "disable_sensor( " << target << ")";
  writeline(oss);
  see_std_request std_request;
  see_client_request_message request(suid,
                                     MSGID_DISABLE_REQ,
                                     &std_request);
  salt_rc rc = send_request(psalt, request);
  oss << "disable_sensor() rc " << to_string(rc);
  writeline(oss);
}

void usage(void) {
  cout << "usage: see_selftest"
      " -sensor=<sensor_type>"
      " [-name=<name>]"
      " [-on_change=<0 | 1>] [-stream_type=<stream_type>\n"
      " [-rigid_body=<display | keyboard | external>]"
      " [-hw_id=<number>]\n"
      " -testtype=<number | sw | hw | factory | com>\n"
      " [-delay=<seconds>]"
      " -duration=<seconds> [-display_events= <1|0>] [-help]\n"
      " where: <sensor_type> :: accel | gyro | mag | ...\n"
      "        <stream_type> :: 0 streaming | 1 on_change | 2 single_output\n";
}

/**
 * @brief parse command line argument of the form keyword=value
 * @param parg[i] - command line argument
 * @param key[io] - gets string to left of '='
 * @param value[io] - sets string to right of '='
 */
bool get_key_value(char* parg, string& key, string& value) {
  char* pkey = parg;

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

void parse_testtype(string value) {
  string lit_sw("sw");
  string lit_hw("hw");
  string lit_factory("factory");
  string lit_com("com");

  config.has_testtype = true;
  if (value == lit_sw) {
    config.testtype = SELF_TEST_TYPE_SW;
  } else if (value == lit_hw) {
    config.testtype = SELF_TEST_TYPE_HW;
  } else if (value == lit_factory) {
    config.testtype = SELF_TEST_TYPE_FACTORY;
  } else if (value == lit_com) {
    config.testtype = SELF_TEST_TYPE_COM;
  } else {
    config.testtype = atoi(value.c_str());
  }
}

/**
  * Removes leading and trailing whitespaces in a string
  *
  * @param line
  * @return string
  */
string trim(const string& line) {
  const char* whitespaces = " \t\v\r\n";
  size_t start = line.find_first_not_of(whitespaces);
  size_t end = line.find_last_not_of(whitespaces);
  return start == end ? string("") : line.substr(start, end - start + 1);
}

/**
 * Extracts a substring "key" from the string "payload"
 *
 * @param payload
 * @param key
 *
 * @return string
 */
string extract_substr(string payload, string key, string limit) {
  size_t start = 0, end = 0;
  string value = "";
  if (payload.find(key) != string::npos) {
    start = payload.find(key);
    end = payload.find(limit, start);

    if (end == string::npos) {
      return value;
    }

    value = payload.substr(start, end - start);
    value = trim(value); // Trim the string
    value.erase(remove(value.begin(), value.end(), '\"'), value.end()); // Dequote the string

    if (value.find(comma) != string::npos) {
      value.erase(value.find(comma)); // remove the comma (,)
    }
  }
  return value;
}

int parse_arguments(int argc, char* argv[]) {
  string sensor_eq("-sensor");
  string name_eq("-name");
  string on_change_eq("-on_change");
  string rigid_body_eq("-rigid_body");
  string hw_id_eq("-hw_id");
  string testtype_eq("-testtype");
  string delay_eq("-delay");
  string duration_eq("-duration");
  string stream_type_eq("-stream_type");
  string display_events_eq("-display_events");

  config.is_valid = true;
  for (int i = 1; i < argc; i++) {
    if ((0 == strcmp(argv[i], "-h"))
        || (0 == strcmp(argv[i], "-help"))) {
      usage();
      exit(SUCCESSFUL);
    }

    string key;
    string value;
    if (get_key_value(argv[i], key, value)) {
      if (sensor_eq == key) {
        strlcpy(config.data_type,
                value.c_str(),
                sizeof(config.data_type));
      } else if (name_eq == key) {
        config.has_name = true;
        strlcpy(config.name,
                value.c_str(),
                sizeof(config.name));
      } else if (on_change_eq == key) {
        config.has_on_change = true;
        config.on_change = atoi(value.c_str());
      } else if (rigid_body_eq == key) {
        config.has_rigid_body = true;
        strlcpy(config.rigid_body, value.c_str(),
                sizeof(config.rigid_body));
      } else if (hw_id_eq == key) {
        config.has_hw_id = true;
        config.hw_id = atoi(value.c_str());
      } else if (testtype_eq == key) {
        parse_testtype(value);
      } else if (delay_eq == key) {
        config.has_delay = true;
        config.delay = atof(value.c_str());
      } else if (duration_eq == key) {
        config.duration = atof(value.c_str());
      } else if (stream_type_eq == key) {
        config.has_stream_type = true;
        config.stream_type = atoi(value.c_str());
        if (config.stream_type < 0 || config.stream_type > 2) {
          cout << "FAIL invalid stream_type value " << value.c_str() << endl;
          config.is_valid = false;
        }
      } else if (display_events_eq == key) {
        config.display_events = std::atoi(value.c_str());
      } else {
        config.is_valid = false;
        cout << "FAIL unrecognized arg " << argv[i] << endl;
      }
    } else {
      config.is_valid = false;
      cout << "FAIL unrecognized arg " << argv[i] << endl;
    }
  }
  if (!config.is_valid) {
    usage();
    return FAILURE;
  }
  return SUCCESSFUL;
}

int main(int argc, char* argv[]) {
  cout << "see_selftest version 1.91" << endl;

  // save and display commandline
  for (int i = 0; i < argc; i++) {
    oss << argv[i] << " ";
  }
  cmdline = oss.str();
  writeline(oss);

  if (parse_arguments(argc, argv))
    return common_exit(FAILURE, nullptr);

  if (config.data_type[0] == '\0') {
    oss << "missing -sensor=<sensor_type>";
    writeline(oss);
    return common_exit(FAILURE, nullptr);
  }

  if (config.has_delay) {
    oss << "delay startup " << config.delay << " seconds";
    writeline(oss);
    usleep((int)config.delay * 1000000);     // implement startup delay
  }

  see_salt* psalt = see_salt::get_instance();
  if (psalt == nullptr) {
    oss << "FAIL see_salt::get_instance() failed";
    writeline(oss);
    return common_exit(FAILURE, nullptr);
  }

  psalt->begin(); // populate sensor attributes

  /* get vector of all suids for target sensor */
  vector<sens_uid*> sens_uids;
  string target_sensor = string(config.data_type);
  oss << "lookup: " << target_sensor;
  writeline(oss);
  psalt->get_sensors(target_sensor, sens_uids);
  for (size_t i = 0; i < sens_uids.size(); i++) {
    sens_uid* suid = sens_uids[i];
    oss << "found " << target_sensor << " ";
    append_suid(oss, suid);
    writeline(oss);
  }

  /* choose target_suid based on type, name, on_change, rigid_body, hw_id */
  sens_uid* target_suid = NULL;
  sensor* psensor = NULL;

  for (size_t i = 0; i < sens_uids.size(); i++) {
    sens_uid* candidate_suid = sens_uids[i];
    psensor = psalt->get_sensor(candidate_suid);
    if (config.has_name) {
      string name = psensor->get_name();
      if (0 != strcmp(config.name, name.c_str())) {
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
      } else if (config.stream_type == 1) {
        if (!psensor->is_stream_type_on_change()) {
          continue;
        }
      } else if (config.stream_type == 2) {
        if (!psensor->is_stream_type_single_output()) {
          continue;
        }
      }
    }
    if (config.has_rigid_body) {
      vector<string> rigid_bodies;
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
    if (config.has_hw_id && psensor->has_hw_id()) {
      if (psensor->get_hw_id() != config.hw_id) {
        continue;
      }
    }
    target_suid = candidate_suid;        // found target
    break;
  }

  stringstream ss;
  ss << "-sensor=" << config.data_type << " ";
  if (config.has_name) {
    ss << "-name=" << config.name << " ";
  }
  if (config.has_on_change) {
    ss << "-on_change=" << to_string(config.on_change) << " ";
  }
  if (config.has_stream_type) {
    ss << "-stream_type=" << to_string(config.stream_type) << " ";
  }
  if (config.has_rigid_body) {
    ss << "-rigid_body=" << config.rigid_body << " ";
  }
  if (config.has_hw_id) {
    ss << "-hw_id=" << to_string(config.hw_id) << " ";
  }
  if (target_suid == NULL) {
    oss << "FAIL " << ss.str() << " not found";
    writeline(oss);
    return common_exit(FAILURE, psalt);
  }
  oss << ss.str() << " found ";
  append_suid(oss, target_suid);
  writeline(oss);

  if (!config.has_testtype) {
    config.testtype = SELF_TEST_TYPE_FACTORY;
  }

  self_test_sensor(psalt, target_suid,
                   (see_self_test_type_e)config.testtype);
  oss << "sleep " << config.duration << " seconds";
  writeline(oss);
  psalt->sleep(config.duration);   // seconds

  disable_sensor(psalt, target_suid, target_sensor);   // SENSORTEST-1414

  return common_exit(SUCCESSFUL, psalt);
}

static int common_exit(int retcode, see_salt* psalt) {
  string passfail = (retcode == SUCCESSFUL) ? "PASS" : "FAIL";

  write_result_description(passfail);
  oss << passfail << " " << cmdline;
  writeline(oss);

  if (psalt != nullptr) {
    delete psalt;
  }

  cout << passfail << " " << cmdline << endl;
  return retcode;
}

static void append_suid(ostringstream& outss, sens_uid* suid) {
  outss << "suid = 0x" << hex << setfill('0')
      << setw(16) << suid->high
      << setw(16) << suid->low
      << dec;
}

static void append_log_time_stamp(ostringstream& outss) {
  std::chrono::system_clock::time_point start_time;
  const auto end_time = std::chrono::system_clock::now();
  auto delta_time = end_time - start_time;
  const auto hrs = std::chrono::duration_cast<std::chrono::hours>
      (delta_time);
  delta_time -= hrs;
  const auto mts = std::chrono::duration_cast<std::chrono::minutes>
      (delta_time);
  delta_time -= mts;
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>
      (delta_time);
  delta_time -= secs;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>
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
static void writeline(ostringstream& outss) {
  ostringstream tss;
  append_log_time_stamp(tss);
  cout << tss.str() << outss.str() << endl;
  outss.str("");   // reset outss to ""
}

/**
 ** append result_description to output file
 ** supports multiple concurrent ssc_drva_tests
*/
static void write_result_description(string passfail) {
    static string results_fn = LOG_PATH + string("sensor_test_result");

  ostringstream tss;
  append_log_time_stamp(tss);
  string timestamp = tss.str();

  int fd = open(results_fn.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd >= 0) {
    if (flock(fd, LOCK_EX) == 0) {
      string result = passfail + ": " + timestamp + cmdline + newline;
      ssize_t len = write(fd, result.c_str(), result.length());
      if (len != result.length()) {
        cout << " FAIL write_result_description failed." << endl;
      }
    } else {
      cout << " FAIL flock failed." << endl;
    }
    close(fd);
  } else {
    cout << " FAIL open failed for " << results_fn << endl;
  }
}

/**
 * event_cb function as registered with usta.
 * 1. Display factory test result, test type and test data (if any) to console
 *    stdout
 * 2. Display the see_selftest events (as is) to the command window when
 *    "-display_events" argument is set
 *
 * @param sensor_event
 * @param sensor_handle
 * @param client_connect_id
 */
void event_cb(string sensor_event, unsigned int sensor_handle,
              unsigned int client_connect_id) {

  if (config.display_events) {
    cout << sensor_event << endl;
  }

  string str_factory_test_msgid = "\"msg_id\" : 1026,";

  if (sensor_event.find(str_factory_test_msgid) != string::npos) {
    oss << "Received factory test event msg_id 1026";
    writeline(oss);

    string str_payload = "\"payload\" : ";
    string str_factory_test_res = "\"test_passed\" : ";
    string str_factory_test_type = "\"test_type\" : ";
    string str_factory_test_data = "\"test_data\" : ";

    size_t start = 0, end = 0;
    string payload = "";

    start = sensor_event.find(str_payload);
    if (start != string::npos) {
      end = sensor_event.find(closing_brace, start);
      if (end != string::npos) {
        string payload = sensor_event.substr(start, end - start + 1);

        // Extract the "test_passed" field from payload
        string test_res = extract_substr(payload, str_factory_test_res, comma);
        if (test_res != "") {
          oss << test_res;
          writeline(oss);
        } else {
          oss << "FAIL: \"test_passed\" field is empty.";
          writeline(oss);
        }

        // Extract the "test_type" field from payload
        string test_type = extract_substr(payload, str_factory_test_type, comma);
        if (test_type != "") {
          oss << test_type;
          writeline(oss);
        } else {
          oss << "FAIL: \"test_type\" field is empty.";
          writeline(oss);
        }

        // Extract the "test_data" field from payload
        string test_data = extract_substr(payload, str_factory_test_data, comma);
        if (test_data != "") {
          oss << test_data;
          writeline(oss);
        } else {
          oss << "\"test_data\" field is empty.";
          writeline(oss);
        }
      }
    } else {
      oss << "FAIL: \"payload\" field not found in the factory test event msg_id 1026";
      writeline(oss);
    }
  }
}
