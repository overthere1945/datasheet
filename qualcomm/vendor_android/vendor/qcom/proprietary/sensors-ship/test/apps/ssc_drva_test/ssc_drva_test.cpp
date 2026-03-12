/* ===================================================================
** Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
** All rights reserved.
** Confidential and Proprietary - Qualcomm Technologies, Inc.
**
** FILE: ssc_drva_test.cpp
** DESC: command line application for SEE driver acceptance testing.
**  Usage should be in the format of "-key=value" as, -sensor=<datatype>
**  -sample_rate=<hz> -duration=<seconds> -batch_period=<seconds> -testcase=<name>
**    where <datatype>  :: accel | gyro | mag | ... | suid
**          <hz>        :: int or float
**          <seconds>   :: int or float
** ================================================================ */
#include <chrono>
#include <condition_variable>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#define strlcat g_strlcat
#endif

#include "sns_client.pb.h"
#include "sns_diag.pb.h"
#include "sns_diag_sensor.pb.h"
#include "sns_da_test.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_suid.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "sensors_log.h"
#include "sensor_diag_log.h" // SENSORTEST-1218
#include "sensors_timeutil.h"
#include "SessionFactory.h"
#include "suid_lookup.h"
#include <util_wakelock.h>

#ifdef __ANDROID_API__
#define LOG_PATH "/data/"
#else
#define LOG_PATH "/etc/"
#endif

using namespace google::protobuf::io;
using namespace std;

using suid = com::quic::sensinghub::suid;

class test_msg_file_logger;

static void write_result_description();

char *my_exe_name = (char *)"";
char *newline = (char *)"\n";

const char lit_pass[] = "PASS";
const char lit_fail[] = "FAIL";
const char lit_recalc_timeout[] = "RECALC_TIMEOUT";
float recalc_duration = 0;    // set by event "RECALC_TIMEOUT number"

const int MAX_RESULT_DESCRIPTION_LEN = 128;
char result_description[ MAX_RESULT_DESCRIPTION_LEN] = "";

const int MAX_CMD_LINE_ARGS_LEN = 2048;
char args[ MAX_CMD_LINE_ARGS_LEN];

suid target_da_test_suid = {0, 0};
suid target_sensor_suid = {0, 0};
bool found_da_test_sensor = false;
bool found_target_sensor = false;
bool attrib_lookup_complete = false;
const string& whitespace = " \t";

int hub_id = -1;
int unique_id = 0;
util_wakelock *wake_lock = nullptr; // SENSORTEST-1684

ostringstream oss;
string cmdline;

sensors_diag_log *sensors_diag_log_obj = NULL; // SENSORTEST-1218
char logging_module_name[] = "ssc_drva_test";


std::unique_ptr<test_msg_file_logger> msg_file_logger;

/**
 * @brief Class for logging client API messages i.e. events, requests, and responses directly to file.
 */
class test_msg_file_logger
{
public:
    /**
     * @brief Constructor
     *
     * @param filename full path name for the log file
     */
    explicit test_msg_file_logger(const string& filename);

    test_msg_file_logger() = delete;

    // delete copy constructors and assignment because fstream is not copyable.
    test_msg_file_logger(const test_msg_file_logger& that) = delete;
    test_msg_file_logger& operator=(const test_msg_file_logger& that) = delete;

    /**
     * @brief Log client API message to file.
     *
     * @param src_sensor_data_type data type of the sensor
     * @param log_type type of log packet i.e. request or event or response.
     * @param msg
     */
    void log(const string& src_sensor_data_type, sensors_diag_logid log_type, void* msg);
private:
    void write_test_events(uint16_t packet_type, const char* pb_encoded_payload, uint16_t payload_len);
    static uint64_t convert_to_packet_timestamp(std::chrono::system_clock::time_point timestamp);

    static const uint16_t LOG_SNS_HLOS_REQUEST_C = 0x19D9;
    static const uint16_t LOG_SNS_HLOS_EVENT_C = 0x19DA;
    static const uint16_t LOG_SNS_HLOS_RESPONSE_C = 0x19FF;

    static const uint64_t CLIENT_MANAGER_SUID_LOW = 0x1628E8697D485ECD;
    static const uint64_t CLIENT_MANAGER_SUID_HIGH = 0xB44E6DDA82EF9B2B;

    fstream msg_file;
};

/**
 * @brief Class for discovering the sensor_uid for a datatype
 */
class locate
{
public:
    /**
     * @brief lookup the first sensor_uid for a given datatype
     *
     * @param datatype
     * @param hub_id
     */
    void lookup( const string& datatype, int hub_id);

    vector<suid> suids;
    int count() { return suids.size(); }
private:
    void on_suids_available(const string& datatype,
                            const std::vector<suid>& suids);
    string _datatype = "";

    bool _lookup_done = false;
    std::mutex _m;
    std::condition_variable _cv;
};
void locate::on_suids_available(const string& datatype,
                                const vector<suid>& suids)
{
    sns_logi("%u sensor(s) available for datatype %s",
             (unsigned int) suids.size(), datatype.c_str());

    _datatype = datatype;
    for ( size_t i = 0; i < suids.size(); i++) {
        suid sensor_uid = {0, 0};
        sensor_uid.low = suids[i].low;
        sensor_uid.high = suids[i].high;

        sns_logi("datatype %s suid = [%" PRIx64 " %" PRIx64 "]",
                 datatype.c_str(), sensor_uid.high, sensor_uid.low);

        locate::suids.push_back(sensor_uid);
    }

    unique_lock<mutex> lk(_m);
    _lookup_done = true;
    _cv.notify_one();
}

void locate::lookup(const string& datatype, int hub_id)
{
    if ( _lookup_done) {
        return;
    }

    suid_lookup lookup(
        [this](const auto& datatype, const vector<suid>& suids)
        {
            this->on_suids_available(datatype, suids);
        }, hub_id);

    lookup.request_suid( datatype);

    sns_logi("waiting for suid lookup");
    // TODO remove timeout once suid_lookup request_suid() is fixed
    auto now = std::chrono::system_clock::now();
    auto interval = 1000ms * 5; // 5 second timeout
    auto then = now + interval;

    unique_lock<mutex> lk(_m);
    while ( !_lookup_done) {
        if ( _cv.wait_until(lk, then) == std::cv_status::timeout) {
            cout << unique_id << " suid not found for " << datatype << endl;
            break;
        }
    }

    sns_logi( "%s", "end suid lookup");
}

struct app_config {
      bool  is_valid = true;
      char  data_type[64]; // type: accel, gyro, mag, ...

      bool  has_name = false;
      char  name[64];

      bool  has_da_test_name = false;
      char  da_test_name[32];

      bool  has_rigid_body = false;
      char  rigid_body[32];

      bool  has_hw_id = false;
      int   hw_id;

      bool  has_stream_type = false;
      int   stream_type;

      double duration = 0.0; // seconds

      double da_test_batch_period = 0.0; // seconds

      double rumifact = 1.0; // for rumi: timeout = duration * rumifact

      bool has_prevent_apps_suspend = false;

      bool  debug;

      app_config () {}
};

static app_config config;

/**
 * @brief Class for sending a sns_da_test_req message to the
 *        da_test sensor
 */
class da_test_runner
{
public:
    da_test_runner(int hub_id)
    {
        _hub_id = hub_id;
    }

    /**
     * @brief create the sns_client_request_msg carrying the
     * sns_da_test_req payload
     *
     * @param cmd_line_args character array of cmd line arguments
     * @param sensor_uid for the da_test sensor
     * @param msg_id for the client request message
     * @return sns_client_request_msg
     */
    sns_client_request_msg create_da_test_request(const char *cmd_line_args,
                                                  suid sensor_uid,
                                                  int32_t msg_id);
    /**
     * @brief run the da_test sensor
     *
     * @param cmd_line_args character array of cmd line arguments
     * @param test duration in seconds
     * @param sensor_uid for the da_test sensor
     * @param data_type for the da_test sensor
     * @param msg_id for the client request message
     * @param rumifact - timeout = cushion + duration * rumifact
     */
    void runner(const char* cmd_line_args,
                double duration,
                suid sensor_uid,
                string& data_type,
                int32_t msg_id,
                double rumifact);

    /**
     * @brief create the sns_client_request_msg carrying a
     *        sns_diag_log_trigger_req based on log_type
     * @param cookie - uint64_t memory/island log cookie
     * @param suid - suid for the diag_sensor
     * @param log_type - log_type for diag_sensor (memory or island logging)
     */
    sns_client_request_msg create_diag_log_request(uint64_t cookie,
                                                   suid sensor_uid,
                                                   sns_diag_triggered_log_type log_type);

   /**
     * send memory/island log request with cookie to diag_sensor
     *
     * @param cookie - unique cookie
     * @param sensor_uid   - SUID of sensor
     * @param log_type   - log_type of sensor(memory or island)
     */
    void send_diag_log_req(uint64_t cookie, suid sensor_uid, sns_diag_triggered_log_type log_type);

   /**
    * @brief lookup attributes for input data_type and suid
    * @param data_type
    * @param suid
    */
    void attributes_lookup(string &data_type, suid &sensor_uid);

private:
    void handle_event(const uint8_t *data, size_t size, uint64_t time_stamp, const string& data_type);
    std::mutex m;
    std::condition_variable cv;
    bool drva_test_done = false;
    size_t num_sns_broadcast_events = 0;
    int _hub_id = -1;
};

sns_client_request_msg
da_test_runner::create_diag_log_request( uint64_t cookie, suid sensor_uid, sns_diag_triggered_log_type log_type)
{
    sns_client_request_msg req_message;
    sns_diag_log_trigger_req trigger_req;
    string trigger_req_encoded;

    /* populate trigger message */
    trigger_req.set_log_type( log_type);
    trigger_req.set_cookie( cookie);
    trigger_req.SerializeToString(&trigger_req_encoded);

    /* populate client request message */
    req_message.mutable_suid()->set_suid_high(sensor_uid.high);
    req_message.mutable_suid()->set_suid_low(sensor_uid.low);
    req_message.set_msg_id( SNS_DIAG_SENSOR_MSGID_SNS_DIAG_LOG_TRIGGER_REQ);
    req_message.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    req_message.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    req_message.mutable_request()->set_payload(trigger_req_encoded);
#ifdef SNS_CLIENT_TECH_ENABLED
    req_message.set_client_tech(SNS_TECH_SENSORS);
#endif
    return req_message;
}

sns_client_request_msg
da_test_runner::create_da_test_request(const char *cmd_line_args,
                                       suid sensor_uid,
                                       int32_t msg_id)
{
    sns_client_request_msg req_message;
    sns_da_test_req da_test_req;
    string pb_da_test_encoded;

    /* populate driver acceptance request message */
    da_test_req.set_test_args(cmd_line_args);
    da_test_req.SerializeToString(&pb_da_test_encoded);

    /* populate client request message */
    req_message.mutable_suid()->set_suid_high(sensor_uid.high);
    req_message.mutable_suid()->set_suid_low(sensor_uid.low);
    req_message.set_msg_id( SNS_DA_TEST_MSGID_SNS_DA_TEST_REQ);
    req_message.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    req_message.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    uint32_t batch_period_us = ( uint32_t)(config.da_test_batch_period * 1000000);
    req_message.mutable_request()->mutable_batching()->set_batch_period( batch_period_us);
    req_message.mutable_request()->set_payload(pb_da_test_encoded);
#ifdef SNS_CLIENT_TECH_ENABLED
    req_message.set_client_tech(SNS_TECH_SENSORS);
#endif
    return req_message;
}

/* conditionally send debug message to stdout */
static void debug_message( string message)
{
   if (config.debug) {
      static std::mutex mu;
      unique_lock<mutex> lk(mu); // serialize debug message
      cout << unique_id << " " << message << endl;
   }
}
/* lookup attributes from the input datatype/suid */
void da_test_runner::attributes_lookup(string &data_type, suid &sensor_uid)
{
    sns_client_request_msg req_message;

    req_message.set_msg_id( SNS_STD_MSGID_SNS_STD_ATTR_REQ);
    req_message.mutable_request()->clear_payload();
    req_message.mutable_suid()->set_suid_high(sensor_uid.high);
    req_message.mutable_suid()->set_suid_low(sensor_uid.low);
    req_message.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    req_message.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
#ifdef SNS_CLIENT_TECH_ENABLED
    req_message.set_client_tech(SNS_TECH_SENSORS);
#endif

    string req_message_encoded;
    req_message.SerializeToString(&req_message_encoded);

    attrib_lookup_complete = false;

    unique_ptr<sessionFactory> factory = nullptr;
    try
    {
       factory =  make_unique<sessionFactory>();
    }
    catch(...)
    {
        printf("Get session failed for sessionFactory");
    }

    if(nullptr == factory){
        printf("Failed to create factory instance");
        return;
    }

    unique_ptr<ISession> session = unique_ptr<ISession>(factory->getSession(_hub_id));
    if(nullptr == session){
      printf("Failed to create session for hub_id %d for suid discovery", _hub_id);
      return;
    }

    int ret = session->open();
    if(-1 == ret){
        printf("Failed to open ISession");
        return;
    }

    ISession::eventCallBack event_cb =[this, data_type](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                  { this->handle_event(msg, msgLength, time_stamp, data_type); };

    ret = session->setCallBacks(sensor_uid,nullptr,nullptr,event_cb);
    if(0 != ret)
        printf("All callbacks are null, no need to register it");

    ret = session->sendRequest(sensor_uid, req_message_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }

    msg_file_logger->log(data_type, SENSORS_DIAG_LOG_REQUEST, &req_message);

    // wait for attribute lookup to complete
    auto now = std::chrono::system_clock::now();
    auto interval = 1000ms * 5; // 5 second timeout
    auto then = now + interval;

    unique_lock<mutex> lk(m);
    while ( !attrib_lookup_complete) {
        if ( cv.wait_until(lk, then) == std::cv_status::timeout) {
           string detail = " Timeout waiting for " + data_type
                + " attribute lookup!";
           throw runtime_error( detail);
        }
    }
    debug_message(" Attributes lookup for " + data_type + " complete.");
    if(nullptr != session) {
      session->close();
    }
}

/* remove trailing whitespace */
static string rtrim( const string& str)
{
   const auto str_end = str.find_last_not_of (whitespace);
   return (str_end == string::npos) ? "" : str.substr(0, str_end + 1);
}
/* remove leading whitespace */
static string ltrim( const string& str)
{
   const auto str_begin = str.find_first_not_of( whitespace);
   return (str_begin == string::npos) ? "" : str.substr(str_begin);
}
/* remove leading and trailing whitespace */
static string trim( const string& str)
{
   return ltrim(rtrim(str));
}

/**
 * return true when attributes match da_test_name
 *
 * @param pb_attr_event
 * @param data_type
 *
 * @return bool
 */
static bool do_attributes_match_target(sns_std_attr_event pb_attr_event, string data_type)
{
    bool match = true;
    for (int i = 0; i < pb_attr_event.attributes_size(); i++) {
        int32_t attr_id = pb_attr_event.attributes(i).attr_id();
        const sns_std_attr_value& attr_value = pb_attr_event.attributes(i).value();


        if ( attr_id == SNS_STD_SENSOR_ATTRID_NAME) {
            // attr_value.values(0).str() produces a string that includes the '\0'
            string raw = attr_value.values(0).str();
            if ( raw.back() == '\0' ) {
                // cout << "raw deal" << endl;
                raw = raw.substr(0, raw.size() - 1); // strip embedded '\0'
            }
            string s = trim(raw);

            if ((data_type == "da_test") && (s == string(config.da_test_name))) {
                return true;
            } else {
                if (config.has_name && (string(config.name) != s)) {
                    match = false;
                    break;
                }
            }
        } else if (attr_id == SNS_STD_SENSOR_ATTRID_RIGID_BODY) {
            if (config.has_rigid_body) {
                int v = attr_value.values(0).sint();
                string rb = string(config.rigid_body);

                if (!((rb  == "display" && v == SNS_STD_SENSOR_RIGID_BODY_TYPE_DISPLAY) ||
                    (rb  == "keyboard" && v == SNS_STD_SENSOR_RIGID_BODY_TYPE_KEYBOARD) ||
                    (rb  == "external" && v == SNS_STD_SENSOR_RIGID_BODY_TYPE_EXTERNAL)))
                {
                    match = false;
                    break;
                }
            }
        } else if (attr_id == SNS_STD_SENSOR_ATTRID_HW_ID) {
            if (config.has_hw_id) {
                if (attr_value.values(0).sint() != config.hw_id)
                {
                    match = false;
                    break;
                }
            }
        } else if ( attr_id == SNS_STD_SENSOR_ATTRID_STREAM_TYPE) {
            if (config.has_stream_type) {
                if (attr_value.values(0).sint() != config.stream_type)
                {
                    match = false;
                    break;
                }
            }
        }
    }

    return match;
}

// display fields from the da_test_log
static void parse_da_test_log(sns_da_test_log *da_test_log)
{
   string key;
   string value;

   if(da_test_log == NULL)
     return ;

   if (da_test_log->has_time_to_first_event()) {
      key ="time_to_first_event";
      value = to_string(da_test_log->time_to_first_event());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_time_to_last_event()) {
      key ="time_to_last_event";
      value = to_string(da_test_log->time_to_last_event());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_sample_ts()) {
      key ="sample_ts";
      value = to_string(da_test_log->sample_ts());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_total_samples()) {
      key ="total_samples";
      value = to_string(da_test_log->total_samples());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_avg_delta()) {
      key ="avg_delta";
      value = to_string(da_test_log->avg_delta());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_phy_config_sample_rate()) {
      key ="phy_config_sample_rate";
      value = to_string(da_test_log->phy_config_sample_rate());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   else if (da_test_log->has_recvd_phy_config_sample_rate()) {
      key ="recvd_phy_config_sample_rate";
      value = to_string(da_test_log->recvd_phy_config_sample_rate());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_random_seed_used()) {
      key ="random_seed_used";
      value = to_string(da_test_log->random_seed_used());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_num_request_sent()) {
      key ="num_request_sent";
      value = to_string(da_test_log->num_request_sent());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
   if (da_test_log->has_first_sample_timestamp()) {
      key ="first_sample_timestamp";
      value = to_string(da_test_log->first_sample_timestamp());
      cout << unique_id << " -" << key << "=" << value << endl;
   }
}

test_msg_file_logger::test_msg_file_logger(const string& filename) : msg_file(filename, std::ios_base::out | std::ios::binary | std::ios_base::app) {}

uint64_t test_msg_file_logger::convert_to_packet_timestamp(std::chrono::system_clock::time_point timestamp)
{
    const uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp.time_since_epoch()).count();

    constexpr uint64_t TICK_PERIOD_NS = 1250000;
    const uint64_t ts_1250us_ticks = timestamp_ns / TICK_PERIOD_NS;

    constexpr uint16_t TS_CHIPS_RES = 49152;
    const uint64_t duration_since_tick_ns = timestamp_ns - ts_1250us_ticks * TICK_PERIOD_NS;
    const uint16_t ts_chips = duration_since_tick_ns * TS_CHIPS_RES / TICK_PERIOD_NS;

    const uint64_t packet_ts = ts_1250us_ticks << 16 | ts_chips;
    return packet_ts;
}

void test_msg_file_logger::write_test_events(uint16_t packet_type, const char* pb_encoded_payload, uint16_t payload_len)
{
    // Write DLF entry length
    constexpr uint16_t DLF_HEADER_ENTRY_LEN = 12;
    const uint16_t dlf_entry_len = payload_len + DLF_HEADER_ENTRY_LEN;
    for (size_t idx = 0; idx < sizeof(dlf_entry_len); ++idx)
    {
        const char byte = (dlf_entry_len >> (8 * idx)) & 0xFF;
        msg_file.write(&byte, 1);
    }

    // Write packet type
    for (size_t idx = 0; idx < sizeof(packet_type); ++idx)
    {
        const char byte = (packet_type >> (8 * idx)) & 0xFF;
        msg_file.write(&byte, 1);
    }

    // Write timestamp
    const uint64_t packet_ts = convert_to_packet_timestamp(std::chrono::system_clock::now());
    for (size_t idx = 0; idx < sizeof(packet_ts); ++idx)
    {
        const char byte = (packet_ts >> 8 * idx) & 0xFF;
        msg_file.write(&byte, 1);
    }

    msg_file.write(pb_encoded_payload, payload_len);
}

void test_msg_file_logger::log(const string& src_sensor_data_type, sensors_diag_logid log_type, void* msg)
{
    if (!msg_file.is_open() || (msg == nullptr))
    {
        return;
    }

    sns_diag_client_api_log client_api_log_event_msg;
    sns_diag_sensor_log sensor_log_event_msg;

    sns_std_suid suid;
    suid.set_suid_low(CLIENT_MANAGER_SUID_LOW);
    suid.set_suid_high(CLIENT_MANAGER_SUID_HIGH);

    client_api_log_event_msg.set_client_id(unique_id);
    client_api_log_event_msg.set_src_sensor_type(src_sensor_data_type.c_str());

    uint16_t log_code;
    if (log_type == SENSORS_DIAG_LOG_REQUEST)
    {
        log_code = LOG_SNS_HLOS_REQUEST_C;
        sns_client_request_msg* req_msg = static_cast<sns_client_request_msg*>(msg);
        client_api_log_event_msg.set_allocated_request_payload(req_msg);
        sensor_log_event_msg.set_log_id(SENSORS_DIAG_LOG_REQUEST);
    }
    else if (log_type == SENSORS_DIAG_LOG_EVENT)
    {
        log_code = LOG_SNS_HLOS_EVENT_C;
        sns_client_event_msg* event_msg = static_cast<sns_client_event_msg*>(msg);
        client_api_log_event_msg.set_allocated_event_payload(event_msg);
        sensor_log_event_msg.set_log_id(SENSORS_DIAG_LOG_EVENT);
    }
    else
    {
        log_code = LOG_SNS_HLOS_RESPONSE_C;
        sns_diag_client_resp_msg* resp_msg = static_cast<sns_diag_client_resp_msg*>(msg);
        client_api_log_event_msg.set_allocated_resp_payload(resp_msg);
        sensor_log_event_msg.set_log_id(SENSORS_DIAG_LOG_RESPONSE);
    }

    uint64_t time_stamp = sensors_timeutil::get_instance().qtimer_get_ticks();
    string* sensor_data_type_str = new string(logging_module_name);

    sensor_log_event_msg.set_timestamp(time_stamp);
    sensor_log_event_msg.set_allocated_suid(&suid);
    sensor_log_event_msg.set_allocated_sensor_type(sensor_data_type_str);
    sensor_log_event_msg.set_instance_id(0);
    sensor_log_event_msg.set_allocated_client_api_payload(&client_api_log_event_msg);

    string pb_encoded_sns_diag_sensor_log;
    sensor_log_event_msg.SerializeToString(&pb_encoded_sns_diag_sensor_log);

    if (log_type == SENSORS_DIAG_LOG_REQUEST)
    {
        client_api_log_event_msg.release_request_payload();
    }
    else if (log_type == SENSORS_DIAG_LOG_EVENT)
    {
        client_api_log_event_msg.release_event_payload();
    }
    else
    {
        client_api_log_event_msg.release_resp_payload();
    }

    sensor_log_event_msg.release_suid();
    sensor_log_event_msg.release_client_api_payload();

    write_test_events(log_code, pb_encoded_sns_diag_sensor_log.c_str(), pb_encoded_sns_diag_sensor_log.size());
}

/* HLOS log event msg */
static void log_event( string from_data_type, string encoded_event_msg)
{
   if(sensors_diag_log_obj != NULL) {
      sensors_diag_log_obj->log_event_msg( encoded_event_msg,
                                           from_data_type,
                                           logging_module_name);
   }
}

void da_test_runner::handle_event(const uint8_t *data, size_t size, uint64_t time_stamp, const string& data_type)
{
    sns_client_event_msg event_msg;
    event_msg.ParseFromArray(data, size);

    if (event_msg.events_size() < 1) {
        sns_logi("no events in message");
        return;
    }

    sns_std_suid sensor_uid; // SENSORTEST-1218
    sensor_uid = event_msg.suid();

    string encoded_event_msg((char *)data, size);
    msg_file_logger->log(data_type, SENSORS_DIAG_LOG_EVENT, &event_msg);
    log_event( data_type, encoded_event_msg);

    for ( int i = 0; i < event_msg.events_size(); i++) {

       const sns_client_event_msg_sns_client_event& pb_event =
           event_msg.events(i);

       auto msg_id = pb_event.msg_id();
       if ( msg_id == SNS_DA_TEST_MSGID_SNS_DA_TEST_EVENT) {

          sns_da_test_event da_test_event;
          da_test_event.ParseFromString(pb_event.payload());

          /* set result_description from da_test_event message*/
          string r = da_test_event.test_event();

          sns_logi("received event: %s", r.c_str());
          cout << unique_id << " received event: " << r.c_str() << endl;

          int  len_recalc_timeout = sizeof( lit_recalc_timeout) - 1;
          if ( 0 == strncmp( r.c_str(), lit_pass, 4)
               || 0 == strncmp( r.c_str(), lit_fail, 4)
               || 0 == strncmp( r.c_str(), lit_recalc_timeout,
                                len_recalc_timeout)) {

             strlcpy( result_description, r.c_str(), sizeof( result_description));

             unique_lock<mutex> lk(m);
             if ( 0 == strncmp( result_description,
                                lit_recalc_timeout,
                                len_recalc_timeout)) {
                 // get number of addtional seconds from "RECALC_DURATION number\0"
                 recalc_duration = atof( result_description + len_recalc_timeout);
                 sns_logi(" recalc_timeout additional seconds: %f", recalc_duration);
             }
             else {
                 recalc_duration = 0.0;
                 drva_test_done = true;
             }
             cv.notify_one();                      // PASS, FAIL, or RECALC_TIMEOUT
          }
          else if (r.find("\"sensor\":\"motion_detect\"") != string::npos)
          {
            cout << unique_id << " md_event: " << r.c_str() << endl;
          }
          else {
             sns_logi("received event:: %s", r.c_str());
             sns_logi("ignored event: %s", r.c_str());
             cout << unique_id << " ignored event: " << r.c_str() << endl;
          }
       }
       else if ( msg_id == SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
            cout << unique_id << " Received attribute event for " << data_type << "." << endl;

            sns_std_attr_event pb_attr_event;
            pb_attr_event.ParseFromString(pb_event.payload());
            if ( do_attributes_match_target(pb_attr_event, data_type)) {
                sns_std_suid sensor_uid = event_msg.suid();
                if (data_type == "da_test") {
                    target_da_test_suid.high = sensor_uid.suid_high();
                    target_da_test_suid.low = sensor_uid.suid_low();
                    found_da_test_sensor = true;
                } else {
                    target_sensor_suid.high = sensor_uid.suid_high();
                    target_sensor_suid.low = sensor_uid.suid_low();
                    found_target_sensor = true;
                }
            }

            unique_lock<mutex> lk(m);   // attribute found
            attrib_lookup_complete = true;
            cv.notify_one();
       }
       else if ( msg_id == SNS_DA_TEST_MSGID_SNS_DA_TEST_LOG) {
          sns_da_test_log da_test_log;
          da_test_log.ParseFromString(pb_event.payload());
          debug_message("DebugString(): " + da_test_log.DebugString());
          cout << unique_id << " Received Test Logs from da_test sensor." << endl;
          parse_da_test_log( &da_test_log);
       }
       else if (msg_id == SNS_DA_TEST_MSGID_SNS_DA_TEST_BROADCAST_EVENT) {
           ++num_sns_broadcast_events;
       }
       else if (msg_id == SNS_STD_MSGID_SNS_STD_ERROR_EVENT) {
           sns_std_error_event error_event;
           error_event.ParseFromString(pb_event.payload());
           cout << unique_id << "Received error event, error = " << error_event.error() << endl;
       }
       else {
          cout << unique_id << " Unknown event with msg_id=" << msg_id << " recieved." << endl;
       }
    }
}

/**
 * send memory/island log request with cookie to diag_sensor
 *
 * @param cookie - unique cookie
 * @param sensor_uid   - SUID of sensor
 * @param log_type   - log_type of sensor(memory or island)
 */
void da_test_runner::send_diag_log_req(uint64_t cookie, suid sensor_uid, sns_diag_triggered_log_type log_type)
{
    if ((sensor_uid.high | sensor_uid.low) == 0) {
        printf("Skipping optional request to diag_sensor\n");
        return;
    }

    const char *logging_type = log_type == SNS_DIAG_TRIGGERED_LOG_TYPE_MEMORY_USAGE_LOG ? "memory" : "island";
    sns_logi("%s %d %" PRIu64, "enter send_diag_log_req", logging_type, cookie);
    cout << unique_id << " Requested " << logging_type << " logs with cookie=" << to_string(cookie) << endl;

    sns_client_request_msg req_message;
    req_message = create_diag_log_request(cookie, sensor_uid, log_type);

    unique_ptr<sessionFactory> factory = nullptr;
    try
    {
       factory =  make_unique<sessionFactory>();
    }
    catch(...)
    {
        printf("Get session failed for sessionFactory");
    }

    if(nullptr == factory){
        printf("Failed to create factory instance");
        return;
    }

    unique_ptr<ISession> session = unique_ptr<ISession>(factory->getSession(_hub_id));
    if(nullptr == session){
      printf("Failed to create session for hub_id %d for suid discovery", _hub_id);
      return;
    }
    int ret = session->open();
    if(-1 == ret){
        printf("Failed to open ISession");
        return;
    }
    ISession::eventCallBack event_cb =[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                  { this->handle_event(msg, msgLength, time_stamp, "da_test"); };

    ret = session->setCallBacks(sensor_uid,nullptr,nullptr,event_cb);
    if(0 != ret)
        printf("All callbacks are null, not registered any callback for suidlow=%" PRIu64 ",high=%" PRIu64, sensor_uid.low, sensor_uid.high);
    string req_message_encoded;
    req_message.SerializeToString(&req_message_encoded);
    ret = session->sendRequest(sensor_uid, req_message_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }

    const string diag_sensor_type = "diag_sensor";
    msg_file_logger->log(diag_sensor_type, SENSORS_DIAG_LOG_REQUEST, &req_message);

    sns_logi("%s %s", "exit send_diag_log_req", logging_type);

    if(nullptr != session){
        session->close();
    }
}

void da_test_runner::runner(const char *cmd_line_args,
                            double duration,
                            suid sensor_uid,
                            string& data_type,
                            int32_t msg_id,
                            double rumifact)
{
    sns_logi("%s", "enter runner");
    cout << unique_id << " Running the driver acceptance test..." << endl;

    sns_client_request_msg req_message;
    req_message = create_da_test_request(cmd_line_args, sensor_uid, msg_id);

    unique_ptr<sessionFactory> factory = nullptr;
    try
    {
       factory =  make_unique<sessionFactory>();
    }
    catch(...)
    {
        printf("Get session failed for sessionFactory");
    }

    if(nullptr == factory){
        printf("Failed to create factory instance");
        return;
    }

    unique_ptr<ISession> session = unique_ptr<ISession>(factory->getSession(_hub_id));
    if(nullptr == session){
      printf("Failed to create session for hub_id %d for suid discovery", _hub_id);
      return;
    }

    int ret = session->open();
    if(-1 == ret){
        printf("Failed to open ISession");
        return;
    }
    ISession::eventCallBack event_cb =[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                  { this->handle_event(msg, msgLength, time_stamp, "da_test"); };

    ret = session->setCallBacks(sensor_uid,nullptr,nullptr,event_cb);
    if(0 != ret)
        printf("all callbacks are null, not registered any callback for suid low=%" PRIu64 ",high=%" PRIu64, sensor_uid.low, sensor_uid.high);

    string req_message_encoded;
    req_message.SerializeToString(&req_message_encoded);
    ret = session->sendRequest(sensor_uid, req_message_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }

    msg_file_logger->log(data_type, SENSORS_DIAG_LOG_REQUEST, &req_message);

    auto cushion = 3000ms;        // android/test startup/shudown cushion
    auto now = std::chrono::system_clock::now();
    auto interval = 1000ms * duration * rumifact;

    /* Compute when to timeout as now + test duration + cushion */
    auto then = now + interval + cushion;

    sns_logi("waiting for da_test event");
    unique_lock<mutex> lk(m);
    auto cv_wait_pred = [&]() {return drva_test_done || (recalc_duration != 0.0); };
    while ( !drva_test_done) {
        if (!cv.wait_until(lk, then, cv_wait_pred)) {
            snprintf( result_description,
                      sizeof( result_description),
                      "FAIL %s timeout", my_exe_name);
            sns_logi("%s", result_description);
            break;
        }
        if ( recalc_duration != 0.0) {
            now = std::chrono::system_clock::now();
            interval = 1000ms * recalc_duration;
            then = now + interval + cushion;
            recalc_duration = 0.0;
        }
    }

    sns_logi("Received %lu sns_da_test_broadcast_event", num_sns_broadcast_events);
    sns_logi("%s", "exit  runner");
    if(nullptr != session){
        session->close();
    }
}

// SENSORTEST-1424
void crash_device()
{
    if (!config.debug) {
        return; // revised - crash only if debug enabled
    }
    cout << unique_id << " echo c > /proc/sysrq-trigger" << endl << endl;

    snprintf( result_description,
              sizeof( result_description),
              "FAIL %d echo c >  /proc/sysrq-trigger", unique_id);
    sns_loge( "%s", result_description);
    write_result_description();

    system( "echo c > /proc/sysrq-trigger");
    sleep(4); // 4 seconds
}

/*
 * Display the target sensor attributes
*/
void display_target_sensor () {
    cout << " Found" << " suid = 0x" << hex << setfill('0')
       << setw(16) << target_sensor_suid.high << setw(16) << target_sensor_suid.low << endl;
    cout << " -sensor=" << config.data_type << " ";
    if (config.has_name) {
        cout << "-name=" << config.name << " ";
    }
    if (config.has_rigid_body) {
        cout << "-rigid_body=" << config.rigid_body << " ";
    }
    if (config.has_stream_type) {
        cout << "-stream_type=" << to_string(config.stream_type) << " ";
    }
    if (config.has_hw_id) {
        cout << "-hw_id=" << to_string(config.hw_id);
    }
    if (hub_id != -1) {
        cout << "\thub_id = " << to_string(hub_id);
    }
    cout << endl;
}

void app_run(char *cmd_line_args, double duration, double rumifact)
{
    unique_ptr<sessionFactory> factory = nullptr;
    try
    {
       factory =  make_unique<sessionFactory>();
    }
    catch(...)
    {
        printf("Get session failed for sessionFactory");
    }

    if(nullptr == factory){
        printf("Failed to create factory instance");
        throw runtime_error("Failed to create factory instance");
    }

    string target_sensor_type = string(config.data_type);
    vector<int> hub_ids = factory->getSensingHubIds();
    if (hub_ids.size() == 0) {
        printf("No hub found.");
        throw runtime_error("No hub found.");
    } else {
        for (auto hub_id_lookup : hub_ids) {
            if (found_target_sensor)
                break;

            locate target_sensor;
            target_sensor.lookup(target_sensor_type, hub_id_lookup);
            if (target_sensor.count() != 0) {
                for (size_t i = 0; i < target_sensor.suids.size(); i++) {
                    da_test_runner get_attr(hub_id_lookup);
                    get_attr.attributes_lookup(target_sensor_type, target_sensor.suids[i]);

                    if (found_target_sensor) {
                        hub_id = hub_id_lookup;
                        break;
                    }
                }
            }
        }
    }

    if (!found_target_sensor) {
        string not_found = " target sensor = " + target_sensor_type + " not found.";
        cout << unique_id << not_found << endl;
        throw runtime_error(not_found);
    }
    else {
        display_target_sensor();
    }

    // get the diag sensor suid
    string diag_sensor_type = "diag_sensor";
    locate diag_sensor;
    suid diag_sensor_suid;
    diag_sensor.lookup(diag_sensor_type, hub_id);
    if (diag_sensor.count() != 0)
        diag_sensor_suid = diag_sensor.suids[0];

    // get da_test suids
    string data_type = "da_test";
    locate runner;
    runner.lookup(data_type, hub_id);

    // find the da_test or da_test_big_image target_suid
    for ( size_t i = 0; i < runner.suids.size(); i++) {
       da_test_runner get_attr(hub_id);
       get_attr.attributes_lookup(data_type, runner.suids[i]);

       if (found_da_test_sensor)
           break;
    }

    if (found_da_test_sensor) {
       cout << unique_id << " Using da_test name = "  << config.da_test_name
            << ", suid = [high=" << hex << target_da_test_suid.high
            << ", low=" << hex << target_da_test_suid.low << dec << "]"
            << (hub_id != -1 ? ", hub_id = " + to_string(hub_id) : "") << endl;
    }
    else {
       string not_found = " da_test name = " + string(config.da_test_name) + " not found.";
       cout << unique_id << not_found << endl;
       crash_device();
       throw runtime_error(not_found);
    }

    uint64_t cookie = unique_id;

    // Send memory and island log requests before the test
    for (auto log_type : {SNS_DIAG_TRIGGERED_LOG_TYPE_MEMORY_USAGE_LOG, SNS_DIAG_TRIGGERED_LOG_TYPE_ISLAND_LOG}) {
        da_test_runner logger(hub_id);
        logger.send_diag_log_req(cookie, diag_sensor_suid, log_type);
    }

    {
       da_test_runner command_line(hub_id);
       command_line.runner(cmd_line_args, duration, target_da_test_suid, data_type,
                           SNS_DA_TEST_MSGID_SNS_DA_TEST_REQ, rumifact);
    }

    // Send memory and island log requests after the test
    for (auto log_type : {SNS_DIAG_TRIGGERED_LOG_TYPE_MEMORY_USAGE_LOG, SNS_DIAG_TRIGGERED_LOG_TYPE_ISLAND_LOG}) {
        sleep(2); // 2 seconds
        da_test_runner logger(hub_id);
        logger.send_diag_log_req(cookie, diag_sensor_suid, log_type);
    }
    cout << unique_id << " " << result_description << endl;
}

/*
** append result_description to output file
** supports multiple concurrent ssc_drva_tests
*/
static void write_result_description( void)
{
   static string results_fn = LOG_PATH + string("drva_test_result");
   int fd = open( results_fn.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
   if ( fd >= 0) {
      if ( flock(fd, LOCK_EX) == 0) {
          string result_desc(result_description);
          string result = result_desc + ": " + cmdline + newline;
          ssize_t len = write( fd, result.c_str(), result.length());
          if ( len != (ssize_t)result.length()) {
              cout << unique_id << " FAIL writing the result to file "
                   << results_fn << " failed." << endl;
          }
         fsync(fd); // SENSORTEST-1424
      }
      else {
         cout << unique_id << " FAIL Unable to lock file " << results_fn << endl;
      }
      close(fd);
   }
   else {
      cout << unique_id << " FAIL Unable to open file " << results_fn << endl;
   }
}

// generate and return unique identifier per run
static int get_unique_id( void)
{
   int identifier = 1;
   static string uid_fn = LOG_PATH + string("drva_test_instance");
   char buffer[13] = { '\0' };

   int fd = open( uid_fn.c_str(), O_RDWR | O_CREAT, 0666);
   if ( fd >= 0) {
      if ( flock(fd, LOCK_EX) == 0) {
         ssize_t len = read( fd, (void *)buffer, (sizeof(buffer) - 1));
         if ( len > 0 ) {
            identifier = atoi((char *)buffer);
            identifier++;
         }
         off_t offset = lseek(fd, SEEK_SET, 0);
         size_t new_len = snprintf(buffer, sizeof(buffer), "%i", identifier);
         len = write( fd, (const void *)buffer, new_len + 1);
         fsync(fd); // SENSORTEST-1424
      }
      else {
         cout << unique_id << " FAIL Unable to lock file " << uid_fn << endl;
      }
      close(fd);
   }
   else {
      cout << unique_id << "FAIL Unable to open file " << uid_fn << endl;
   }
   return identifier;
}

/* signal handler for graceful handling of Ctrl-C */
void signal_handler(int signum)
{
    snprintf( result_description,
             sizeof( result_description),
             "FAIL %s SIGINT received. Aborted.", my_exe_name);
    sns_logi( "%s", result_description);
    write_result_description();
    cout << unique_id << " " << result_description << endl;

    exit(signum);
}

/* concatenate argvi to args to reconstruct command line */
static void concatenate(const char *argvi)
{
    if ( strlcat( args, argvi, sizeof( args)) >= sizeof(args)) {
            snprintf( result_description,
                      sizeof( result_description),
                      "FAIL %s command line args too long. Limit to %d chars.",
                      my_exe_name,
                      MAX_CMD_LINE_ARGS_LEN);
        sns_logi("%s", result_description);
        cout << unique_id << " " << result_description << endl;
        write_result_description();
        exit( EXIT_FAILURE);
    }
}

/* show usage for the testapp */
void usage()
{
    cout << "usage: ssc_drva_test\n"
            " -sensor=<sensor_type> -sample_rate=<float> ...\n"
            " -da_test_name=<test_name> -duration=<seconds>\n"
            " -prevent_apps_suspend=<0 | 1>\n"
            " -rumifact=<0 | 1> \n"
            " -da_test_batch_period=<seconds>\n"
            " -debug -help\n";
}

/**
 * @brief parse command line argument of the form keyword=value
 * @param parg[i] - command line argument
 * @param key[io] - gets string to left of '='
 * @param value[io] - sets string to right of '='
 */
bool get_key_value (char *parg, string& key, string& value) {
    char *pkey = parg;
    key = string(pkey, strlen(parg)); // SENSORTEST-1554

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

/*
 * @brief parse command line arguments
 * @param argc - number of command line arguments
 * @param argv - command line arguments
 * @return 0 on success, else failure
*/
int parse_arguments (int argc, char *argv[]) {
    // command line args:
    string sensor_eq("-sensor");
    string name_eq("-name");
    string da_test_name_eq("-da_test_name");
    string rigid_body_eq("-rigid_body");
    string hw_id_eq("-hw_id");
    string stream_type_eq("-stream_type");
    string duration_eq("-duration");
    string da_test_batch_period_eq("-da_test_batch_period");
    string rumifact_eq("-rumifact");
    string prevent_apps_suspend_eq("-prevent_apps_suspend");
    string debug_eq("-debug");

    for (int i = 1; i < argc; i++) {
        string key;
        string value;
        if (get_key_value(argv[i], key, value)) {
            if (da_test_name_eq == key) {
                config.has_da_test_name = true;
                strlcpy(config.da_test_name, value.c_str(), sizeof(config.da_test_name));
                continue;
            }
            else if (da_test_batch_period_eq == key) {
                config.da_test_batch_period = atof(value.c_str());
                continue;
            }
            else if (rumifact_eq == key) {
                config.rumifact = atof(value.c_str());
                continue;
            }
            else if (prevent_apps_suspend_eq == key) {
                config.has_prevent_apps_suspend = atoi(value.c_str());
                continue;
            }
            else
            {
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
                else if (hw_id_eq == key) {
                    config.has_hw_id = true;
                    config.hw_id = atoi(value.c_str());
                }
                else if (stream_type_eq == key) {
                    config.has_stream_type = true;
                    config.stream_type = atoi(value.c_str());
                    if (config.stream_type < 0 || config.stream_type > 2) {
                        cout << unique_id << "FAIL invalid stream_type value " << value.c_str() << endl;
                        exit(EXIT_FAILURE);
                    }
                }
                else if (duration_eq == key) {
                    config.duration = atof(value.c_str());
                }

                /* concatenate argv[] to reconstruct command line */
                string argvi(argv[ i]);  // SENSORTEST-2107
                size_t pos =  argvi.find_first_of(' ');
                if ( pos == string::npos ) {
                    concatenate(argv[ i]);
                }
                else { // argvi contains embedded blanks
                    pos = argvi.find_first_of('=');
                    if ( pos >= 1 ) {
                        concatenate(argvi.substr(0,pos + 1).c_str());
                        concatenate("\\\"");
                        concatenate(argvi.substr(pos+1).c_str());
                        concatenate("\\\"");
                    }
                }
                concatenate(" ");
            }
        }
        else if (debug_eq == key) {
            config.debug = true;
        }
        else if (0 == strncmp(argv[i], "-help", 5)) {
            usage();
            exit(EXIT_SUCCESS);
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
        usage();
        return 4; // failed
    }
    return 0; // successful parse
}

/* pass the command line arguments to the ssc driver acceptance module */
int main(int argc, char* argv[])
{
    *args = '\0';

    signal(SIGINT, signal_handler);

    unique_id = get_unique_id();

    const string msg_log_file_path = LOG_PATH + string("msg_log") + to_string(unique_id) + string(".dlf");
    msg_file_logger =  std::make_unique<test_msg_file_logger>(msg_log_file_path);
    if (msg_file_logger == nullptr)
    {
        cout << unique_id << " FAIL Failed to create a msg file logger object." << endl;
        exit(EXIT_FAILURE);
    }

    /* Parse/latch command line args */
    my_exe_name = argv[0];
    cout << unique_id << " " << my_exe_name << " version 1.36" << endl;

    // display command line
    for ( int i = 0; i < argc; i++) {
       oss << argv[i] << " ";
    }
    cmdline = oss.str();
    cout << unique_id << " " << cmdline << endl;

    int rc = parse_arguments(argc, argv);

    if (rc != 0)
        return EXIT_FAILURE;

    if (!config.has_da_test_name) {
        strlcpy(config.da_test_name, string("da_test").c_str(),
                sizeof(config.da_test_name));
    }

    if (config.debug)
        sensors_log::set_level(sensors_log::VERBOSE);

    try {
        size_t len = strlen( args);
        if (len >=  1)
            args[len - 1] = '\0';

        if (config.has_prevent_apps_suspend) { // SENSORTEST-1684
            debug_message(" util_wakelock::get_instance()");
            cout << unique_id << " Acquiring wakelock instance for "
                                 "preventing APPS suspend..." << endl;
            wake_lock = util_wakelock::get_instance();
            if ( wake_lock == nullptr) {
                snprintf( result_description,
                          sizeof( result_description),
                          "FAIL %i %s Unable to get an instance of wakelock.",
                          unique_id, my_exe_name);
                cout << unique_id << " " << result_description << endl;
                write_result_description();
                exit( EXIT_FAILURE);
            }

            char *wl_file = wake_lock->get_n_locks(1);
            if ( wl_file != nullptr) {
                cout << unique_id << " Acquired wakelock \"" << wl_file << "\"";
            } else {
                cout << unique_id << " Unable to acquire wakelock.";
                rc = EXIT_FAILURE;
            }
            cout << endl;
        }

        sensors_diag_log_obj = new sensors_diag_log(); //SENSORTEST-1218
        if (sensors_diag_log_obj == nullptr) {
            cout << unique_id << " FAIL Failed to create a new sensors_diag_log object." << endl;
            exit( EXIT_FAILURE);
        }

        app_run( args, config.duration, config.rumifact);
    } catch ( exception& e) {
        snprintf( result_description,
                  sizeof( result_description),
                  "FAIL %i %s caught exception: %s",
                  unique_id, my_exe_name, e.what());
        cout << unique_id << " " << result_description << endl;
        sns_loge( "%s", result_description);
        rc = EXIT_FAILURE;
    }

    write_result_description();

    if ( wake_lock != nullptr) {
        char *wl_file = wake_lock->put_n_locks(1);
        if ( wl_file != nullptr) {
            cout << unique_id << " Released wakelock \"" << wl_file << "\"";
        } else {
            cout << unique_id << " Unable to release wakelock.";
            rc = EXIT_FAILURE;
        }
        cout << endl;
    }

    if ( sensors_diag_log_obj) { // SENSORTEST-1218
       delete sensors_diag_log_obj;
       sensors_diag_log_obj = NULL;
    }
    return rc;
}
