/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once

#include <sys/stat.h>
#include <condition_variable>
#include <unordered_map>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <time.h>
#include <string>
#include <vector>
#include <mutex>

#include "sensor_attributes.h"
#include "sensor_discovery.h"
#include "SessionFactory.h"
#include "sns_client.pb.h"
#include "sensors_log.h"
#include "suid_lookup.h"
#include "sensor.h"

/**
 * @brief singleton class for creating sensor objects of
 *        multiple types, querying information such as SUIDs,
 *        attributes. Classes that implement sensors are
 *        registered here.
 */
class sensor_factory
{
public:
    /**
     * @brief get handle to sensor_factory instance
     * @return sensor_factory&
     */
    static sensor_factory& instance()
    {
        static sensor_factory factory;
        return factory;
    }

    /**
     * @brief register a new sensor type to be supported
     *
     * @param type sensor type as defined in sensors.h
     * @param func factory function for creating sensors of this
     *             type
     *
     */
    static void register_sensor(int type, get_available_sensors_func func)
    {
        try {
            callbacks().emplace(type, func);
        } catch (const std::exception& e) {
            sns_loge("failed to register type %d", type);
        }
    }

    /**
     * @brief get list of suids for given datatype & hub_id
     * @param datatype & hub_id
     * @return const std::vector<suid>& list of suids
     */
    const std::vector<suid>& get_suids(const std::string& datatype, int hub_id = -1);

    /**
     * @brief get mapped/secondary sensor suid for a specified sensor type and hardware ID
     * @param suid and datatype
     * @return const sensor_suid
     */
     int get_pairedsuid( const std::string& datatype,
                                      const suid &paired_suid,
                                      suid &mapped_suid, int hub_id = -1);

    /**
     * @brief get sensor attributes for given suid
     * @param suid
     * @return const sensor_attributes&
     */
    const sensor_attributes& get_attributes(const suid& sensor_uid) const
    {
        auto it = _attributes.find(sensor_uid);
        if (it == _attributes.end()) {
            throw std::runtime_error("get_attributes() unrecognized suid");
        }
        return it->second;
    }
    /**
     * @brief request a datatype to be queried when factory is
     *        initialized
     *
     * @param datatype and default_only
     */
    static void request_datatype(const char *datatype)
    {
        try {
            datatypes().insert(std::string(datatype));
        } catch (const std::exception& e) {
            sns_loge("failed to insert %s", datatype);
        }
    }

    std::vector<std::unique_ptr<sensor>> get_all_available_sensors() const;

    /* set of all qsh sensor datatypes */
    static std::unordered_set<std::string>& datatypes()
    {
        static std::unordered_set<std::string> _datatypes;
        return _datatypes;
    }

    /* map of factory callback functions for each sensor type */
    static std::unordered_map<int, get_available_sensors_func>& callbacks()
    {
        static std::unordered_map<int, get_available_sensors_func> _callbacks;
        return _callbacks;
    }

    /* get list of all hub ids supported by the target */
    std::vector<int> get_supported_hub_ids();
    int get_default_hub_id();

private:
    /* singleton operation */
    sensor_factory();
    sensor_factory (const sensor_factory&) = delete;
    sensor_factory& operator= (const sensor_factory&) = delete;

    bool non_default_dual_sensor_discovery_allowed();

    /* map of sensor suids for each datatype */
    std::unordered_map<int, std::unordered_map<std::string, std::vector<suid>>> _suid_map;
    /* objects of sensors discovery class for each hub id supported */
    std::vector<sensor_discovery*> _hub_handles;
    /* map of sensor attributes for each suid */
    std::unordered_map<suid, sensor_attributes, suid_hash> _attributes;
    /* map of default sensor suid for each datatype */
    std::unordered_map<int, std::unordered_map<std::string, std::vector<suid>>> _default_suids_container;
    /* list of supported hub  ids */
    std::vector<int> _supported_hub_ids;
};
