/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once

#include <functional>
#include <thread>

#include "sensor_attributes.h"
#include "sns_client.pb.h"
#include "suid_lookup.h"
#include "sensor.h"

using get_available_sensors_func =
    std::function<std::vector<std::unique_ptr<sensor>>()>;
/**
 * @brief class to provide hashing operation on suid to
 *        use suid as a key in unordered_map
 *
 */
struct suid_hash
{
    std::size_t operator()(const suid& sensor_uid) const
    {
        std::string data(reinterpret_cast<const char*>(&sensor_uid), sizeof(suid));
        return std::hash<std::string>()(data);
    }
};

class sensor_discovery
{
public:
    sensor_discovery(int hub_id);
    /* thread function to be executed by threads */
    void start_sensor_discovery();

    void set_discovery_thread(std::thread&& val) { discovery_thread = std::move(val);}
     std::thread& get_discovery_thread() { return discovery_thread;}

    /* get map of sensor suids for each datatype */
    const std::unordered_map<std::string, std::vector<suid>>&  get_suid_map() const { return _suid_map;}

    /* get map of default sensor suid for each datatype */
    const std::unordered_map<std::string, std::vector<suid>>& get_default_suids_container() const { return _default_suids_container;}

    /* get map of sensor attributes for each suid */
    const std::unordered_map<suid, sensor_attributes, suid_hash>& get_attributes() const { return _attributes;}

    const int& get_hub_id() const { return _hub_id;}

    static std::vector<std::string> get_mandatory_sensors() { return _mandatory_sensor_datatypes;}

private:
    /* discover available suids from static list of datatypes */
    void discover_sensors();

    /* query and save attributes for all available suids */
    void retrieve_attributes();

    sns_client_request_msg create_attr_request(suid sensor_uid);

    void handle_attribute_event(
                    suid sensor_uid, const sns_client_event_msg_sns_client_event& pb_event);

    /* to check registry sensor up in expected time */
    bool is_registry_up();

    /* callback function to handle registry suid event to check registry up */
    void registry_lookup_callback(
                    const std::string& datatype, const std::vector<suid>& suids);

    uint32_t get_discovery_timeout_ms();

    /* waits for a longer amount of time for some critical sensor types to
    become available */
    void wait_for_mandatory_sensors(suid_lookup& lookup);

    /* callback function to handle suid events */
    void suid_lookup_callback(const std::string& datatype,
                              const std::vector<suid>& suids);

    /**
     * @brief is used to update the mandatory list if new sensor found
     *
     * @param datatype: sensor name
     */
    void update_mandatory_list_database(std::string datatype);

    /* waits for a longer amount of time for in first boot if sensor list is 0
     * if suid sensor is not available then none of the sensors are
     * available so wait more time for first boot
     */
    void wait_for_sensors(suid_lookup& lookup);

    /* init the mandatory lit from sensors_list.txt */
    void init_mandatory_list_db();

    std::mutex _wait_mutex;

    /* condition variable to unblock registry_up method once registry available */
    std::condition_variable _wait_cond;
    bool _registry_available;
    /* _laterboot is used to convery whether current bootup is first bootup or not */
    static bool _laterboot;
    static bool _is_mandatory_db_list_init;
    /* pending attribute requests */
    uint32_t _pending_attributes;

    /* map of non-default sensor suids for each data type */
    std::unordered_map<std::string, std::vector<suid>> _all_suids_container;

    /* mutex and condition variables to synchronize attribute query */
    std::mutex _attr_mutex;

    std::chrono::steady_clock::time_point _tp_last_suid;

    std::unique_ptr<sessionFactory> _session_discovery_instance;
    static std::mutex _mandatory_sensors_mutex;
    static size_t _num_mandatory_sensors_found;
    static std::atomic<bool> _all_mandatory_sensors_found;
    static std::vector<std::string> _mandatory_sensor_datatypes;
    static std::map<std::string, bool> _mandatory_sensors_map;

    std::thread discovery_thread;
    /* map of sensor suids for each datatype */
    std::unordered_map<std::string, std::vector<suid>> _suid_map;
    /* map of default sensor suid for each datatype */
    std::unordered_map<std::string, std::vector<suid>> _default_suids_container;
    /* map of sensor attributes for each suid */
    std::unordered_map<suid, sensor_attributes, suid_hash> _attributes;
    int _hub_id;

};
