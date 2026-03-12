/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once
#include <vector>
#include "ISession.h"
#include "SessionFactory.h"

using suid = com::quic::sensinghub::suid;

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;
/**
 * @brief type alias for an suid event function
 *
 * param datatype: datatype of of the sensor associated with the
 * event
 * param suids: vector of suids available for the given datatype
 */
using suid_event_function =
    std::function<void(const std::string& datatype,
                       const std::vector<suid>& suids)>;

/**
 * @brief Utility class for discovering available sensors using
 *        dataytpe
 *
 */
class suid_lookup
{
public:
    /**
     * @brief creates a new connection to qsh for suid lookup
     *
     * @param cb callback function for suids
     */
    suid_lookup(suid_event_function cb, int hub_id = -1);
    ~suid_lookup();

    /**
     *  @brief look up the suid for a given datatype, registered
     *         callback will be called when suid is available for
     *         this datatype
     *
     *  @param datatype data type for which suid is requested
     *  @param default_only option to ask for publishing only default
     *         suid for the given data type. default value is false
     */
    void request_suid(std::string datatype, bool default_only = false);

private:
    suid_event_function _cb;
    void handle_qsh_event(const uint8_t *data, size_t size, uint64_t time_stamp);
    std::unique_ptr<ISession> _session = nullptr;
    std::unique_ptr<sessionFactory> _sessionFactory = nullptr;
    suid _sensor_uid;
    bool _set_thread_name = false;
};
