/*============================================================================
  @qsh_Calibrate.h

  Sensor calibration for qsh header file

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ============================================================================*/

#include <iostream>
#include <unordered_map>
#include <cinttypes>
#include <unistd.h>
#include "sns_client.pb.h"
#include "sns_suid.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"
#include "SessionFactory.h"
#include "ISession.h"
#include "suid.h"
#include "sensors_log.h"
#include "suid_lookup.h"

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;
using suid = com::quic::sensinghub::suid;
/**
 * @brief Class for discovering the suid for a datatype
 */
class qsh_calibrate
{

public:
    qsh_calibrate();
    ~qsh_calibrate();
    /**
     * @brief do sensor test for a given datatype and test_type
     *
     * @param datatype
     * @return string include sensot name and test result
     */
    std::string calibrate(const std::string& datatype, const uint8_t &test_type);

private:

    struct suid_hash
    {
        std::size_t operator()(const suid& sensor_uid) const
        {
            std::string data(reinterpret_cast<const char*>(&sensor_uid), sizeof(suid));
            return std::hash<std::string>()(data);
        }
    };

    struct sensor_attributes
    {
        uint8_t num_attributes;
        std::unordered_map<int32_t, sns_std_attr_value> _attr_map;
    };

    /* map of sensor suids for each datatype */
    std::unordered_map<std::string, std::vector<suid>> _sensor_suid_map;
    std::unordered_map<suid, std::string, suid_hash> _sensortest_map;
    std::unordered_map<suid, sensor_attributes, suid_hash> _attributes_map;
    std::vector<suid> avaib_suids;
    sensor_attributes attrs;
    std::condition_variable _cv_attr;
    std::condition_variable _cv_phy_test;
    std::condition_variable _cv_result;
    std::mutex _attr_mutex;
    std::mutex _test_mutex;
    std::mutex _result_mutex;
    uint32_t _pending_attrs;
    uint32_t _pending_test;
    uint8_t _test_type;
    ISession::eventCallBack _event_cb = nullptr;

    std::string get_sensor_name(suid sensor_uid);
    std::string get_invalid_result(std::string data_type);
    std::string get_test_result(suid sensor_uid);
    std::string test_result_list(std::string data_type);
    const std::string get_string(suid sensor_uid, int32_t attr_id);
    std::vector<float> get_floats(suid sensor_uid, int32_t attr_id);
    std::vector<int64_t> get_ints(suid sensor_uid, int32_t attr_id);
    std::vector<std::pair<float, float>> get_ranges(suid sensor_uid);
    int64_t get_stream_type(suid sensor_uid);
    std::string suid_to_string(suid sensor_uid);
    std::vector<suid> get_all_available_suids(std::string data_type, uint8_t test_type);
    void suid_cbk(const std::string& datatype, const std::vector<suid>& suids);

    /**
     * Send a SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG
     * to sensor driver
     */
    void
    send_selftest_req(ISession *session, suid sensor_uid, const uint8_t &test_type);

    /**
     * Send a SNS_STD_MSGID_SNS_STD_ATTR_REQ
     * to sensor driver
     */
    void send_attr_req(ISession *session, suid sensor_uid);

   /**
    * Physical test event handle function
    */
    void handle_physical_test_event(suid sensor_uid,
                const sns_client_event_msg_sns_client_event& pb_event);

   /**
    * Attribute event handle function
    */
    void handle_attribute_event(suid sensor_uid,
                const sns_client_event_msg_sns_client_event& pb_event);

    void calibrate_event_cb(const uint8_t* msg,
        int msgLength,
        uint64_t time_stamp);

    std::unique_ptr<sessionFactory> _session_factory_instance;

};
