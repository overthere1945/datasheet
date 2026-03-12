/*=============================================================================
  @file qsh_calibrate.cpp

  Sensorcal module: using libqsh function to send requst SUIDS for given data
  type and do a test for given test type.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include "qsh_calibrate.h"

using namespace google::protobuf::io;
using namespace std;


/*=============================================================================
  Static Data
  ===========================================================================*/

static const auto SENSOR_GET_TIMEOUT = 15000ms;
static const auto WAIT_RESULT_TIMEOUT = 16s;

/*=============================================================================
  Function Definitions
  ===========================================================================*/
qsh_calibrate::qsh_calibrate()
{
    sensors_log::set_tag("qsh_calibrate");
    sensors_log::set_level(sensors_log::VERBOSE);
    sensors_log::set_stderr_logging(true);
    _pending_attrs = 0;
    _pending_test = 0;

    _session_factory_instance = make_unique<sessionFactory>();
    if(nullptr == _session_factory_instance){
      printf("failed to create factory instance");
    }

}

qsh_calibrate::~qsh_calibrate() {

}

const std::string qsh_calibrate::get_string(suid sensor_uid, int32_t attr_id)
{
  string attrib_string = "";
  auto it = _attributes_map.find(sensor_uid);
  if (it == _attributes_map.end()) {
    sns_loge("suid is not available ");
  } else {
    const sensor_attributes& attrs_t = it->second;
    if (attrs_t.num_attributes <= 0) {
        sns_loge("attribute value is invalid");
        return attrib_string;
    }
    auto attr_it = attrs_t._attr_map.find(attr_id);
    if (attr_it ==attrs_t._attr_map.end()) {
        sns_loge("attribute value is not found for given attr_id");
        return attrib_string;
    }
    const sns_std_attr_value& attr_value = attr_it->second;
    if (attr_value.values_size() > 0 && attr_value.values(0).has_str()) {
        return attr_value.values(0).str();
    }
  }
  return attrib_string;
}

vector<float> qsh_calibrate::get_floats(suid sensor_uid, int32_t attr_id)
{
    const sensor_attributes& attrs_t = _attributes_map.at(sensor_uid);
    auto it = attrs_t._attr_map.find(attr_id);
    if (it == attrs_t._attr_map.end()) {
        return vector<float>();
    }
    const sns_std_attr_value& attr_value = it->second;
    vector<float> floats(attr_value.values_size());
    for (int i=0; i < attr_value.values_size(); i++) {
        if (!attr_value.values(i).has_flt()) {
            sns_loge("attribute :%d and element %d is not a float", attr_id, i);
        }
        floats[i] = attr_value.values(i).flt();
    }
    return floats;
}

vector<int64_t> qsh_calibrate::get_ints(suid sensor_uid, int32_t attr_id)
{
    const sensor_attributes& attrs_t = _attributes_map.at(sensor_uid);
    auto it = attrs_t._attr_map.find(attr_id);
    if (it == attrs_t._attr_map.end()) {
        return vector<int64_t>();
    }
    const sns_std_attr_value& attr_value = it->second;
    vector<int64_t> ints(attr_value.values_size());
    for (int i=0; i < attr_value.values_size(); i++) {
        if (!attr_value.values(i).has_sint()) {
            sns_loge("attribute :%d and element %d is not an int", attr_id, i);
        }
        ints[i] = attr_value.values(i).sint();
    }
    return ints;
}

vector<pair<float, float>> qsh_calibrate::get_ranges(suid sensor_uid)
{
    const sensor_attributes& attrs_t = _attributes_map.at(sensor_uid);
    auto it = attrs_t._attr_map.find(SNS_STD_SENSOR_ATTRID_RANGES);
    if (it == attrs_t._attr_map.end()) {
        return vector<pair<float, float>>();
    }
    const sns_std_attr_value& attr_value = it->second;
    vector<pair<float, float>> ranges(attr_value.values_size());
    for (int i=0; i < attr_value.values_size(); i++) {
        if (attr_value.values(i).has_subtype() &&
            attr_value.values(i).subtype().values_size() > 1 &&
            attr_value.values(i).subtype().values(0).has_flt() &&
            attr_value.values(i).subtype().values(1).has_flt())
        {
            ranges[i].first = attr_value.values(i).subtype().values(0).flt();
            ranges[i].second = attr_value.values(i).subtype().values(1).flt();
        } else {
            sns_loge("malformed value for SNS_STD_SENSOR_ATTRID_RANGES");
        }
    }
    return ranges;
}


int64_t qsh_calibrate::get_stream_type(suid sensor_uid)
{
    auto ints = get_ints(sensor_uid, SNS_STD_SENSOR_ATTRID_STREAM_TYPE);
    if (ints.size() == 0) {
        sns_loge("stream_type attribute not available");
        return -1;
    }
    return ints[0];
}

std::string qsh_calibrate::suid_to_string(suid sensor_uid)
{
   std::string suid_str;
   std::string str = to_string((unsigned long)sensor_uid.high);
   suid_str.append(str, 0, 4);
   return suid_str;
}


std::vector<suid> qsh_calibrate::get_all_available_suids(std::string data_type, uint8_t test_type)
{
    std::vector<suid> avalib_suids;
    std::vector<suid> sensor_uids = _sensor_suid_map.at(data_type);

    if((data_type == "pressure" || data_type == "humidity" || data_type == "mag")
        && test_type == SNS_PHYSICAL_SENSOR_TEST_TYPE_FACTORY) {
           return vector<suid>();
    } else {

        if(sensor_uids.size() == 1)
            return sensor_uids;

        if(sensor_uids.size() == 2)
        {
            std::vector<suid>::iterator suid_1 = sensor_uids.begin();
            std::vector<suid>::iterator suid_2 = sensor_uids.end()-1;
            if(get_sensor_name(*suid_1) == get_sensor_name(*suid_2) &&
               get_stream_type(*suid_1) != get_stream_type(*suid_2)) {
              if(get_stream_type(*suid_1)==SNS_STD_SENSOR_STREAM_TYPE_STREAMING)
                avalib_suids.push_back(*suid_2);
              else
                avalib_suids.push_back(*suid_1);
                return avalib_suids;
            }

            avalib_suids.push_back(*suid_1);
            avalib_suids.push_back(*suid_2);
            return avalib_suids;
        }
        return vector<suid>();
    }

}


std::string qsh_calibrate::get_sensor_name(suid sensor_uid)
{
    std::string sensor_name;
    std::string tmp;

    tmp = get_string(sensor_uid, SNS_STD_SENSOR_ATTRID_NAME);
    if (tmp.empty()) {
          sns_loge("name attribute not available");
          return "error";
    }
    sensor_name.append(tmp, 0, tmp.size()-1);
    return sensor_name;
}

std::string qsh_calibrate::get_invalid_result(std::string data_type)
{
    std::vector<suid> sensor_uids = _sensor_suid_map.at(data_type);
    std::string sensor_name;
    std::string sensor_name1;
    std::string sensor_name2;

    if(sensor_uids.size() == 1) {
        sensor_name = get_sensor_name(*sensor_uids.begin());
        return sensor_name + ":" + "-1" + ";";
    }
    /* suppose only support at most two suids for given data_type */
    if(sensor_uids.size() == 2) {
        std::vector<suid>::iterator suid_1 = sensor_uids.begin();
        std::vector<suid>::iterator suid_2 = sensor_uids.end()-1;
        sensor_name1 = get_sensor_name(*suid_1);
        sensor_name2 = get_sensor_name(*suid_2);
        if(sensor_name1 != sensor_name2) {
            return sensor_name1 + ":" + "-1" + ";" + sensor_name2 + ":" + "-1" + ";";
        }
        return sensor_name1 + ":" + "-1" + ";";
    }

    return "not support more than two suids;";
}

std::string qsh_calibrate::get_test_result(suid sensor_uid)
{
    auto it = _sensortest_map.find(sensor_uid);
    if (it != _sensortest_map.end()) {
        return it->second;
    } else {
        return "-1";
    }
}


std::string qsh_calibrate::test_result_list(std::string data_type)
{
    std::string result, tmp;
    std::string sensor_name;

    int num = 0;
    for (const suid& sensor_uid : avaib_suids) {
        sns_logd("test result for %s-%d", data_type.c_str(), num);
        num++;
        sensor_name = get_sensor_name(sensor_uid);
        /* distinguish between two sensors that have same sensor name */
        tmp = suid_to_string(sensor_uid);
        result.append(sensor_name, 0, sensor_name.length());
        const std::string& result_tmp = get_test_result(sensor_uid);
        result = result + "-" + tmp + ":" + result_tmp + ";";

    }
    return result;
}

/**
 * Event callback function.
 */
void qsh_calibrate::handle_physical_test_event(suid sensor_uid,
    const sns_client_event_msg_sns_client_event& pb_event)
{
    sns_physical_sensor_test_event test_event;
    test_event.ParseFromString(pb_event.payload());

    std::string sensor_name = get_sensor_name(sensor_uid);
    std::string test_status = std::to_string(test_event.test_passed());
    sns_logd("sensor name is %s,test result is %d, test type is %d",
          sensor_name.c_str(), test_event.test_passed(), test_event.test_type());
    _sensortest_map[sensor_uid] = test_status;

    /* notify the thread waiting for test result */
    unique_lock<mutex> test_lock(_test_mutex);
    _pending_test--;
    if (_pending_test == 0) {
        _cv_phy_test.notify_one();
    }

}


void qsh_calibrate::handle_attribute_event(suid sensor_uid,
    const sns_client_event_msg_sns_client_event& pb_event)
{
    sns_std_attr_event pb_attr_event;
    sns_std_attr_value attr_value;
    pb_attr_event.ParseFromString(pb_event.payload());

    attrs.num_attributes = pb_attr_event.attributes_size();
    for (int i=0; i<pb_attr_event.attributes_size(); i++) {
           sns_logv("attr_id is %d", pb_attr_event.attributes(i).attr_id());
           attrs._attr_map[pb_attr_event.attributes(i).attr_id()] = pb_attr_event.attributes(i).value();
    }
    _attributes_map[sensor_uid] = attrs;

    /* notify the thread waiting for attributes */
    unique_lock<mutex> lock(_attr_mutex);
    _pending_attrs--;
    if (_pending_attrs == 0) {
        _cv_attr.notify_one();
    }
}


/**
 * Send a SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG
 * to sensor driver
 */
void qsh_calibrate::
send_selftest_req(ISession* session, suid sensor_uid, const uint8_t &test_type)
{
    string pb_req_msg_encoded;
    string config_encoded;
    sns_client_request_msg pb_req_msg;
    sns_physical_sensor_test_config config;

    config.set_test_type((sns_physical_sensor_test_type)test_type);
    config.SerializeToString(&config_encoded);

    pb_req_msg.set_msg_id(SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG);
    pb_req_msg.mutable_request()->set_payload(config_encoded);
    pb_req_msg.mutable_suid()->set_suid_high(sensor_uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(sensor_uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->
        set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pb_req_msg.set_client_tech(SNS_TECH_SENSORS);

    pb_req_msg.SerializeToString(&pb_req_msg_encoded);
    int ret = session->sendRequest(sensor_uid, pb_req_msg_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }
}

void qsh_calibrate::
send_attr_req(ISession* session, suid sensor_uid)
{
    string pb_req_msg_encoded;
    sns_client_request_msg pb_req_msg;
   /* populate the client request message */
    pb_req_msg.set_msg_id(SNS_STD_MSGID_SNS_STD_ATTR_REQ);
    pb_req_msg.mutable_suid()->set_suid_high(sensor_uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(sensor_uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pb_req_msg.set_client_tech(SNS_TECH_SENSORS);
    pb_req_msg.mutable_request()->clear_payload();

    pb_req_msg.SerializeToString(&pb_req_msg_encoded);
    int ret = session->sendRequest(sensor_uid, pb_req_msg_encoded);
    if(0 != ret){
        printf("Error in sending request");
        return;
    }
}

void qsh_calibrate::calibrate_event_cb(const uint8_t* msg, int msgLength, uint64_t time_stamp)
{
  sns_logd("event callback start with timestamp %" PRIu64, time_stamp);
  sns_client_event_msg pb_event_msg;
  pb_event_msg.ParseFromArray(msg, msgLength);
  suid sensor_uid (pb_event_msg.suid().suid_low(),
      pb_event_msg.suid().suid_high());
  for (int i=0; i < pb_event_msg.events_size(); i++) {
      auto&& pb_event = pb_event_msg.events(i);
      sns_logv("event[%d] msg_id=%d", i, pb_event.msg_id());
      if (pb_event.msg_id() ==  SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
          handle_attribute_event(sensor_uid, pb_event);
      }
      if (pb_event.msg_id() ==
              SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_EVENT)
          handle_physical_test_event(sensor_uid, pb_event);
  }
}

void qsh_calibrate::suid_cbk(const std::string& datatype,
                       const std::vector<suid>& suids) {

  sns_logv("Received SUID event with length %zu", suids.size());
  if(suids.size() > 0)
  {
    _sensor_suid_map[datatype] = suids;
    _event_cb =[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
                          { this->calibrate_event_cb(msg, msgLength, time_stamp); };

    if(nullptr == _session_factory_instance) {
      return;
    }

    ISession* qmiSession = _session_factory_instance->getSession();
    if(nullptr == qmiSession){
      printf("failed to create ISession");
      return;
    }

    /* open the suidSession */
    int ret = qmiSession->open();
    if(-1 == ret){
      printf("failed to open ISession ");
      return;
    }

    int count = 0;
    for (size_t i = 0; i < suids.size(); i++) {
      sns_logv("Received SUID %" PRIx64 "%" PRIx64 " for '%s'-%d",
          suids[i].high, suids[i].low, datatype.c_str(), count);
      count++;
      _pending_attrs++;
      int ret = qmiSession->setCallBacks(suids[i],nullptr,nullptr,_event_cb);
      if(0 != ret)
        printf("all callbacks are null, not registered any callback for SUID  %" PRIx64 "%" PRIx64, suids[i].high, suids[i].low);

      send_attr_req(qmiSession, suids[i]);
    }

    sns_logd("waiting for attributes...");
    /* wait until attributes are received */
    unique_lock<mutex> lock(_attr_mutex);
    while( _pending_attrs) {
      _cv_attr.wait(lock);
    }
    sns_logd("attributes received");

    avaib_suids = get_all_available_suids(datatype, _test_type);
    if(avaib_suids.size()) {
      count = 0;
      for (size_t i = 0; i < avaib_suids.size(); i++) {
        sns_logv("Received SUID %" PRIx64 "%" PRIx64 " for '%s'-%d",
            avaib_suids[i].high, avaib_suids[i].low, datatype.c_str(), count);
        count++;
        _pending_test++;
        send_selftest_req(qmiSession, avaib_suids[i], _test_type);
      }

      /* wait until test result are received */
      unique_lock<mutex> test_lock(_test_mutex);
      bool timeout = !_cv_phy_test.wait_for(test_lock, SENSOR_GET_TIMEOUT,
          [this] { return (_pending_test == 0); });
      if (timeout) {
        sns_loge("timeout while waiting for test result, pending = %d",
            _pending_test);
      } else {
        sns_logd("test result received");

        /* notify the thread waiting for test result */
        unique_lock<mutex> result_lock(_result_mutex);
        _cv_result.notify_one();
      }

      if(nullptr != qmiSession) {
        qmiSession->close();
        delete qmiSession;
        qmiSession = nullptr;
      }
    }
  } else {
    sns_loge("not found suids for given datatype");
    exit(1);
  }

}


std::string qsh_calibrate::
calibrate(const string& datatype, const uint8_t &test_type)
{
    std::string test;
    _test_type = test_type;

    suid_lookup lookup(
     [this](const string& datatype, const auto& suids)
     {
        this->suid_cbk(datatype, suids);
     });

    lookup.request_suid(datatype);

    unique_lock<mutex> result_lock(_result_mutex);
    if(_cv_result.wait_for(result_lock, std::chrono::seconds(WAIT_RESULT_TIMEOUT))
      == std::cv_status::no_timeout) {
      if(avaib_suids.size()){
        test = test_result_list(datatype);
        sns_logd("test result: %s", test.c_str());
        return test;
      }
    }
    test = get_invalid_result(datatype);
    sns_logd("test result fail: %s", test.c_str());
    return test;
}
