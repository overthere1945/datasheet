/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#pragma once

#include <functional>
#include <hardware/sensors.h>
#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/1.0/ISensors.h>
#include <android/hardware/sensors/2.1/ISensors.h>
#include <android/hardware/sensors/2.0/types.h>
#include <android/hardware/sensors/2.1/types.h>
#include <cinttypes>
#include "SessionFactory.h"

using ::android::hardware::sensors::V1_0::SensorStatus;
using ::android::hardware::sensors::V2_1::Event;
using ::android::hardware::sensors::V2_1::SensorType;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V1_0::MetaDataEventType;
using ::android::hardware::sensors::V1_0::AdditionalInfoType;

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;

using sensor_event_cb_func = std::function<void (const std::vector<Event>&, bool)>;
using suid = com::quic::sensinghub::suid;


/**
 * @brief parameters to be used for sensor configuration
 *
 */
struct sensor_params
{
    /* period at which the sensor data is to be sampled (nanoseconds) */
    uint64_t sample_period_ns = 0;
    /* time duration by which event from sensor can be delayed (nanoseconds) */
    uint64_t max_latency_ns = 0;

    /* == operator to compare parameters */
    bool operator==(const sensor_params& rhs) const
    {
        return ((sample_period_ns == rhs.sample_period_ns) &&
                (max_latency_ns == rhs.max_latency_ns));
    }
};

/* types of sensor wakeup modes */
enum sensor_wakeup_type
{
    SENSOR_NO_WAKEUP = 0,
    SENSOR_WAKEUP = 1,
};

/**
 * @brief abstract class representing a single physical or
 *        virtual sensor.
 *
 * This class defines an interface that is implemented by all
 * sensors to be supported in HAL. This is used as a base class
 * for all sensor implementations
 *
 */
class sensor
{
public:

    sensor(sensor_wakeup_type wakeup_type);
    virtual ~sensor() = default;

    /**
     * @brief activate sensor hardware, start streaming of the
     *        sensor events
     */
    virtual void activate() = 0;

    /**
     * @brief deactivate sensor hardware, stop streaming of sensor
     *        events
     */
    virtual void deactivate() = 0;

    /**
     * @brief flush sensor FIFO, publishes flush_complete event
     *        after FIFO is flushed
     *
     */
    virtual void flush() = 0;

    /**
     * @brief check if this sensor is currently active
     * @return bool
     */
    virtual bool is_active() = 0;

    /**
     * @brief get calibration mode
     * @return bool true if calibration mode is SENSOR_CALIBRATED
     *                false otherwrise.
     */
    virtual bool is_calibrated() = 0;

    /**
     * @brief get sensor suid
     * @return const suid
     */
    virtual const suid get_sensor_suid() = 0;

    /**
     * @brief reset sensor
     */
    virtual void reset() = 0;

    /**
     * @brief sets configuration parameters for the sensor.
     * @param params
     */
    void set_config(sensor_params params);

    void update_direct_channel_support_status(bool status)
    {
        _is_direct_channel_supported = status;
        set_direct_channel_flag(_is_direct_channel_supported);
    }

    void update_stats_flags(bool stats_flag){
      _is_stats_enable = stats_flag;
    }
    /**
     * @brief get sensor_t object describing this sensor
     * @return const sensor_t&
     */
    const sensor_t& get_sensor_info() const { return _sensor_info; }

    uint32_t get_reporting_mode() const
    {
        return _sensor_info.flags & SENSOR_FLAG_MASK_REPORTING_MODE;
    }

    virtual void register_event_callback(sensor_event_cb_func cb) { _event_cb = cb; }

    int get_sensor_type() {
      return _sensor_info.type;
    }

    bool get_incompatible_flag() {return _is_OS_incompatible;}
protected:

    uint64_t _previous_ts;

    /**
     * @brief if sensor is active, updates the sensor hardware with
     *        new configuration
     *
     */
    virtual void update_config() = 0;

    /* function to submit a new event to sensor HAL clients */
    void submit_sensors_hal_event();

    /* submits the sensors samples to android framework */
    void submit();

    /* functions for setting sensor parameters in sensor_t structure */

    void set_type(int type) { _sensor_info.type = type; update_handle(type); }

    void set_string_type(const char *str) { _sensor_info.stringType = str; }

    void set_name(const char *name) { _sensor_info.name = name; }

    void set_vendor(const char *vendor) { _sensor_info.vendor = vendor; }

    void set_version(int version) { _sensor_info.version = version; }

    void set_max_range(float r) { _sensor_info.maxRange = r; }

    void set_resolution(float r) { _sensor_info.resolution = r; }

    void set_power(float p) { _sensor_info.power = p; }

    void set_min_delay(int32_t d) { _sensor_info.minDelay = d; }

    void set_max_delay(int32_t d) { _sensor_info.maxDelay = d; }

    void set_fifo_reserved_count(uint32_t val) { _sensor_info.fifoReservedEventCount = val; }

    void set_fifo_max_count(uint32_t val) { _sensor_info.fifoMaxEventCount = val; }

    void set_required_permission(const char* str) { _sensor_info.requiredPermission = str; }

    /* set reporting mode for this sensor */
    void set_reporting_mode(uint32_t mode)
    {
        _sensor_info.flags &= ~SENSOR_FLAG_MASK_REPORTING_MODE;
        _sensor_info.flags |= mode;
    }

    /* set/clear additional info flag */
    void set_additional_info_flag(bool en)
    {
        if (en) {
            _sensor_info.flags |= SENSOR_FLAG_ADDITIONAL_INFO;
        } else {
            _sensor_info.flags &= ~SENSOR_FLAG_ADDITIONAL_INFO;
        }
    }

    /* set/clear wakeup flag */
    void set_wakeup_flag(bool en)
    {
        if (en) {
            _sensor_info.flags |= SENSOR_FLAG_WAKE_UP;
        } else {
            _sensor_info.flags &= ~SENSOR_FLAG_WAKE_UP;
        }
    }

    /* set/clear direct channel flags according to sensor minDelay */
    void set_direct_channel_flag(bool en);

    void set_incompatible_flag(bool flag) { _is_OS_incompatible = flag; }

    const sensor_params& get_params() const { return _params; }

    /* set minDelay for direct channel */
    void set_min_low_lat_delay(int32_t val) { _min_low_lat_delay = val; }

    /* set handle */
    void set_handle(int handle) { _sensor_info.handle = handle; }

    /* update handle with the given sensor type info */
    virtual void update_handle(int type) {}

    /*events store in this vector and pulled_in and going to be submnitted later*/
    std::vector<Event> events;

    std::atomic<bool> _is_deactivate_in_progress;

    std::vector<Event>::size_type _event_count;
    sensor_event_cb_func _event_cb;

    bool _is_direct_channel_supported;
    /*_is_stats_enable is boolean flag to enable/disable HAL statistics
    based on system property */
    bool _is_stats_enable;
    bool _is_OS_incompatible = false;

private:

    /* sensor information used by hal */
    sensor_t _sensor_info;
    sensor_params _params;
    bool _bwakeup;
    static std::mutex _mutex;
    int32_t _min_low_lat_delay;

    bool is_direct_channel_supported_sensortype(int type);

};

/* types of sensor calibration modes */
enum sensor_cal_type
{
    SENSOR_UNCALIBRATED = 0,
    SENSOR_CALIBRATED = 1,
};

/**
 * sensor module initialization
 *
 * derivatives of sensor class can define a function that is
 * called during sensor hal library initialization. This
 * function is used for registering new sensor types with
 * sensor_factory. This module_init function is registered using
 * this macro
 *
 * module_init function must be of following prototype which
 * always returns true.
 *
 *   bool accelerometer_module_init();
 *
 */
#define SENSOR_MODULE_INIT(module_init_func) \
    static const bool __mod_init = (module_init_func)();
