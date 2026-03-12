/* ===================================================================
** Copyright (c) 2017-2022 Qualcomm Technologies, Inc.
** All Rights Reserved.
** Confidential and Proprietary - Qualcomm Technologies, Inc.
**
** FILE: display_active.cpp
** DESC: parse and display sns_diag_cm_active_client_req_event
** ================================================================ */
#include <cinttypes>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>    // std::setprecision
#include <iostream>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/file.h>
#include <unistd.h>
#include <vector>

#include "see_salt.h"
#include "display_active.h"

using namespace std;

class active_req {
public:
    string timestamp;
    string suid_low;
    string suid_high;
    sens_uid suid;
    string msg_id;
    string client_proc_type;
    string delivery_type;
    string data_type;
    string register_updates;
    string default_only;
    string sample_rate;

    active_req() {
        timestamp = "";
        suid_low = "";
        suid_high = "";
        msg_id = "";
        client_proc_type = "";
        delivery_type = "";
        data_type = "";
        register_updates = "";
        default_only = "";
        sample_rate = "";
    }
};

// forward declarations
#ifdef SIMULATION
static string parse_arguments(int argc, char *argv[]);
static bool get_key_value(char *parg, string &key, string &value);
static string read_event(string filename);
#endif

void register_diag_sensor_event_cb(see_salt *psalt, sens_uid *diag_sensor_suid);
static void display_active(string event, bool is_registry_sensor);
static size_t find_element(string event, string name);
static string consume_preceeding(string event, size_t preceeding);
static bool process_active(string event);
static void show_legend();
static void show_active(active_req req);
static size_t scan(string event, size_t pos, string delimiter);
static string get_body(string event, string &body);
static string get_block(string event, string &block);
static bool get_value(string block, string name, string &value);
static uint64_t convert_to_uint64_t(string half_suid);
static string get_display_suid(sens_uid &suid);

static string shorten(string event);
static void failed(string reason);
static string trim(const string& str);

/* data */
static const string quote = "\"";
static const string colon = ":";
static const string left_brace = "{";
static const string right_brace = "}";
static const string left_bracket = "[";
static const string right_bracket = "]";
static const string comma = ",";

static const string newline ="\n";
static const string& whitespace = " \r\t\n";
static const string& comma_nl = ",\r\n";

static see_salt *_psalt = nullptr;
static bool _display_events = false;

// suid of the suid sensor
sens_uid suid_sensor(0xabababababababab, 0xabababababababab);

#ifdef SIMULATION
int main(int argc, char *argv[])
{
    cout << "display_active v1.1" << endl;
    string fn = parse_arguments(argc, argv);
    string event = trim(read_event(fn));
    display_active(event, false);
    return EXIT_SUCCESS;
}

/* parse command line arguments */
static string parse_arguments(int argc, char *argv[])
{
    string fn("-fn");

    for (int i = 1; i < argc; i++) {
        string key;
        string value;
        if (get_key_value(argv[i], key, value)) {
            if (key == fn) {
                return value;
            }
        }
    }

    cout << "missing -fn=filename" << endl;
    exit(EXIT_FAILURE);
}

/**
 * @brief parse command line argument of the form keyword=value
 * @param parg[i] - one command line argument
 * @param key[io] - gets string to left of '='
 * @param value[io] - sets string to right of '='
 * @return true - successful parse. false - parse failed.
 */
static bool get_key_value(char *parg, string &key, string &value)
{
   char *pkey = parg;

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

// open filename file and return it as a string
static string read_event(string filename)
{
    ifstream ifs;
    ifs.open(filename);
    if (!ifs.is_open()) {
        failed("failed to open: " + filename);
        exit(EXIT_FAILURE);
    }
    stringstream out;
    out << ifs.rdbuf();
    return out.str();
}
#endif

/**
* Register diag sensor event cb. This is the point
* @param display_events
*/
void register_diag_sensor_event_cb(see_salt *psalt, bool display_events)
{
    _display_events = display_events;
    if (psalt != nullptr) {
        _psalt = psalt; // latch see_salt instance SENSORTEST-2110
    }
}


/**
* diag sensor event cb
* Display the sns_diag_cm_active_client_req_event message
* @param event - json formated active_client_req_event message
* @param sensor_handle
* @param client_connect_id
*/
void display_active(string sensor_event, unsigned int sensor_handle,
                           unsigned int client_connect_id)
{
#ifndef SIMULATION
    if ( _display_events) {
        cout << sensor_event << endl;
    }

    // get pos of colon after "cm_request"
    size_t pos = find_element(sensor_event, "cm_request");
    if (pos == string::npos) {
        return; // not found
    }

    sensor_event = consume_preceeding(sensor_event, pos); // consume cm_request :
    show_legend();
    process_active(sensor_event);
#endif
}

/**
* return offset of the colon after the \"name\" element
* @param event message
* @param name target element name
*
* @return size_t string::npos or offset of colon after the \"name\" element
*/
static size_t find_element(string event, string name)
{
    const string element = "\"" + name + "\"";
    size_t pos = event.find(element);
    if (pos == string::npos ) {
        return pos;
    }
    return event.find(colon, pos + element.length());
}

/**
* Return the event tail starting at the character after preceeding
* @param event message
* @param preceeding - length to consume
*
* @return string - tail of the input event
*/
static string consume_preceeding(string event, size_t preceeding)
{
    string trailing = event.substr(preceeding);
    return trim(trailing);
}

/**
* parse the active client cm_request message populating active_clients
* @param event - active events beginning with colon after cm_request
*/
static bool process_active(string event)
{
    int active_sensors = 0;

    string body;
    event = get_body(event, body);
    if (body == "") {
        failed("missing cm_request body");
        return false;
    }

    body = body.substr(1, body.length() -2); // strip '[' and ']'
    body = trim(body);

    // body {...},{...}...
    // block {...}
    string block;
    while (body.length() > 0) {
        body = trim(body);
        if (comma == body.substr(0,1)) {
            body = consume_preceeding(body,1); // strip comma
        }

        if (left_brace == body.substr(0,1)) {
            body = get_block(body, block);
            block = trim(block);
            block = block.substr(1, block.length() -2); // strip '{' and '}'

            active_req req = active_req();
            get_value(block, "timestamp", req.timestamp);
            get_value(block, "suid_low", req.suid_low);
            get_value(block, "suid_high", req.suid_high);
            get_value(block, "msg_id", req.msg_id);
            get_value(block, "client_proc_type", req.client_proc_type);
            get_value(block, "delivery_type", req.delivery_type);
            get_value(block, "data_type", req.data_type);
            get_value(block, "register_updates", req.register_updates);
            get_value(block, "default_only", req.default_only);
            get_value(block, "sample_rate", req.sample_rate);

            uint64_t suid_low = convert_to_uint64_t(req.suid_low);
            uint64_t suid_high = convert_to_uint64_t(req.suid_high);
            req.suid.low = suid_low;
            req.suid.high = suid_high;
            show_active(req);
            active_sensors++;
        }
    }
    return active_sensors > 0;
}

static const string legend(
  "   Request LogTS\n"
  " (19.2MHz ticks)           Request Sensor                       Request SUID  MSGD_ID Proc  Delivery Request arguments ...\n"
  "---------------- ------------------------ ---------------------------------- -------- ---- --------- ---------------------\n");
// 1234567890123456 123456789012345678901234 1234567890123456789012345678901234 12345678 1234 123456789
static void show_legend() {cout << legend;}
static void show_active(active_req req)
{
    string datatype = "unknown - not found";
    if ( suid_sensor.low == req.suid.low
        && suid_sensor.high == req.suid.high) {
        datatype = "suid";
    }
#ifndef SIMULATION
    else if ( _psalt != nullptr) {
        sensor *psensor = _psalt->get_sensor(&(req.suid));
        if (psensor != nullptr && psensor->has_type()) {
            datatype = psensor->get_type();
        }
    }
#endif

    string suid = get_display_suid(req.suid);
    string proc = "";
    string delivery = "";

    size_t pos = req.client_proc_type.find_last_of('_');
    if ( pos != string::npos) {
        proc = req.client_proc_type.substr(pos + 1); // strip underscore
    }
    pos = req.delivery_type.find_last_of('_');
    if ( pos != string::npos) {
        delivery = req.delivery_type.substr(pos + 1); // strip underscore
    }

    ostringstream oss;
    oss << setfill(' ')
        << setw(16) << req.timestamp << " "
        << setw(24) << datatype << " "
        << setw(34) << suid << " "
        << setw(8)  << req.msg_id << " "
        << setw(4)  << proc << " "
        << setw(9)  << delivery << " ";

    if ( req.data_type != "") {
        oss << "data_type=" << req.data_type;
    }
    if ( req.register_updates != "") {
        oss << ", register_updates=" << req.register_updates;
    }
    if ( req.default_only != "") {
        oss << ", default_only=" << req.default_only;
    }
    if ( req.sample_rate != "") {
        oss << "sample_rate=" << req.sample_rate;
    }

    cout << oss.str() << endl;
}

// body [{...},{...}...]
// block {...}
/**
*  Get the body of the usually compound name element often
*  starting with '[' or '{'
* @param event - message starting with the colon after \"name\"
* @param body - gets the json string value of the element
*
* @return event message trailing the body
*/
static string get_body(string event, string &body)
{
    body = "";
    event = trim(event);
    if (colon != event.substr(0,1)) {
        failed("get_body expecting ':' but got '" + shorten(event) + "'");
        return "";
    }
    event = consume_preceeding(event,1); // strip off colon
    return get_block(event, body);
}

/**
* set block to the value of the event
* @param event - message positioned to first char of value
* @param block - gets json string value
* @return string - message string trailing the block
*/
static string get_block(string event, string &block)
{
    size_t close_block;

    if ( event.substr(0, 1) == left_brace ) {
        close_block = scan(event, 0, right_brace);
        if (close_block != string::npos) {
            block = event.substr(0, close_block + 1); // leave braces
            return event.substr(close_block + 1);
        }
    }
    else if (event.substr(0,1) == left_bracket) {
        close_block = scan(event, 0, right_bracket);
        if (close_block != string::npos) {
            block = event.substr(0, close_block + 1); // leave brackets
            return event.substr(close_block + 1);
        }
    }
    else if (event.substr(0,1) == quote) {
        close_block = scan(event, 0, quote);
        if (close_block != string::npos) {
            block = event.substr(1, close_block - 1); // strip quotes
            return event.substr(close_block + 1);
        }
    }
    else {
        close_block = event.find_first_of(comma_nl);
        if (close_block != string::npos) {
            block = event.substr(0, close_block);
            return event.substr(close_block + 1);
        }
        else {
            block = event;
            return "";
        }
    }
    return ""; // error
}

/**
* Scan for the delimiter honoring brace and bracket nesting
* @param event
* @param pos - offset of left delimiter
* @param delimiter - right delimiter
* @return size_t - offset of delimiter
*/
static size_t scan(string event, size_t pos, string delimiter)
{
    for (pos++; pos < event.length(); pos++) {
        string str = event.substr(pos,1);
        if (str == delimiter) {
            return pos;
        }
        if (str == left_brace) {
            pos = scan(event, pos, right_brace);
            if ( pos == string::npos ) {
                return pos;
            }
        }
        if (str == left_bracket ) {
            pos = scan(event, pos, right_bracket);
            if ( pos == string::npos ) {
                return pos;
            }
        }
    }
    return string::npos; // not found.
}

/**
* get leaf values for element name in block
* @param block - block of elements
* @param name - element name
* @param value - gets "" or element's value
* @return bool - true for element found else false
*/
static bool get_value(string block, string name, string &value)
{
    value = "";
    size_t pos = find_element(block, name);
    if (pos == string::npos) {
        return false;
     }

     block = consume_preceeding(block, pos);
     block = get_body(block, value);
     if (value == "") {
         failed( "missing block " + name + " value");
         return false;
      }
      return true;
}

// convert the suid half from asciihex string to uint64_t
static uint64_t convert_to_uint64_t(string half_suid)
{
    uint64_t x;

    std::stringstream ss;
    ss << std::hex << half_suid.substr(2); // strip the leading 0x
    ss >> x;
    return x;
}

/**
* returns displayable suid string
* @param suid
* @return string
*/
static string get_display_suid(sens_uid &suid)
{
    ostringstream sss;
    sss << "0x"
        << hex << setfill('0')
        << setw(16) << suid.high
        << setw(16) << suid.low;
    return sss.str();
}

static void failed(string reason)
{
    cout << "FAIL " << reason << endl;
}

static string shorten(string event)
{
    if (event.length() > 64) {
        return event.substr(0,64);
    }
    return event;
}

/* remove trailing whitespace */
static string rtrim(const string& str)
{
   const auto str_end = str.find_last_not_of (whitespace);
   return (str_end == string::npos) ? "" : str.substr(0, str_end + 1);
}
/* remove leading whitespace */
static string ltrim(const string& str)
{
   const auto str_begin = str.find_first_not_of(whitespace);
   return (str_begin == string::npos) ? "" : str.substr(str_begin);
}
/* remove leading and trailing whitespace */
static string trim(const string& str)
{
   return ltrim(rtrim(str));
}

