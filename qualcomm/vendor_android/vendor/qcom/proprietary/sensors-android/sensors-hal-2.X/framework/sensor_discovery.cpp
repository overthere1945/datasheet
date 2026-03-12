/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <chrono>

#include "sensor_factory.h"
#include "sensor_discovery.h"

#define MSEC_PER_SEC 1000
/* timeout duration for discovery of qsh sensors for first bootup */
#define SENSOR_DISCOVERY_TIMEOUT_FIRST 7
/* timeout duration for discovery of qsh sensors for all bootups */
#define SENSOR_DISCOVERY_TIMEOUT_REST  1
/* additional wait time for discovery of critical sensors */
#define MANDATORY_SENSOR_WAIT_TIME_SEC 8

/* wait for registry sensor up */
#define REGISTRY_UP_WAITTIME_SEC 1
#define REGISTRY_RETRY_CNT 20
#define NSEC_PER_SEC 1000000000ull
/* timeout duration for receiving attributes for a sensor */
#define SENSOR_GET_ATTR_TIMEOUT_NS 3000000000
#define GET_ATTR_NUM_ITERS 100
#define MANDATORY_SENSORS_LIST_FILE "/mnt/vendor/persist/sensors/sensors_list.txt"

using namespace std;

vector<string> sensor_discovery::_mandatory_sensor_datatypes;
map<string, bool> sensor_discovery::_mandatory_sensors_map;
size_t sensor_discovery::_num_mandatory_sensors_found = 0;
atomic<bool> sensor_discovery::_all_mandatory_sensors_found = false;
mutex sensor_discovery::_mandatory_sensors_mutex;
bool sensor_discovery::_laterboot = true;
bool sensor_discovery::_is_mandatory_db_list_init = false;

void sensor_discovery::init_mandatory_list_db()
{
    struct stat buf;
    ifstream _read;
    ofstream _write;
    _laterboot = (stat(MANDATORY_SENSORS_LIST_FILE, &buf) == 0);

    if (!_laterboot) {
        sns_logi("first boot after flash");
        _write.open(MANDATORY_SENSORS_LIST_FILE, ofstream::out);
        if (_write.fail()) {
            sns_loge("create fail for sensors_list with errno %s ",
            strerror(errno));
            return;
        }
    } else {
       _read.open(MANDATORY_SENSORS_LIST_FILE, ofstream::in);
        if (_read.fail()) {
           sns_loge("open read fail for sensors_list reading with errno %s ",
           strerror(errno));
           return;
       }

       string str;
       while (getline(_read, str)){
           auto it = find(_mandatory_sensor_datatypes.begin() , _mandatory_sensor_datatypes.end() , str);
           if(it == _mandatory_sensor_datatypes.end()) {
               _mandatory_sensor_datatypes.push_back(str);
           } else {
               sns_logi("duplicate entry for %s", str.c_str());
           }
       }
       _read.close();
       _write.open(MANDATORY_SENSORS_LIST_FILE, ofstream::app);
        if (_write.fail()) {
           sns_loge("open append fail for sensors_list with errno %s ",
            strerror(errno));
           return;
        }
        for (const string& s : _mandatory_sensor_datatypes) {
            _mandatory_sensors_map.emplace(s, 0);
        }
    }
    _write.close();
    _is_mandatory_db_list_init = true;
}

uint32_t sensor_discovery::get_discovery_timeout_ms()
{
    if (_laterboot) {
        return SENSOR_DISCOVERY_TIMEOUT_REST * MSEC_PER_SEC;
    } else {
        return SENSOR_DISCOVERY_TIMEOUT_FIRST * MSEC_PER_SEC;
    }
}

void sensor_discovery::update_mandatory_list_database(string datatype)
{
    if(_laterboot){
        sns_logi("laterboot datatype %s write", datatype.c_str());
    } else {
            _num_mandatory_sensors_found++;
    }
    if((find(_mandatory_sensor_datatypes.begin() , _mandatory_sensor_datatypes.end(), datatype) ==
                                                                               _mandatory_sensor_datatypes.end())){
        _mandatory_sensor_datatypes.push_back(datatype);
    }
}

void sensor_discovery::registry_lookup_callback
            (const string& datatype, const vector<suid>& suids)
{
    lock_guard<mutex> lk(_wait_mutex);
    if (suids.size() > 0) {
        _registry_available = true;
        _wait_cond.notify_one();
        sns_logd("registry is available on hub id %d", _hub_id);
    }
}

bool sensor_discovery::is_registry_up()
{
#ifdef SNS_WEARABLES_TARGET
    /* No need to wait for registry suid for wearables target */
    sns_logd("Returning true for registry up for wearables");
    return true;
#endif
    using namespace std::chrono;
    struct timespec ts;
    chrono::steady_clock::time_point tp_start;
    chrono::steady_clock::time_point tp_end;

    int retry_attempt_num = REGISTRY_RETRY_CNT;
    sns_logd("starting is_registry_up in hub_id %d @ t=%fs", this->_hub_id,
               duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count());
    tp_start = steady_clock::now();
    for (; retry_attempt_num > 0; retry_attempt_num--) {
        sns_logd("registry discovery countdown: %d", retry_attempt_num);
        suid_lookup lookup([this](const string& datatype, const auto& suids)
        {
            this->registry_lookup_callback(datatype, suids);
        }, this->_hub_id);
        lookup.request_suid("registry", false);
        unique_lock<mutex> lk(_wait_mutex);
        _wait_cond.wait_for(lk, chrono::seconds(REGISTRY_UP_WAITTIME_SEC));
        lk.unlock();
        if (false == _registry_available)
          this_thread::sleep_for(chrono::milliseconds(250));
        else
          break;
    }
    tp_end = steady_clock::now();
    if (true == _registry_available)
    {
        sns_logi("registry_sensor availability in hub_id %d & time '%fsec' from serviceup(no of repeats:'%d') ", this->_hub_id,
                 duration_cast<duration<double>>(tp_end - tp_start).count(),
                 (REGISTRY_RETRY_CNT-retry_attempt_num));
    }
    else
    {
        sns_loge("registry_sensor unavailable in hub_id %d even after '%fsecs' of serviceup(no of repeats:'%d')", this->_hub_id,
                duration_cast<duration<double>>(tp_end - tp_start).count(),
                (REGISTRY_RETRY_CNT-retry_attempt_num));
    }

    return _registry_available;
}

void sensor_discovery::start_sensor_discovery() {
    if (nullptr != _session_discovery_instance && true == is_registry_up()) {
        /* find available sensors on qsh */
        discover_sensors();
        if (_suid_map.size() > 0) {
             retrieve_attributes();
        }
    } else {
        sns_loge("_session_factory_instance is null || registry sensor not ready on _hub_id %d", _hub_id);
    }
}

sensor_discovery::sensor_discovery(int hub_id)
{
    this->_hub_id = hub_id;
    _pending_attributes = 0;
    _registry_available = false;

    sns_logd("started sensor discovery in hub id:%d", hub_id);
    struct stat buf;
    _session_discovery_instance = make_unique<sessionFactory>();

    if(nullptr == _session_discovery_instance) {
      sns_loge("failed to create session discovery instance for hub_id %d", hub_id);
    }
    if(true != _is_mandatory_db_list_init) {
        init_mandatory_list_db();
    }
}

void sensor_discovery::suid_lookup_callback(const string& datatype,
                                          const vector<suid>& suids)
{
    using namespace std::chrono;

    if (suids.size() > 0) {
        lock_guard<mutex> lock(_mandatory_sensors_mutex);
        _suid_map[datatype] = suids;
        update_mandatory_list_database(datatype);
        if (_laterboot) {
            auto it = _mandatory_sensors_map.find(datatype);
            if (it != _mandatory_sensors_map.end() && it->second == false) {
                it->second = true;
                _num_mandatory_sensors_found++;
                if (_num_mandatory_sensors_found ==
                    _mandatory_sensor_datatypes.size()) {
                    _all_mandatory_sensors_found.store(true);
                }
            }
        }
        _tp_last_suid = steady_clock::now();
        sns_logv("received suid for dt=%s, t=%fs", datatype.c_str(),
                 duration_cast<duration<double>>(
                     _tp_last_suid.time_since_epoch()).count());
    }
}

void sensor_discovery::wait_for_sensors(suid_lookup& lookup)
{
    int count = MANDATORY_SENSOR_WAIT_TIME_SEC;
    while (count) {
        sns_logi("SEE not ready yet wait count %d", count);
        {
            for (auto& dt : sensor_factory::datatypes()) {
                if(_suid_map.find(dt) == _suid_map.end()) {
                    sns_logd("re-sending request for %s", dt.c_str());
                    lookup.request_suid(dt, true);
                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(2*MSEC_PER_SEC));
        count--;
        {
            lock_guard<mutex> lock(_mandatory_sensors_mutex);
            if (_num_mandatory_sensors_found) {
                sns_logi("sensors found after re discover %d", count);
                break;
            }
            if ((count == 0) && (_num_mandatory_sensors_found == 0)) {
                sns_loge("sensors list is 0 after first boot ");
                break;
            }
        }
    }
}

void sensor_discovery::wait_for_mandatory_sensors(suid_lookup& lookup)
{
    int count = MANDATORY_SENSOR_WAIT_TIME_SEC;
    while (_all_mandatory_sensors_found.load() == false) {
        sns_logi("some mandatory sensors not available yet, "
                 "will wait for %d seconds...", count);
        {
            unique_lock<mutex> guard(_mandatory_sensors_mutex);
            for (auto p : _mandatory_sensors_map) {
                if (p.second == false) {
                    sns_logd("non available ones %s", p.first.c_str());
                }
            }
            guard.unlock();
            for (auto& dt : sensor_factory::datatypes()) {
                if(_suid_map.find(dt) == _suid_map.end()) {
                    sns_logd("re-sending request for %s", dt.c_str());
                    lookup.request_suid(dt, true);
                }
            }
        }

        this_thread::sleep_for(chrono::milliseconds(MSEC_PER_SEC));
        count--;

        {
            lock_guard<mutex> lock(_mandatory_sensors_mutex);
            if (count == 0 && _all_mandatory_sensors_found.load() == false) {
                sns_loge("some mandatory sensors not available even after %d "
                         "seconds, giving up.", MANDATORY_SENSOR_WAIT_TIME_SEC);
                sns_loge("%lu missing sensor(s)", (unsigned long)
                         _mandatory_sensor_datatypes.size() - _num_mandatory_sensors_found);
                for (auto p : _mandatory_sensors_map) {
                    if (p.second == false) {
                        sns_loge("    %s", p.first.c_str());
                    }
                }
                break;
            }
        }
    }
}

void sensor_discovery::discover_sensors()
{
    using namespace std::chrono;
    sns_logd("discovery start t=%fs", duration_cast<duration<double>>(
             steady_clock::now().time_since_epoch()).count());

    suid_lookup lookup([this](const string& datatype, const auto& suids)
        {
            this->suid_lookup_callback(datatype, suids);
        }, this->_hub_id);

    sns_logd("discovering available sensors in hub_id %d", this->_hub_id);

    for (const string& dt : sensor_factory::datatypes()) {
        sns_logd("requesting %s", dt.c_str());
        lookup.request_suid(dt, true);
    }
    auto tp_wait_start = steady_clock::now();

    /* wait for some time for discovery of available sensors */
    auto delay = get_discovery_timeout_ms();

    sns_logv("before sleep, now=%f", duration_cast<duration<double>>(
             steady_clock::now().time_since_epoch()).count());

    sns_logi("waiting for suids for %us ...", (delay/MSEC_PER_SEC));

    this_thread::sleep_for(chrono::milliseconds(delay));

    sns_logv("after sleep, now=%f", duration_cast<duration<double>>(
             steady_clock::now().time_since_epoch()).count());

    /* additional wait for discovery of critical sensors */
    if (_laterboot) {
        wait_for_mandatory_sensors(lookup);
    } else if (0 == _num_mandatory_sensors_found) {
        sns_loge("first boot generated 0 sensors so re-discover");
        wait_for_sensors(lookup);
    }
    sns_logd("available sensors on hub_id %d", this->_hub_id);
    for(auto item : _suid_map){
        sns_logd("%-20s%4u", item.first.c_str(), (unsigned int)item.second.size());
    }


    sns_logi("suid discovery time = %fs",
             duration_cast<duration<double>>(_tp_last_suid - tp_wait_start)
             .count());
}

sns_client_request_msg sensor_discovery::create_attr_request(suid sensor_uid)
{
    sns_client_request_msg pb_req_msg;
    /* populate the client request message */
    pb_req_msg.set_msg_id(SNS_STD_MSGID_SNS_STD_ATTR_REQ);
    pb_req_msg.mutable_request()->clear_payload();
    pb_req_msg.mutable_suid()->set_suid_high(sensor_uid.high);
    pb_req_msg.mutable_suid()->set_suid_low(sensor_uid.low);
    pb_req_msg.mutable_susp_config()->set_delivery_type(SNS_CLIENT_DELIVERY_WAKEUP);
    pb_req_msg.mutable_susp_config()->set_client_proc_type(SNS_STD_CLIENT_PROCESSOR_APSS);
    pb_req_msg.set_client_tech(SNS_TECH_SENSORS);
    return pb_req_msg;
}

void sensor_discovery::retrieve_attributes()
{
    sns_logd("retrieving attributes in hub_id %d", this->_hub_id);
    ISession::eventCallBack event_cb =[this](const uint8_t* msg , int msgLength, uint64_t time_stamp)
    {
        sns_client_event_msg pb_event_msg;
        pb_event_msg.ParseFromArray(msg, msgLength);
        for (int i=0; i < pb_event_msg.events_size(); i++) {
            auto&& pb_event = pb_event_msg.events(i);
            sns_logv("event[%d] msg_id=%d", i, pb_event.msg_id());
            if (pb_event.msg_id() ==  SNS_STD_MSGID_SNS_STD_ATTR_EVENT) {
              suid sensor_uid (pb_event_msg.suid().suid_low(),
                                 pb_event_msg.suid().suid_high());
                handle_attribute_event(sensor_uid, pb_event);
            }
        }
    };
    unique_ptr<ISession> session = unique_ptr<ISession>(_session_discovery_instance->getSession(this->_hub_id));
    if(nullptr == session) {
        sns_loge("failed to create ISession instance");
        return;
    }

    int ret = session->open();
    if(ret == -1) {
      sns_loge("error while opening the session ");
      session.reset();
      session = nullptr;
      return;
    }

    /* send attribute request for all suids */
    for (const auto& item : _suid_map) {
        const auto& datatype = item.first;
        const auto& suids = item.second;
        int count=0;
        for (const suid& sensor_uid : suids) {
            _attr_mutex.lock();
            _pending_attributes++;
            _attr_mutex.unlock();
            string pb_attr_req_encoded;
            sns_logd("requesting attributes for %s-%d", datatype.c_str(),
                     count++);
            create_attr_request(sensor_uid).SerializeToString(&pb_attr_req_encoded);
            int ret = session->setCallBacks(sensor_uid,nullptr,nullptr,event_cb);
             if(0 != ret)
                sns_loge("all callbacks are null, not registered any callback for suid(low=%" PRIu64 ",high=%)" PRIu64, sensor_uid.low, sensor_uid.high);

            sns_logd("%s before send_request for with SUID low=%llu high=%llu", __func__, (unsigned long long)sensor_uid.low, (unsigned long long)sensor_uid.high);
            ret = session->sendRequest(sensor_uid, pb_attr_req_encoded);
            if(0 != ret)
                sns_loge("Error in sending request");
        }
    }

    sns_logi("waiting for All attribute event");

    /* wait until attributes are received */
    int count = GET_ATTR_NUM_ITERS;
    uint64_t delay = SENSOR_GET_ATTR_TIMEOUT_NS / GET_ATTR_NUM_ITERS;
    struct timespec ts;
    int err;
    ts.tv_sec = delay / NSEC_PER_SEC;
    ts.tv_nsec = delay % NSEC_PER_SEC;

    unique_lock<mutex> lk(_attr_mutex);
    while (count > 0) {
        if (_pending_attributes == 0) {
            break;
        }
        /* unlock the mutex while sleeping */
        lk.unlock();
        do {
            err = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
        } while (err == EINTR);
        lk.lock();
        if (err != 0) {
            sns_loge("clock_nanosleep() failed, err=%d, count=%d", err, count);
        }
        count--;
    }
    if (_pending_attributes > 0) {
        sns_loge("timeout while waiting for attributes, pending = %d, err = %d",
              _pending_attributes, err);
        return;
    }

    if(nullptr != session){
      session->close();
      session.reset();
      session = nullptr;
    }

    sns_logi("attributes received in hub_id %d", this->_hub_id);
}

void sensor_discovery::handle_attribute_event(suid sensor_uid,
    const sns_client_event_msg_sns_client_event& pb_event)
{
    sns_std_attr_event pb_attr_event;
    pb_attr_event.ParseFromString(pb_event.payload());

    sensor_attributes attr;

    for (int i=0; i<pb_attr_event.attributes_size(); i++) {
        attr.set(pb_attr_event.attributes(i).attr_id(),
                       pb_attr_event.attributes(i).value());
    }
    _attributes[sensor_uid] = attr;

    /* notify the thread waiting for attributes */
    unique_lock<mutex> lock(_attr_mutex);
    if (_pending_attributes > 0) {
        _pending_attributes--;
    }
}
