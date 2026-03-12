/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once
#include <vector>
#include <mutex>
#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/2.1/types.h>
#include "sns_client.pb.h"

#include "ISession.h"
#include "sensor.h"
#include "sensors_trace.h"
#include "sensors_log.h"
#include "sensor_attributes.h"
#include "sensor_factory.h"
#include "sensors_hal_common.h"
#include "sensor_diag_log.h"
#include "sensors_timeutil.h"
#include "qsh_intf_resource_manager.h"
#include "worker.h"
#include "wakelock_utils.h"

using suid = com::quic::sensinghub::suid;

static const auto MSEC_PER_SEC = 1000;

// suid lookup timeout after SSR
static const auto SUID_DISCOVERY_TIMEOUT_MS = 5000;

/**
 * @brief Class which records statistics of of all sensors
 * which are implemented on QSH.
 */
class qsh_sensor_stats{
public:
    /**
     * @brief initializes variables used to calculate statistics.
     */
    void start();

    /**
     * @brief Stops calculating statistics when a sensor is deactivated.
     *        Records amount of time served by the sensor.
     */
    void stop();

    /**
     * @brief Records received time stamp of the sensor.
     */
    void add_sample(uint64_t received_ts);

    /**
     * @brief Records other time stamps of the newly added sample.
     *        Calculates other stats like sample interval, time stamp diference,
     *        max and min timestamp difference etc.
     */
    void update_sample(uint64_t meas_ts, uint64_t hal_ts);

    /**
     * @brief Returns a String with all the stats and their values.
     */
    std::string get_stats();

private:
    /*to hold difference b/w 2 samples TS*/
    uint64_t _sample_cnt = 0;
    uint64_t _activate_ts = 0;
    uint64_t _deactive_ts = 0;
    uint64_t _served_duration = 0;

    uint64_t _ts_diff_acc = 0;
    uint64_t _max_ts_rxved = 0;
    uint64_t _min_ts_rxved = std::numeric_limits<uint64_t>::max();

    uint64_t _ts_diff_acc_hal = 0;
    uint64_t _max_ts_rxved_hal = 0;
    uint64_t _min_ts_rxved_hal = std::numeric_limits<uint64_t>::max();

    uint64_t _ts_diff_acc_qmi = 0;
    uint64_t _max_ts_rxved_qmi = 0;
    uint64_t _min_ts_rxved_qmi = std::numeric_limits<uint64_t>::max();

    uint64_t _sample_received_ts = 0;

    uint64_t _previous_qsh_ts = 0;
    uint64_t _current_qsh_ts = 0;
    uint64_t _qsh_ts_diff_bw_samples = 0;
    uint64_t _acc_qsh_ts = 0;
    uint64_t _min_qsh_ts_diff = std::numeric_limits<uint64_t>::max();
    uint64_t _max_qsh_ts_diff = 0;

    /*Flag to check if add_sample has been called by the client*/
    bool _sample_added = false;
};


/**
 * @brief Abstract class of all sensors which are implemented on
 *        QSH.
 *
 *  This class provides common implementation for activating,
 *  deactivating and configuring sensors. This also contains
 *  logic for handling wakeup/nowakeup and batching
 *  functionality.
 */
class qsh_sensor : public sensor
{

public:
    /**
     * @brief creates an qsh_sensor object for given SUID and wakeup
     *        mode
     *
     * @param suid suid of the qsh sensor
     * @param wakeup wakeup mode
     */
    qsh_sensor(suid sensor_uid, sensor_wakeup_type wakeup);

    virtual ~qsh_sensor() = default;

    /* see sensor::activate */
    virtual void activate() override;

    /* see sensor::deactivate */
    virtual void deactivate() override;

    /* see sensor::update_config */
    virtual void update_config() override;

    /* see sensor::flush */
    virtual void flush() override;

    /* see sensor::is_active */
    virtual bool is_active() override { return ( _qmiSession != nullptr); }

    /**
     * @brief get calibration mode
     * @return bool true if calibration mode is SENSOR_CALIBRATED
     *                false otherwrise.
     */
    virtual bool is_calibrated() override { return false; }

    /**
     * @brief get SUID of this sensor
     * @return const suid
     */
    virtual const suid get_sensor_suid() override { return _sensor_uid; }

    /**
     * @brief reset sensor
     */
    virtual void reset() override { _reset_requested = true; }

    /**
     * @brief generic function template for creating qsh_sensors of
     *        a given typename S
     */
    template<typename S>
    static std::vector<std::unique_ptr<sensor>> get_available_sensors()
    {
        std::vector<std::unique_ptr<sensor>> sensors;
        const char *datatype = S::qsh_datatype();
        const std::vector<suid>& suids =
                sensor_factory::instance().get_suids(datatype);

        for (const auto& sensor_uid : suids) {
            try {
                sensors.push_back(std::make_unique<S>(sensor_uid, SENSOR_WAKEUP));
            } catch (const std::exception& e) {
                sns_loge("%s", e.what());
                sns_loge("failed to create sensor %s, wakeup=true", datatype);
            }
            try {
                sensors.push_back(std::make_unique<S>(sensor_uid, SENSOR_NO_WAKEUP));
            } catch (const std::exception& e) {
                sns_loge("%s", e.what());
                sns_loge("failed to create sensor %s, wakeup=false", datatype);
            }
        }
        return sensors;
    }

    /**
     * @brief generic function template for creating qsh_sensors of
     *        a given typename S
     */
    template<typename S>
    static std::vector<std::unique_ptr<sensor>> get_available_wakeup_sensors()
    {
        std::vector<std::unique_ptr<sensor>> sensors;
        const char *datatype = S::qsh_datatype();
        const std::vector<suid>& suids =
                sensor_factory::instance().get_suids(datatype);

        for (const auto& sensor_uid : suids) {
            try {
                sensors.push_back(std::make_unique<S>(sensor_uid, SENSOR_WAKEUP));
            } catch (const std::exception& e) {
                sns_loge("%s", e.what());
                sns_loge("failed to create sensor %s, wakeup=true", datatype);
            }
        }
        return sensors;
    }

    /**
     * @brief update hal handle with the given sensor type
     */
    virtual void update_handle(int type) override;

    bool _set_thread_name = false;
protected:
    /**
     * @brief create a config request message for this sensor
     * @return sns_client_request_msg
     */
    virtual sns_client_request_msg create_sensor_config_request();

    /**
     * @brief handle all sensor-related events from qsh, default
     *        implementation is for std_sensor.proto
     *
     * @param pb_event
     */
    virtual void handle_sns_client_event(
        const sns_client_event_msg_sns_client_event& pb_event);

    /**
     * @brief handle the std_sensor event, default implementation
     *        copies the data from sensor event and submits event to
     *        sensor_hal
     * @param pb_event
     */
    virtual void handle_sns_std_sensor_event(
        const sns_client_event_msg_sns_client_event& pb_event);

    /**
     * @brief handler function that will be called after a
     *        METADATA_FLUSH_COMPLETE event is sent. Derived sensor
     *        classes can use this as a notification for flush
     *        completion
     */
    virtual void on_flush_complete() { }

    /**
     * @brief handle qsh connection error
     *        this cal be overriden by derived class to do
     *        each sensor's own error handling and can call
     *        this function in base class to do common job
     * @param ISession::error
     */

    virtual void qsh_conn_error_cb(ISession::error e);

    void acquire_qsh_interface_instance();
    void release_qsh_interface_instance();
    void start_stream();

    /**
     * @brief returns the datatype of this sensor
     * @return const std::string&
     */
    const std::string& datatype() const { return _datatype; }

    /**
     * @brief returns an object storing attributes of this sensor
     * @return const sensor_attributes&
     */
    const sensor_attributes& attributes() const { return _attributes; }

    /**
     * @brief creates a blank sensor_event_t for given qsh qtimer
     *        timestamp
     * @param qsh_timestamp timestamp in qtimer ticks
     * @return Event
     */
    Event create_sensor_hal_event(uint64_t qsh_timestamp);

    sns_client_delivery get_delivery_type() const
    {
        if ((get_sensor_info().flags & SENSOR_FLAG_WAKE_UP) != 0) {
            return SNS_CLIENT_DELIVERY_WAKEUP;
        } else {
            return SNS_CLIENT_DELIVERY_NO_WAKEUP;
        }
    }

    static SensorStatus sensors_hal_sample_status(sns_std_sensor_sample_status std_status);

    /**
     * @brief set if resampler is used for this sensor
     * @param val
     */
    void set_resampling(bool val);

    /**
     * @brief set the sensor name with optional type parameter
     * @param type_name
     */
    void set_sensor_typename(const std::string& type_name = "");

    /**
     * @brief Adds a message id into the no wakeup message id list
     * maintained for the sensor.
     * @param msg_id
     */
    void set_nowk_msgid(const int msg_id);

   /**
     * @brief no need to handle flush events returned from remote proc if
     * no flush request can be placed from APSS.
     * @param val
     */
    void donot_honor_flushevent(bool val);

    bool is_resolution_set;
    bool is_max_range_set;
    std::atomic<bool> _reset_requested;

    bool _is_one_shot_senor = false;
    bool _oneshot_done = false;
    std::unique_ptr<worker> _oneshot_worker;

    unsigned int _qsh_intf_id;

    static const uint32_t _resp_timeout_msec = 500;

    /* connection to QSH */
    ISession* _qmiSession;

    /* wake_lock control */
    sns_wakelock *_wakelock_inst;

    /* interface to diag logging library */
    sensors_diag_log *_diag;
    /* number of flush requests for which a flush event is not generated */
    std::atomic<unsigned int> _pending_flush_requests;

    /* Send disable request and wait for resp*/
    void send_sensor_disable_request();

    qsh_sensor_stats* _stats;

private:

    /* set sensor information based on attributes and config */
    void set_sensor_info();

    /* callback function for all ISession events */
    void qsh_conn_event_cb(const uint8_t *data, size_t size, uint64_t sample_received_ts);

    void qsh_conn_resp_cb(uint32_t resp_value, uint64_t client_connect_id);
    /* event handler for qsh flush event */
    void handle_sns_std_flush_event(uint64_t ts);

    /* Update the list of nowk message ids in the request message */
    void add_nowk_msgid_list(sns_client_request_msg &req_msg);

    /* create and send config request to qsh */
    void send_sensor_config_request();

    /* lookup attributes for this sensor */
    const sensor_attributes& lookup_attributes() const
    {
        return sensor_factory::instance().get_attributes(_sensor_uid);
    }

    /* send sensor request message in sync manner */
    void send_sync_sensor_request(std::string& pb_req_msg_encoded);

    /*Check if sensor is available.
      Return true if available, else false*/
    bool is_sensor_available();

    /* suid of this sensor */
    suid _sensor_uid;

    /* datatype of this sensor */
    std::string _datatype;

    /* attributes of this sensor */
    const sensor_attributes& _attributes;

    /* flag for resampler usage */
    bool _resampling = false;

    /* max delay supported by sensor */
    int32_t _sensor_max_delay;

    /* max delay supported by resampler */
    const uint32_t RESAMPLER_MAX_DELAY = USEC_PER_SEC;

    sensor_wakeup_type _wakeup_type;

    /* mutex for serializing api calls */
    std::mutex _mutex;

    /*used to update the wakeup sensor string*/
    std::string _sensor_name;

    /* Maintains the list of no wakeup message ids */
    std::vector<int> _nowk_msg_id_list;

    /* flag for flushing the event */
    bool _donot_honor_flushevent = false;

    /* variables required to make sync reqeust */
    std::condition_variable_any _resp_cv;
    std::recursive_mutex _resp_mutex;
    uint32_t _resp_ret;
    bool _notify_resp;
    bool _resp_received;
    bool _set_rt_prio_event_cb;
    /* RT priority for event handling thread */
    static const int _sched_priority = 10;


protected:

    /*previous sample*/
    Event _prev_sample;
    /* check for duplicate samples for onchange samples */
    bool duplicate_onchange_sample(const Event &hal_event);

    /* check for un-order samples  */
    bool can_submit_sample(Event hal_event)
    {
      if(hal_event.timestamp > _previous_ts) {
        if(!duplicate_onchange_sample(hal_event)) {
          _previous_ts = hal_event.timestamp;
          return true;
        } else {
          sns_logi("duplicate sample for on change sensor received");
          return false;
        }
      } else {
        sns_logi("unorder sample: Type:%d,curr_ts %lld,prev_ts:%lld ",
                  (int)hal_event.sensorType,
                  (long long)hal_event.timestamp,
                  (long long)_previous_ts);
        return false;
      }
    }
};
