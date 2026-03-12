/* ===================================================================
** Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
** All rights reserved.
** Confidential and Proprietary - Qualcomm Technologies, Inc.
**
** FILE: ssc_sensor_info.cpp
** DESC: Android command line application to list available suids
**       and attributes per suid
** ================================================================ */
#include <iostream>
#include <iomanip> // setfill, setw
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string.h>
#include <sys/time.h>
#include "SessionFactory.h"
#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif

#include "sns_client.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_suid.pb.h"

#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "sensors_log.h"
#include "sensor_diag_log.h" // SENSORTEST-2219
#include <signal.h>
using namespace google::protobuf::io;
using namespace std;
using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;

using suid = com::quic::sensinghub::suid;
#ifdef __ANDROID_API__
#define LOG_PATH "/data/"
#else
#define LOG_PATH "/etc/"
#endif

static bool debugging = false;
static int32_t hub_id = -1;
const int MAX_SENSOR_NAME_LEN = 30; // SENSORTEST-3220

typedef struct {
    string  data_type;
    suid  sensor_uid;
} dtuid_pair;

static std::vector<dtuid_pair> pairs;
static stringstream ss;  // gets sensor/attribute terminal/file output

// SENSORTEST-2219
static sensors_diag_log *sensors_diag_log_obj = NULL;
static char logging_module_name[] = "ssc_sensor_info";

/* conditionally send debug message to stdout
** because WD does not support sensors_log
*/
void debug_message( string message)
{
   if (debugging) {
      cout << message << endl;
   }
}

// SENSORTEST-2219
/* HLOS log event msg */
static void log_attr_event( string data_type, string encoded_event_msg)
{
    if (sensors_diag_log_obj != NULL) {
       sensors_diag_log_obj->log_event_msg( encoded_event_msg, data_type, logging_module_name);
    }
    else
        cout << "Error in sensor_diag_log instance." << endl;
}

class locate
{
public:
    void lookup( const string& datatype, bool default_only, float duration = 0.0, bool has_logging = false);
private:
    void on_suids_available(const std::string& datatype,
                            const std::vector<suid>& suids);
    void handle_ssc_event(const uint8_t *data, size_t size, uint64_t time_stamp, float duration, bool has_logging, int32_t id);
    suid _sensor_uid;

    bool _lookup_done;
    std::mutex _m;
    std::condition_variable _cv;
};

class do_attrib_lookup
{
public:
    do_attrib_lookup(int id) { _hub_id = id; }
    void lookup_attrib(bool has_logging = false);
private:
    void handle_attrib_event(const uint8_t *data, size_t size, uint64_t time_stamp, bool has_logging);
    std::mutex _m;
    std::condition_variable _cv;
    bool _lookup_done = false;
    int _hub_id;
};

void locate::on_suids_available(const string& datatype,
                                const vector<suid>& suids)
{
    string message( "found " + to_string(suids.size()) + " suid(s)");
    if ( datatype != "") {
       message += " for " + datatype;
    }
    debug_message( message);

    for ( size_t i = 0; i < suids.size(); i++) {

        dtuid_pair element;
        element.data_type = datatype;
        element.sensor_uid.high = suids[i].high;
        element.sensor_uid.low = suids[i].low;
        pairs.push_back(element);

        stringstream ss;
        if ( datatype != "") {
           ss << "datatype " << datatype << " ";
        }
        ss << "suid = 0x" << hex << element.sensor_uid.high << element.sensor_uid.low << dec;
        debug_message( ss.str());
    }

    unique_lock<mutex> lk(_m);
    _lookup_done = true;
    _cv.notify_one();

}

void locate::handle_ssc_event(const uint8_t *data, size_t size, uint64_t time_stamp, float duration, bool has_logging, int32_t id)
{
    /* parse the pb encoded event */
    sns_client_event_msg pb_event_msg;
    pb_event_msg.ParseFromArray(data, size);

    /* iterate over all events in the message */
    for (int i = 0; i < pb_event_msg.events_size(); i++) {
        auto& pb_event = pb_event_msg.events(i);
        if (pb_event.msg_id() != SNS_SUID_MSGID_SNS_SUID_EVENT) {
            sns_loge("invalid event msg_id=%d", pb_event.msg_id());
            continue;
        }
        sns_suid_event pb_suid_event;
        pb_suid_event.ParseFromString(pb_event.payload());
        const string& datatype =  pb_suid_event.data_type();

        sns_logv("suid_event for %s, num_suids=%d, ts=%fs", datatype.c_str(),
                 pb_suid_event.suid_size(),
                 std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now().
                                                time_since_epoch()).count());

        if (duration == 0) {
            /* create a list of  all suids found for this datatype */
            vector<suid> suids(pb_suid_event.suid_size());
            for (int j=0; j < pb_suid_event.suid_size(); j++) {
                suids[j] = suid(pb_suid_event.suid(j).suid_low(),
                                      pb_suid_event.suid(j).suid_high());
            }

            on_suids_available(datatype, suids);
        } else {
            ss << "SUID event(s) received: SUID count = " << pb_suid_event.suid_size()
            << ", Type = " << datatype << ", Time = " << time_stamp << endl;

            for ( size_t i = 0; i < (size_t)pb_suid_event.suid_size(); i++) {
                dtuid_pair element;
                element.data_type = datatype;
                element.sensor_uid.high = pb_suid_event.suid(i).suid_high();
                element.sensor_uid.low = pb_suid_event.suid(i).suid_low();
                pairs.push_back( element);
            }

            if (pairs.size() != 0) {
                do_attrib_lookup attribes(id);
                attribes.lookup_attrib(has_logging);
                pairs.clear();
            }
        }
    }
}

void locate::lookup(const string &datatype, bool default_only, float duration, bool has_logging)
{
    const suid LOOKUP_SUID = {
        12370169555311111083ull,
        12370169555311111083ull
    };

    unique_ptr<sessionFactory> factory = make_unique<sessionFactory>();
    if(nullptr == factory){
        printf("failed to create factory instance");
        return;
    }

    std::vector<int> sensingHubIds = factory->getSensingHubIds();
    if(sensingHubIds.size() == 0){
        printf("no sensing hub found");
        return;
    }

    for (auto id : sensingHubIds) {
        if (hub_id != -1 && id != hub_id) {
            printf("User provided hub_id %d != supported hub_id %d. Skipping...\n", hub_id, id);
            continue;
        }

        debug_message("lookup suids for hub_id " + to_string(id));

        _lookup_done = false;
        pairs.clear();

        ISession::eventCallBack event_cb =[this, duration, has_logging, id](const uint8_t* msg , size_t msgLength, uint64_t time_stamp)
                  { this->handle_ssc_event(msg, msgLength, time_stamp, duration, has_logging, id); };

        unique_ptr<ISession> session = unique_ptr<ISession>(factory->getSession(id));

        if(nullptr == session){
            printf("failed to create session for hub_id %d for suid discovery", id);
            return;
        }

        int ret = session->open();
        if(-1 == ret){
            printf("failed to open ISession");
            return;
        }
        ret = session->setCallBacks(LOOKUP_SUID,nullptr,nullptr,event_cb);
        if(0 != ret)
            printf("all callbacks are null, no need to register it");

        sns_client_request_msg pb_req_msg;
        sns_suid_req pb_suid_req;
        string pb_suid_req_encoded;

        sns_logv("requesting suid for %s, ts = %fs", datatype.c_str(),
                std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now().
                                                time_since_epoch()).count());

        /* populate SUID request */
        pb_suid_req.set_data_type(datatype);
        pb_suid_req.set_register_updates(true);
        pb_suid_req.set_default_only(default_only);
        pb_suid_req.SerializeToString(&pb_suid_req_encoded);

        /* populate the client request message */
        pb_req_msg.set_msg_id(SNS_SUID_MSGID_SNS_SUID_REQ);
        pb_req_msg.mutable_request()->set_payload(pb_suid_req_encoded);
        pb_req_msg.mutable_suid()->set_suid_high(LOOKUP_SUID.high);
        pb_req_msg.mutable_suid()->set_suid_low(LOOKUP_SUID.low);
        pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
        pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    #ifdef SNS_CLIENT_TECH_ENABLED
        pb_req_msg.set_client_tech(SNS_TECH_SENSORS);
    #endif
        string pb_req_msg_encoded;
        pb_req_msg.SerializeToString(&pb_req_msg_encoded);
        ret = session->sendRequest(LOOKUP_SUID, pb_req_msg_encoded);
        if(0 != ret){
            printf("Error in sending request");
            return;
        }
        debug_message("waiting for suid lookup");

        if (duration == 0) {
            auto now = std::chrono::system_clock::now();
            auto interval = 1000ms * 5; // 5 second timeout
            auto then = now + interval;

            unique_lock<mutex> lk(_m);
            while ( !_lookup_done) {
                if ( _cv.wait_until(lk, then) == std::cv_status::timeout) {
                    cout << "suid not found for " << datatype << endl;
                    break;
                }
            }
        } else {
            cout << "Sleep( " << duration << ") seconds" << endl;
            usleep((int) duration * 1000000);
        }

        if (nullptr != session){
            session->close();
        }

        // Do attribute lookup for this hub_id
        if (duration == 0) {
            debug_message("querying attribute for hub_id " + to_string(id));
            do_attrib_lookup attribes(id);
            attribes.lookup_attrib(has_logging);
        }

        debug_message("end suid lookup");
    }
}

class sensor_attributes {
public:
    const sns_std_attr_value& get(int32_t attr_id) {
        return _attr_map.at(attr_id);
    }

    void put( int32_t attr_id, const sns_std_attr_value& attr_value) {
        _attr_map[ attr_id] = attr_value;
    }

    string attr_to_string( int32_t attr_id,
                           const sns_std_attr_value& attr_value);

    const string& get_name( int32_t attr_id) {
       return _attr_id_to_name.at( attr_id);
    }
    const string& get_id( string name);

    void display();

sensor_attributes() {
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_NAME          ] = "NAME          ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_VENDOR        ] = "VENDOR        ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_TYPE          ] = "TYPE          ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_AVAILABLE     ] = "AVAILABLE     ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_VERSION       ] = "VERSION       ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_API           ] = "API           ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_RATES         ] = "RATES         ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_RESOLUTIONS   ] = "RESOLUTIONS   ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_FIFO_SIZE     ] = "FIFO_SIZE     ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_ACTIVE_CURRENT] = "ACTIVE_CURRENT";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_SLEEP_CURRENT ] = "SLEEP_CURRENT ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_RANGES        ] = "RANGES        ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_OP_MODES      ] = "OP_MODES      ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_DRI           ] = "DRI           ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_STREAM_SYNC   ] = "STREAM_SYNC   ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_EVENT_SIZE    ] = "EVENT_SIZE    ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_STREAM_TYPE   ] = "STREAM_TYPE   ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_DYNAMIC       ] = "DYNAMIC       ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_HW_ID         ] = "HW_ID         ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_RIGID_BODY    ] = "RIGID_BODY    ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR             ] = "PHYSICAL_SENSOR      ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR_TESTS       ] = "PHYSICAL_SENSOR_TESTS";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_SELECTED_RESOLUTION         ] = "SELECTED_RESOLUTION  ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_SELECTED_RANGE              ] = "SELECTED_RANGE       ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_ADDITIONAL_LOW_LATENCY_RATES] = "LOW_LATENCY_RATES    ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_PASSIVE_REQUEST             ] = "PASSIVE_REQUEST      ";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_TRANSPORT_MTU_SIZE            ] = "TRANSPORT_MTU_SIZE";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_HLOS_INCOMPATIBLE             ] = "HLOS_INCOMPATIBLE";
    _attr_id_to_name[SNS_STD_SENSOR_ATTRID_SERIAL_NUM                    ] = "SERIAL_NUM";

    _display_order.push_back( SNS_STD_SENSOR_ATTRID_NAME);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_VENDOR);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_TYPE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_AVAILABLE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_VERSION);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_API);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_RATES);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_RESOLUTIONS);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_RANGES);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_DRI);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_FIFO_SIZE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_STREAM_TYPE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_STREAM_SYNC);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_DYNAMIC);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_EVENT_SIZE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_OP_MODES);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_ACTIVE_CURRENT);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_SLEEP_CURRENT);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_HW_ID);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_RIGID_BODY);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR_TESTS);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_SELECTED_RESOLUTION);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_SELECTED_RANGE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_ADDITIONAL_LOW_LATENCY_RATES);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_PASSIVE_REQUEST);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_TRANSPORT_MTU_SIZE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_HLOS_INCOMPATIBLE);
    _display_order.push_back( SNS_STD_SENSOR_ATTRID_SERIAL_NUM);
}

private:
    std::map<int32_t, string> _attr_id_to_name;
    std::unordered_map<int32_t, sns_std_attr_value> _attr_map;
    std::vector<int32_t> _display_order;
};

/* remove leading and trailing whitespace */
std::string trim(const std::string& str,
                 const std::string& whitespace = " \t")
{
    const auto str_begin = str.find_first_not_of(whitespace);
    if (str_begin == std::string::npos)
        return ""; // all blanks

    const auto str_end = str.find_last_not_of(whitespace);
    const auto str_len = str_end - str_begin;

    return str.substr(str_begin, str_len);
}

/* returns string representation of attr_value */
string sensor_attributes::attr_to_string( int32_t attr_id,
                                          const sns_std_attr_value& attr_value)
{
    auto count = attr_value.values_size();

    string s;
    try {
       s = get_name( attr_id);   // get attribute name
    }
    catch ( std::out_of_range const&) {
       s = "unknown id(" + to_string(attr_id) + ")"; // attribute name not known
    }
    s += " = ";

    if ( count) {
        if ( count > 1) { s += "["; }

        for (int i=0; i < count; i++) {
            if ( i) { s += ", ";}
            if (attr_value.values(i).has_str()) {
                s += trim(attr_value.values(i).str());;
            }
            else if (attr_value.values(i).has_sint()) {
                int v = attr_value.values(i).sint();
                if ( attr_id == SNS_STD_SENSOR_ATTRID_AVAILABLE
                     || attr_id == SNS_STD_SENSOR_ATTRID_DYNAMIC) {
                    s += (v) ? "true" : "false";
                }
                else if ( attr_id == SNS_STD_SENSOR_ATTRID_VERSION) {
                    uint16_t revision = v & 0xff;
                    uint16_t minor = ( v & 0xff00) >> 8;
                    int major =  v >> 16;
                    s += to_string( major) + "." + to_string( minor) + "." +
                         to_string( revision);
                }
                else if ( attr_id == SNS_STD_SENSOR_ATTRID_STREAM_TYPE) {
                    if ( v == SNS_STD_SENSOR_STREAM_TYPE_STREAMING) {
                        s += "streaming";
                    }
                    else if ( v == SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE) {
                        s += "on_change";
                    }
                    else if ( v == SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT) {
                        s += "single_output";
                    }
                    else {
                        s += "unknown";
                    }
                }
                else if ( attr_id == SNS_STD_SENSOR_ATTRID_RIGID_BODY) {
                    if ( v == SNS_STD_SENSOR_RIGID_BODY_TYPE_DISPLAY) {
                        s += "display";
                    }
                    else if ( v == SNS_STD_SENSOR_RIGID_BODY_TYPE_KEYBOARD) {
                        s += "keyboard";
                    }
                    else if ( v == SNS_STD_SENSOR_RIGID_BODY_TYPE_EXTERNAL) {
                        s += "external";
                    }
                    else {
                        s += "unknown";
                    }
                }
                else {
                    s += to_string( v);
                }
            }
            else if (attr_value.values(i).has_flt()) {
                s += to_string( attr_value.values(i).flt());
            }
            else if (attr_value.values(i).has_boolean()) {
                int v = attr_value.values(i).boolean();
                s += (v) ? "true" : "false";
            }
            else if (attr_value.values(i).has_subtype()) {
                // assert subtype used only for ranges of float.
                sns_std_attr_value subtype = attr_value.values(i).subtype();
                s += "[";
                for ( int j=0; j < subtype.values_size(); j++) {
                    if ( j) { s += ","; }
                    if (subtype.values(j).has_flt()) {
                        s += to_string( subtype.values(j).flt());
                    }
                    else {
                        s += "!flt";
                    }
                }
                s += "]";
            }
            else {
                s += "!!! unknown value type !!!";
            }
        }
        if ( count > 1) { s += "]"; }
    }

    return s;
}

/* display attributes using determinstic order */
void sensor_attributes::display()
{
    for ( size_t i=0; i < _display_order.size(); i++) {
        int32_t attr_id = _display_order[ i];

        auto it = _attr_map.find( attr_id);
        if ( it != _attr_map.end()) {
            const sns_std_attr_value& attr_value = get( attr_id);
            string sout = attr_to_string( attr_id, attr_value);
            ss << sout << endl;
        }
    }
    // handle new unknown attributes
    int32_t attr_id = _display_order.size();
    int32_t done_id = _attr_map.size();
    while ( attr_id < done_id) {
       const sns_std_attr_value& attr_value = get( attr_id);
       string sout = attr_to_string( attr_id, attr_value);
       ss << sout << endl;
       attr_id++;
    }
}

void do_attrib_lookup::lookup_attrib(bool has_logging)
{
    debug_message("begin lookup_attrib");

    ISession::eventCallBack attrb_event_cb =[this, has_logging](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                  { this->handle_attrib_event(msg, msgLength, time_stamp, has_logging); };

    unique_ptr<sessionFactory> factory = nullptr;
    try {
        factory = make_unique<sessionFactory>();
    } catch(...) {
        printf("Get session failed for sessionFactory");
    }
    if(nullptr == factory){
        printf("failed to create factory instance");
        return;
    }

    unique_ptr<ISession> session = unique_ptr<ISession>(factory->getSession(_hub_id));
    if(nullptr == session){
      printf("failed to create session for suid discovery");
      return;
    }

    int ret = session->open();
    if(-1 == ret){
        printf("failed to open ISession");
        return;
    }

    for ( size_t i = 0; i < pairs.size(); i++) {
        debug_message( "get_sensor_info " + to_string(i));

        string data_type = pairs[i].data_type;
        suid sensor_uid = pairs[i].sensor_uid;

        ret = session->setCallBacks(sensor_uid,nullptr,nullptr,attrb_event_cb);
        if(0 != ret)
            printf("all callbacks are null, no need to register it");

        stringstream ss;
        ss << "lookup_attrib for ";
        if ( data_type != "") {
           ss << "datatype " << data_type << " ";
        }
        ss << "suid = 0x" << hex << sensor_uid.high << sensor_uid.low << dec;
        debug_message( ss.str());

        sns_client_request_msg req_message;

        /* populate the client request message */
        req_message.set_msg_id( SNS_STD_MSGID_SNS_STD_ATTR_REQ);
        req_message.mutable_request()->clear_payload();
        req_message.mutable_suid()->set_suid_high(sensor_uid.high);
        req_message.mutable_suid()->set_suid_low(sensor_uid.low);
        req_message.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
        req_message.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);

        _lookup_done = false;
        string req_message_encoded;
        req_message.SerializeToString(&req_message_encoded);
        ret = session->sendRequest(sensor_uid, req_message_encoded);
        if(0 != ret)
            printf("Error in sending request");

        debug_message( "waiting for attr lookup");
        unique_lock<mutex> lk(_m);
        auto now = std::chrono::system_clock::now();
        auto then = now + 500ms;    // timeout after 500ms
        while (! _lookup_done) {
            if ( _cv.wait_until(lk, then) == std::cv_status::timeout) {
                debug_message( "attr lookup timed out");
                break;
            }
        }
        debug_message( "attr lookup done");
    }

    if(nullptr != session){
      session->close();
    }
    debug_message( "end lookup_attrib\n");
}

void do_attrib_lookup::handle_attrib_event(const uint8_t *data, size_t size,  uint64_t time_stamp, bool has_logging = false)
{
    sensor_attributes attributes;

    sns_client_event_msg event_msg;
    event_msg.ParseFromArray(data, size);

    stringstream oss;
    oss << "received attr event for suid " << hex << event_msg.suid().suid_high() << event_msg.suid().suid_low() << dec << " size:" << size;

    debug_message( oss.str());

    if (event_msg.events_size() < 1) {
        debug_message("no events in message");
        return;
    }

    const sns_client_event_msg_sns_client_event& pb_event =
        event_msg.events(0);

    sns_std_attr_event pb_attr_event;
    pb_attr_event.ParseFromString(pb_event.payload());

    ss << endl;   // SENSORTEST-897 setfill + setw
    ss << "SUID           = 0x" << hex << setfill('0')
                            << setw(16) << event_msg.suid().suid_high()
                            << setw(16) << event_msg.suid().suid_low()
                            << dec << endl;

    for (int i=0; i < pb_attr_event.attributes_size(); i++) {
        int32_t attr_id = pb_attr_event.attributes(i).attr_id();
        const sns_std_attr_value& attr_value = pb_attr_event.attributes(i).value();

        attributes.put( attr_id, attr_value);

        // SENSORTEST-2219
        if (has_logging) {
            string data_type = "";
            int pos = 0;
            if (attr_id == SNS_STD_SENSOR_ATTRID_TYPE) {
                string attrib_string = attributes.attr_to_string(attr_id, attr_value);
                // Get the datatype value from the type and value attribute pair
                pos = attrib_string.find("=");
                data_type = attrib_string.substr(pos + 2);
                string encoded_event_msg((char *)data, size);
                log_attr_event(data_type, encoded_event_msg);
            }
        }
    }

    attributes.display();

    unique_lock<mutex> lk(_m);
    _lookup_done = true;
    _cv.notify_one();
}

void get_sensor_info(char *target_sensor, bool default_only, float duration, bool has_logging = false)
{
    string target = target_sensor;

    locate sensors;
    sensors.lookup(target, default_only, duration, has_logging);
}

/* signal handler for graceful handling of Ctrl-C */
void signal_handler(int signum)
{
    string message( "fail SIGINT received. Aborted");
    debug_message( message);
    exit(signum);
}

void usage( void){
    cout << "usage: ssc_sensor_info [-sensor=name] [-delay=<time_in_seconds>] [-duration=<time_in_seconds>] ";
    cout << "[-default_only=<1|0>] [-log=<1|0>] [-help]" << endl;
    cout << "       where name: accel | gyro | mag | pressure | ..." << endl;
}

// write sensor/attributes info to file
void write_sensor_info( void)
{
    string out_fn = LOG_PATH + string("sensor_info.txt");
    ofstream outfile;
    outfile.open( out_fn);
    outfile << ss.str() << endl;
    outfile.close();
}

/* get/display attributes for one or all sensors */
int main(int argc, char* argv[])
{
    char target_sensor[ MAX_SENSOR_NAME_LEN];
    target_sensor[ 0] = '\0';

    const char lit_sensor_eq[] = "-sensor=";
    int lseq_len = strlen( lit_sensor_eq);

    const char lit_delay_eq[] = "-delay=";
    int ldelayeq_len = strlen( lit_delay_eq);
    const char lit_duration_eq[] = "-duration=";
    int lduration_len = strlen( lit_duration_eq);
    const char lit_default_only_eq[] = "-default_only=";
    int ldoq_len = strlen( lit_default_only_eq);

    bool default_only = false; // false by default
    float duration = 0.0; // in seconds
    float delay = 0.0; // in seconds
    bool has_delay = false;

    const char lit_logging_eq[] = "-log=";
    int llogging_len = strlen(lit_logging_eq);
    bool has_logging = false;

    string version = "ssc_sensor_info v1.89";
    cout << version << endl;

    signal(SIGINT, signal_handler);

    /* parse command line arg */
    for ( int i = 1; i < argc; i++) {
        if ( 0 == strncmp( argv[ i], lit_sensor_eq, lseq_len)) {
            strlcpy( target_sensor,
                     argv[ i] + lseq_len,
                     sizeof( target_sensor));
        }
        else if (strncmp(argv[i], lit_delay_eq, ldelayeq_len) == 0) {
            delay = std::atof(argv[i] + ldelayeq_len);
            has_delay = true;
        }
        else if (strncmp(argv[i], lit_duration_eq, lduration_len) == 0) {
            duration = std::atof(argv[i] + lduration_len);
        }
        else if (strncmp(argv[i], lit_default_only_eq, ldoq_len) == 0) {
            default_only = std::atoi(argv[i] + ldoq_len);
        }
        else if (strncmp(argv[i], "-hub_id=", 8) == 0) {
            hub_id = std::atoi(argv[i] + 8);
        }
        else if ( 0 == strncmp( argv[i], "-help", 5)
                 ||  0 == strncmp( argv[i], "-h", 2)) {
            usage();
            exit( 0);
        }
        else if ( 0 == strncmp( argv[i], "-debug", 6)) {
            debugging = true;
            sensors_log::set_level(sensors_log::VERBOSE);
        }
        else if (strncmp(argv[i], lit_logging_eq, llogging_len) == 0)  {
            has_logging = std::atoi(argv[i] + llogging_len);
        }
        else {
            cout << "unrecognized arg: " << argv[i] << endl;
            usage();
            exit( EXIT_FAILURE);
        }
    }

    int rc = 0;
    try {
        //delay block
        if (has_delay) {
            cout << "Sleep( " << delay << ") seconds" << endl;
            usleep((int) delay * 1000000);
        }//delay block end

        // SENSORTEST-2219
        if (has_logging) {
             sensors_diag_log_obj = new sensors_diag_log();
             if (sensors_diag_log_obj == nullptr) {
                 cout << "WARNING: new sensors_diag_log() instantiation failed. Diag logging will be disabled." << endl;
                 has_logging = false;
             }
        }

        get_sensor_info(target_sensor, default_only, duration, has_logging);
        write_sensor_info();        // write sensor/attributes info to file
        cout << ss.str() << endl;   // echo sensor/attributes info to terminal
    } catch (runtime_error& e) {
        string message( "fail caught runtime_error: ");
        message += e.what();
        debug_message( message);
        rc = EXIT_FAILURE;
    }

    if (sensors_diag_log_obj != NULL) { // SENSORTEST-2219
        delete sensors_diag_log_obj;
        sensors_diag_log_obj = NULL;
    }

    return rc;
}
