/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <functional>
#include <cutils/properties.h>
#include <cassert>
#include "sensors_hal.h"
#include "sensor_factory.h"
#include "sensors_timeutil.h"
#include "sensors_trace.h"
#include "qsh_intf_resource_manager.h"

using namespace std;

enum {
  SNS_LLCM_FLAG_INTERRUPT_EN          = 0x1,
  SNS_LLCM_FLAG_ANDROID_STYLE_OUTPUT  = 0x2,
  SNS_LLCM_FLAG_CALIBRATED_STREAM     = 0x4
};

/*  These macros define the equivalent sample period (in us) for the various
    SENSOR_DIRECT_RATE levels.
    NOTE: Although the HAL will request at these sampling periods, it's possible
    for the sensor to stream at another nearby rate based on ODR supported by driver. */
static const int SNS_DIRECT_RATE_NORMAL_SAMPLE_US    = 20000U;
static const int SNS_DIRECT_RATE_FAST_SAMPLE_US      = 5000U;
static const int SNS_DIRECT_RATE_VERY_FAST_SAMPLE_US = 1250U;
static const int SNS_DIRECT_REPORT_UPDATE_INTERVAl_SCALER = 2;

sensors_hal* sensors_hal::_self = nullptr;

/* map debug property value to log_level */
static const unordered_map<char, sensors_log::log_level> log_level_map = {
    { '0', sensors_log::SILENT },
    { '1', sensors_log::INFO },
    { 'e', sensors_log::ERROR },
    { 'E', sensors_log::ERROR },
    { 'i', sensors_log::INFO },
    { 'I', sensors_log::INFO },
    { 'd', sensors_log::DEBUG },
    { 'D', sensors_log::DEBUG },
    { 'v', sensors_log::VERBOSE },
    { 'V', sensors_log::VERBOSE },
};

sensors_hal* sensors_hal::get_instance()
{
    if(nullptr != _self) {
      return _self;
    } else {
      _self = new sensors_hal();
      return _self;
    }
}

sensors_hal::sensors_hal()
  : _ssr_handler(nullptr),
    _event_callback(nullptr)
{
    using namespace std::chrono;
    auto tp_start = steady_clock::now();
    sensors_log::set_tag("sensors-hal");
    char level_code = prop.get_log_level();
    if (log_level_map.find(level_code) != log_level_map.end()) {
      sensors_log::set_level(log_level_map.at(level_code));
    }
    SNS_TRACE_ENABLE(prop.is_trace_enabled());
    _is_direct_channel_supported = prop.get_direct_channel_config();
    _is_stats_enable = prop.get_stats_support_flag();

    _offset_update_timer = NULL;

    sns_logi("initializing sensors_hal");

    _session_factory_instance = make_unique<sessionFactory>();
    if(nullptr == _session_factory_instance){
      printf("failed to create factory instance");
    }

    try {
        init_sensors();
        if(_is_direct_channel_supported == true) {
            open_direct_channels = std::list<direct_channel *>();
            struct sigevent sigev = {};
            sigev.sigev_notify = SIGEV_THREAD;
            sigev.sigev_notify_attributes = nullptr;
            sigev.sigev_value.sival_ptr = (void *) this;
            sigev.sigev_notify_function = on_offset_timer_update;
            if (timer_create(CLOCK_BOOTTIME, &sigev, &_offset_update_timer) != 0) {
                sns_loge("offset update timer creation failed: %s", strerror(errno));
            }
        }
    } catch (const runtime_error& e) {
        sns_loge("FATAL: failed to initialize sensors_hal: %s", e.what());
        return;
    }
    auto tp_end = steady_clock::now();
    sns_logi("sensors_hal initialized, init_time = %fs",
            duration_cast<duration<double>>(tp_end - tp_start).count());
}

int sensors_hal::activate(int handle, int enable)
{
    auto it = _sensors.find(handle);
    if (it == _sensors.end()) {
        sns_loge("handle %d not supported", handle);
        return -EINVAL;
    }
    auto& sensor = it->second;
    try {
        sns_logi("%s/%d en=%d", sensor->get_sensor_info().stringType,
                 handle, enable);
        if (enable) {
            sensor->activate();
        } else {
            sensor->deactivate();
        }
    } catch (const exception& e) {
        sns_loge("failed: %s", e.what());
        return -1;
    }
    sns_logi("%s/%d en=%d completed", sensor->get_sensor_info().stringType,
             handle, enable);
    return 0;
}

int sensors_hal::batch(int handle, int64_t sampling_period_ns,
                       int64_t max_report_latency_ns)
{
    auto it = _sensors.find(handle);
    if (it == _sensors.end()) {
        sns_loge("handle %d not supported", handle);
        return -EINVAL;
    }

    auto& sensor = it->second;
    sensor_params params;
    params.max_latency_ns = max_report_latency_ns;
    params.sample_period_ns = sampling_period_ns;

    try {
        sns_logi("%s/%d, period=%lld, max_latency=%lld",
                 sensor->get_sensor_info().stringType, handle,
                 (long long) sampling_period_ns,
                 (long long) max_report_latency_ns);
        sensor->set_config(params);
    } catch (const exception& e) {
        sns_loge("failed: %s", e.what());
        return -1;
    }
    sns_logi("%s/%d, period=%lld, max_latency=%lld request completed",
             sensor->get_sensor_info().stringType, handle,
             (long long) sampling_period_ns,
             (long long) max_report_latency_ns);
    return 0;
}

int sensors_hal::flush(int handle)
{
    auto it = _sensors.find(handle);
    if (it == _sensors.end()) {
        sns_loge("handle %d not supported", handle);
        return -EINVAL;
    }
    auto& sensor = it->second;
    const sensor_t& sensor_info = sensor->get_sensor_info();
    bool is_oneshot = (sensor->get_reporting_mode() == SENSOR_FLAG_ONE_SHOT_MODE);
    if (!sensor->is_active() || is_oneshot) {
        sns_loge("invalid flush request, active=%d, oneshot=%d",
                  sensor->is_active(), is_oneshot);
        return -EINVAL;
    }
    try {
        sns_logi("%s/%d", sensor_info.stringType, handle);
        sensor->flush();
    } catch (const exception& e) {
        sns_loge("failed, %s", e.what());
        return -1;
    }
    sns_logi("%s/%d completed", sensor_info.stringType, handle);

    return 0;
}

void sensors_hal::init_sensors()
{
    auto sensors = sensor_factory::instance().get_all_available_sensors();

    auto event_cb = [this](const std::vector<Event>& events, bool wakeup) { _event_callback->post_events(events, wakeup); };

    sns_logi("initializing sensors");

    for (unique_ptr<sensor>& s : sensors) {
        s->register_event_callback(event_cb);
        s->update_direct_channel_support_status(_is_direct_channel_supported);
        s->update_stats_flags(_is_stats_enable);
        if(false == s->get_incompatible_flag()) {
            const sensor_t& sensor_info = s->get_sensor_info();
            sns_logd("%s: %s/%d wakeup=%d", sensor_info.name,
                     sensor_info.stringType, sensor_info.handle,
                     (sensor_info.flags & SENSOR_FLAG_WAKE_UP) != 0);
            _hal_sensors.push_back(sensor_info);
            _sensors[sensor_info.handle] = std::move(s);
        }
    }
    //make persist QMI connection to catch SSR
    if (_ssr_handler == nullptr) {
        _ssr_handler = _session_factory_instance->getSession();
        if(nullptr == _ssr_handler) {
          sns_loge("failed to create ISession");
          return;
        }
        int ret = _ssr_handler->open();
        if(-1 == ret) {
          sns_loge("failed to open a static session for SSR handling ");
          delete _ssr_handler;
          _ssr_handler = nullptr;
          return;
        }

        /*suid needs to be a special one - to take care of persist QMI channel for error handling
         * 0xFFFFFFFFFFFFFFFF
         * 0xFFFFFFFFFFFFFFFF
         * */
        suid sensor_uid;
        sensor_uid.low = 18446744073709551615ull;
        sensor_uid.high = 18446744073709551615ull;

        ISession::errorCallBack errr_cb =[this](ISession::error error)
        {
          sns_logi("connection is reset. it's SSR");
          unique_lock<mutex> lk(open_direct_channels_mutex);
          sns_logi("close all opened direct_channels, num of channel = %zu",
              open_direct_channels.size());
          for (auto iter = open_direct_channels.begin();
              iter != open_direct_channels.end();) {
              delete *iter;
              iter = open_direct_channels.erase(iter);
          }
          if (open_direct_channels.size() == 0) {
              stop_offset_update_timer();
          }
          sns_logi("reset direct_channel");
          direct_channel::reset();
          lk.unlock();
          for (auto& it : _sensors) {
              auto& sensor = it.second;
              const sensor_t& sensor_info = sensor->get_sensor_info();
              sns_logd("%s: %s/%d : reset sensor", sensor_info.name,
                  sensor_info.stringType, sensor_info.handle);
              sensor->reset();
          }
        };

        ret = _ssr_handler->setCallBacks(sensor_uid,nullptr,errr_cb,nullptr);
        if(0 != ret)
            sns_loge("all callbacks are null, no need to register it");

        sns_logi("SSR handler is registered");
    }
}

int sensors_hal::get_sensors_list(const sensor_t **s_list)
{
    int num_sensors = (int)_hal_sensors.size();
    sns_logi("num_sensors=%d", num_sensors);
    *s_list = &_hal_sensors[0];
    return num_sensors;
}

int sensors_hal::register_direct_channel(const struct sensors_direct_mem_t* mem)
{
    int ret = 0;
    struct native_handle *mem_handle = NULL;

    sns_logi("%s : enter", __FUNCTION__);
    /* Validate input parameters */
    if (mem->type != SENSOR_DIRECT_MEM_TYPE_GRALLOC ||
        mem->format != SENSOR_DIRECT_FMT_SENSORS_EVENT ||
        mem->size == 0) {
        sns_loge("%s : invalid input mem type=%d, format=%d, size=%zu",
            __FUNCTION__, mem->type, mem->format, mem->size);
        return -EINVAL;
    }

  // Make sure the input fd isn't already in use
    if (mem->handle == NULL) {
      sns_loge("%s: invalid mem handle ", __FUNCTION__);
      return -EINVAL;
    } else if((mem->handle->numFds + mem->handle->numInts) <= 0 ){
      sns_loge("%s: invalid mem size ", __FUNCTION__);
      return -EINVAL;
    } else if (this->is_used_buffer_fd(mem->handle->data[0])) {
      sns_loge("%s: invalid direct_mem", __FUNCTION__);
      return -EINVAL;
    }
    sns_logd("%s : handle=%p, size=%zu", __FUNCTION__,
        mem->handle, mem->size);

    /* Make sure memory not in use */
    if (this->is_used_memory(mem->handle)) {
        sns_loge("%s : memory is in use %p", __FUNCTION__, mem->handle);
        return -EINVAL; //debug only
    }

    /* Construct a new direct channel object and initialize channel */
    direct_channel* channel = new direct_channel(mem->handle, mem->size);
    if(nullptr == channel) {
      sns_loge("%s : direct channel init failed, ret=%d deleting obj", __FUNCTION__, ret);
      return -EINVAL;
    }

    if (-1 == channel->get_low_lat_handle()) {
        sns_loge("%s : direct channel init failed, ret=%d deleting obj", __FUNCTION__, ret);
        delete channel;
        return -EINVAL;
    }

    /* Add the new direct channel object to the list of direct channels */
    lock_guard<mutex> lk(open_direct_channels_mutex);
    open_direct_channels.insert(open_direct_channels.cend(), channel);
    if (open_direct_channels.size() == 1) {
        // Force an update so we provide the most accurate initial delta on configuring.
        // Also ensures that we will update faster than
        sensors_timeutil::get_instance().recalculate_offset(true /* force_update */);
        start_offset_update_timer();
    }

    /* If all goes well, return the android handle */
    sns_logv("%s : assigned channel handle %d", __FUNCTION__,
                channel->get_client_channel_handle());
    return channel->get_client_channel_handle();
}

int sensors_hal::unregister_direct_channel(int channel_handle)
{
    int ret;
    std::list<direct_channel*>::const_iterator iter;
    sns_logi("%s : channel handle %d", __FUNCTION__, channel_handle);

    /* Search for the corresponding DirectChannel object */
    lock_guard<mutex> lk(open_direct_channels_mutex);
    iter = find_direct_channel(channel_handle);
    /* If the channel is found, stop it, remove it and delete it */
    if (iter != open_direct_channels.cend()) {
        /* The stream_stop call below isn't strictly necessary. When the
           DirectChannel object is deconstructed, it calls stream_deinit()
           which inherently calls stream_stop(). But it was added here for
           completeness-sake. */
        ret = (*iter)->stop_channel();
        if(ret != 0) {
          sns_loge("%s: error stop_channel for hanadle %d " , __FUNCTION__, channel_handle);
          return -EINVAL;
        }
        delete *iter;
        open_direct_channels.erase(iter);

        if (open_direct_channels.size() == 0) {
            stop_offset_update_timer();
        }
    } else {
        /*Return 0 to satisfy VTS testcase*/
        sns_logi("%s : channel_handle[%d] either not activated or not present", __FUNCTION__ , channel_handle);
        return 0;
    }

    return 0;
}

int sensors_hal::config_direct_report(int sensor_handle, int channel_handle,
    const struct sensors_direct_cfg_t *config)
{
    int ret = -EINVAL;
    int type;

    suid request_suid = {0, 0};

    int64_t timestamp_offset;
    unsigned int flags = SNS_LLCM_FLAG_ANDROID_STYLE_OUTPUT;
    sensor_factory& sensor_fac = sensor_factory::instance();
    std::list<direct_channel*>::const_iterator iter;

    sns_logi("%s : enter", __FUNCTION__);

    unique_lock<mutex> lk(open_direct_channels_mutex);
    iter = find_direct_channel(channel_handle);
    if (iter == open_direct_channels.cend()) {
      sns_loge("%s : The channel %d is not available!", __FUNCTION__, channel_handle);
      lk.unlock();
      return -EINVAL;
    }

    lk.unlock();
    /* If sensor_handle is -1 and the rate is STOP, then stop all streams within
       the channel */
    if (sensor_handle == -1) {
        if (config->rate_level == SENSOR_DIRECT_RATE_STOP) {
          ret = (*iter)->stop_channel();
          if(ret != 0) {
            sns_loge("%s: error stop_channel for hanadle %d " , __FUNCTION__, channel_handle);
            return -EINVAL;
          }
          if (open_direct_channels.size() == 0) {
              stop_offset_update_timer();
          }
        } else {
            sns_loge("%s : unexpected inputs, sensor handle %d rete_level %d", __FUNCTION__,
                        sensor_handle, config->rate_level);
            ret = -EINVAL;
        }
    } else {
        auto it = _sensors.find(sensor_handle);

        if (it == _sensors.end()) {
            sns_loge("handle %d not supported", sensor_handle);
            return -EINVAL;
        }
        /* Obtain the sensor SUID */
        auto& sensor = it->second;
        request_suid = sensor->get_sensor_suid();
        /* Check whether calibration is required */
        if(sensor->is_calibrated())
        {
          flags |= SNS_LLCM_FLAG_CALIBRATED_STREAM;
        }

        if (config->rate_level == SENSOR_DIRECT_RATE_STOP) {
            /* Stop single sensor by set sampling period to 0 */
            int sensor_type = sensor->get_sensor_type();
            ret = (*iter)->config_channel(0, &request_suid, 0, flags, sensor_handle, sensor_type);
            if (ret) {
                sns_loge("%s : Deactivate the handle %d is not successful",
                             __FUNCTION__, sensor_handle);
            }
        } /* Check to make sure the sensor supports the desired rate */
        else if ((int)((sensor->get_sensor_info().flags & SENSOR_FLAG_MASK_DIRECT_REPORT) >>
                    SENSOR_FLAG_SHIFT_DIRECT_REPORT) >= config->rate_level) {
            unsigned int sample_period_us = direct_rate_level_to_sample_period(config->rate_level);
            if (sample_period_us == 0) {
                sns_loge("%s : Unexpected direct report rate %d for handle %d", __FUNCTION__,
                            config->rate_level, sensor_handle);
                return -EINVAL;
            }
            /* Get timestamp offset */
            timestamp_offset = sensors_timeutil::get_instance().getElapsedRealtimeNanoOffset();
            sns_logi("%s : config direct report, TS_offset=%lld",
                             __FUNCTION__, timestamp_offset);
            /* Start sensor with non-zero sampling rate */
            int sensor_type = sensor->get_sensor_type();
            ret = (*iter)->config_channel(timestamp_offset, &request_suid, sample_period_us, flags, sensor_handle, sensor_type);
            if (ret) {
                sns_loge("%s : activate the handle %d is not successful", __FUNCTION__,
                           sensor_handle);
            } else {
                /* Return the sensor handle if everything is successful */
                ret = sensor_handle;
            }
        } else {
            sns_loge("%s : Unsupported direct report rate %d for handle %d", __FUNCTION__,
                        config->rate_level, sensor_handle);
            return -EINVAL;
        }
    }
    return ret;
}

/*
  * Find channel which match match particular handle,
  * this function should be used under protect of mutex
  */
std::list<direct_channel*>::const_iterator sensors_hal::find_direct_channel(int channel_handle)
{
    for (auto iter = open_direct_channels.cbegin();
        iter != open_direct_channels.cend(); ++iter) {
        if ((*iter)->get_client_channel_handle() == channel_handle) {
            return iter;
        }
    }
    return open_direct_channels.cend();
}

bool sensors_hal::is_used_buffer_fd(int buffer_fd)
{
    int ret = -EINVAL;
    bool found = false;

    lock_guard<mutex> lk(open_direct_channels_mutex);

    for (auto iter = open_direct_channels.cbegin();
        iter != open_direct_channels.cend(); ++iter) {
        if ((*iter)->get_buffer_fd() == buffer_fd) {
            found = true;
        }
    }

    return found;
}

bool sensors_hal::is_used_memory(const struct native_handle *mem_handle)
{
    int ret = -EINVAL;
    bool found = false;

    lock_guard<mutex> lk(open_direct_channels_mutex);

    for (auto iter = open_direct_channels.cbegin();
        iter != open_direct_channels.cend(); ++iter) {
        if ((*iter)->is_same_memory(mem_handle)) {
            found = true;
            break;
        }
    }

    return found;
}

unsigned int sensors_hal::direct_rate_level_to_sample_period(int rate_level)
{
    unsigned int sample_period_us = 0;
    switch(rate_level) {
        case SENSOR_DIRECT_RATE_NORMAL:
            sample_period_us = SNS_DIRECT_RATE_NORMAL_SAMPLE_US;
            break;
        case SENSOR_DIRECT_RATE_FAST:
            sample_period_us = SNS_DIRECT_RATE_FAST_SAMPLE_US;
            break;
        case SENSOR_DIRECT_RATE_VERY_FAST:
            sample_period_us = SNS_DIRECT_RATE_VERY_FAST_SAMPLE_US;
            break;
       default:
            sns_loge("%s : Unsupported direct report rate %d", __FUNCTION__, rate_level);
            break;
    }
    return sample_period_us;
}

void sensors_hal::start_offset_update_timer()
{
    constexpr uint64_t NSEC_PER_MSEC = UINT64_C(1000000);
    constexpr uint64_t NSEC_PER_SEC = NSEC_PER_MSEC * 1000;
    struct itimerspec timerspec = {};
    uint64_t interval_ns = sensors_timeutil::get_instance().get_offset_update_schedule_ns();

    // The scaler set timer faster than normal offset update, which ensures that
    // we're the ones driving offset updates
    interval_ns /= SNS_DIRECT_REPORT_UPDATE_INTERVAl_SCALER;

    timerspec.it_value.tv_sec = interval_ns / NSEC_PER_SEC;
    timerspec.it_value.tv_nsec = interval_ns % NSEC_PER_SEC;
    timerspec.it_interval = timerspec.it_value;

    if (timer_settime(_offset_update_timer, 0, &timerspec, nullptr) != 0) {
        sns_loge("%s: failed to start offset update timer: %s", __FUNCTION__, strerror(errno));
    } else {
        sns_logv("%s : offset update timer started, next update in %" PRIu64 " ms",
                 __FUNCTION__, interval_ns / NSEC_PER_MSEC);
    }
}

void sensors_hal::stop_offset_update_timer()
{
    // Set to all-zeros to disarm the timer
    if(_offset_update_timer){
        struct itimerspec timerspec = {};
        if (timer_settime(_offset_update_timer, 0, &timerspec, nullptr) != 0) {
            sns_loge("%s: failed to stop offset update timer: %s", __FUNCTION__, strerror(errno));
        } else {
            sns_logv("%s : offset update timer canceled", __FUNCTION__);
        }
    }
}

void sensors_hal::handle_offset_timer()
{
    sensors_timeutil& timeutil = sensors_timeutil::get_instance();
    if (timeutil.recalculate_offset(true /* force_update */)) {
        int64_t new_offset = timeutil.getElapsedRealtimeNanoOffset();
        lock_guard<mutex> lk(open_direct_channels_mutex);
        for (direct_channel *dc : open_direct_channels) {
            dc->update_offset(new_offset);
        }
        sns_logv("%s: updated time offsets", __FUNCTION__);
    } else {
        sns_logd("offset timer did not generate new offset");
    }
}

void sensors_hal::on_offset_timer_update(union sigval param)
{
    auto *obj = static_cast<sensors_hal *>(param.sival_ptr);
    obj->handle_offset_timer();
}
