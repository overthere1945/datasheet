/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <cutils/properties.h>
#include <algorithm>
#include <thread>
#include <time.h>

#include "sensor_factory.h"

using namespace std;

#define MANDATORY_SENSORS_LIST_FILE "/mnt/vendor/persist/sensors/sensors_list.txt"
#define SENSING_HUB_1 1
#define SENSING_HUB_2 2

vector<int> sensor_factory::get_supported_hub_ids()
{
    return _supported_hub_ids;
}

int sensor_factory::get_default_hub_id()
{
#ifdef SNS_WEARABLES_TARGET
    return SENSING_HUB_2;
#else
    return SENSING_HUB_1;
#endif
}

sensor_factory::sensor_factory() {
    unique_ptr<sessionFactory> _session_factory_instance = make_unique<sessionFactory>();
    vector<int> hub_ids  =_session_factory_instance->getSensingHubIds();
    /* loop through all the hub ids & create sensor discovery handles */
    for(auto hub_id : hub_ids ) {
        sensor_discovery* discovery_handle  = new sensor_discovery(hub_id);
        discovery_handle ->set_discovery_thread(move(thread(&sensor_discovery::start_sensor_discovery, discovery_handle)));
        _hub_handles.push_back(discovery_handle);
        sns_logd("Created handle for sensing hub id %d", hub_id);
      }
    sns_logd("spawned all the threads for sensor discovery, waiting");
    /* join all the threads */
    for(auto handle : _hub_handles) {
        handle->get_discovery_thread().join();
    }
    sns_logd("All discovery threads have finished their tasks");
    sns_logd("Update mandatory sensor list file");
    ofstream _write;
    _write.open(MANDATORY_SENSORS_LIST_FILE, ofstream::out);
    if (_write.fail()) {
        sns_loge("open fail for mandatory sensors_list");
        return;
    }
    for(auto datatype : sensor_discovery::get_mandatory_sensors()) {
       _write << datatype + "\n";
    }
    _write.close();
    // Merge data from all the handles and delete the handles
    for(auto handle : _hub_handles) {
        if(handle->get_suid_map().size() > 0)
        {
            int hub_id = handle->get_hub_id();
            _supported_hub_ids.push_back(hub_id);
            sns_logd("Merging data in factory for hub id = %d", hub_id);
            this->_suid_map[hub_id].insert(handle->get_suid_map().begin(),handle->get_suid_map().end());
            this->_default_suids_container[hub_id].insert(handle->get_default_suids_container().begin(),handle->get_default_suids_container().end());
            this->_attributes.insert(handle->get_attributes().begin(),handle->get_attributes().end());
        }

        delete handle;
    }
    _hub_handles.clear();
    sns_logd("attribute size %d", _attributes.size());
    sns_logd("Merged suids and attributes of all the discovery threads");
}

vector<unique_ptr<sensor>> sensor_factory::get_all_available_sensors() const
{
    vector<unique_ptr<sensor>> all_sensors;
    for (const auto& item : callbacks()) {
        const auto& get_sensors = item.second;
        vector<unique_ptr<sensor>> sensors = get_sensors();
        if(sensors.size() == 0){
          continue;
        }
        sns_logd("type=%d, num_sensors=%u", item.first, (unsigned int)sensors.size());
        for (auto&& s : sensors) {
            if(nullptr != s ) {
              all_sensors.push_back(move(s));
            }
        }
    }
    return all_sensors;
}

int sensor_factory::get_pairedsuid(
                               const string& datatype,
                               const suid &master_suid,
                               suid &paired_suid, int hub_id)
{
    /* get the list of suids */
    if(-1 == hub_id) {
        hub_id = get_default_hub_id();
    }
    vector<suid>  suids = get_suids(datatype, hub_id);
    const string master_suid_vendor = _attributes[master_suid].get_string(SNS_STD_SENSOR_ATTRID_VENDOR);
    const string master_suid_name   = _attributes[master_suid].get_string(SNS_STD_SENSOR_ATTRID_NAME);
    const string master_type = _attributes[master_suid].get_string(SNS_STD_SENSOR_ATTRID_TYPE);
    int master_suid_hwid = -1;
    bool match = false;
    if (_attributes[master_suid].is_present(SNS_STD_SENSOR_ATTRID_HW_ID)) {
        master_suid_hwid = _attributes[master_suid].get_ints(SNS_STD_SENSOR_ATTRID_HW_ID)[0];
    }

    sns_logd("to be paired::datatype: %s, vendor: %s , name:%s, hwid %d",
                datatype.c_str(), master_suid_vendor.c_str(),
                master_suid_name.c_str(), master_suid_hwid);

    /* get attributes of specified data type suid & return if matches */
    for (vector<suid>::iterator it = suids.begin(); it != suids.end(); ++it) {
        sns_logv("for pairing.., parsed vendor:%s, name:%s ",
                    _attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_VENDOR).c_str(),
                    _attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_NAME).c_str());

        if ((_attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_VENDOR)
                 == master_suid_vendor ) &&
                 (_attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_NAME)
                 ==(master_suid_name))) {
            match = true;
            /*HW_ID is optional field so check if it is present only*/
            if ( master_suid_hwid != -1 && _attributes[*it].is_present(SNS_STD_SENSOR_ATTRID_HW_ID)){
                if ( master_suid_hwid != _attributes[*it].get_ints(SNS_STD_SENSOR_ATTRID_HW_ID)[0]) {
                    match = false;
                    continue;
                }
            }
            if( match == true) {
                paired_suid = *it;
                sns_logd("datatype: %s, paired with:type:%s,name:%s,vendor:%s",
                            datatype.c_str(), master_type.c_str(),
                            _attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_NAME).c_str(),
                            _attributes[*it].get_string(SNS_STD_SENSOR_ATTRID_VENDOR).c_str());
                break;
            }
        }
    }

    if(match == false) {
       sns_loge("no data type mapping b/w master/paired %s/%s",
                                        master_type.c_str(), datatype.c_str());
       return -1;
    }

    return 0;
}

const vector<suid>& sensor_factory::get_suids(const string& datatype, int hub_id)
{
    if(-1 == hub_id) {
        hub_id = get_default_hub_id();
    }
    auto it = _suid_map[hub_id].find(datatype);
    if (it != _suid_map.at(hub_id).end()) {
        return it->second;
    } else {
        static vector<suid> empty;
        return empty;
    }
}

