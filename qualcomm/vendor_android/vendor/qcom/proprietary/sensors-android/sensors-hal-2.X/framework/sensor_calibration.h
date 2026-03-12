/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_diag_log.h"
#include "worker.h"
#include <string.h>
#include <cstdint>
#include <list>
#include <mutex>
#include <condition_variable>

using suid = com::quic::sensinghub::suid;

class calibration {
  public:
  /*
   * @brief: Bias parameters for the calibration
   *         data received from sensor.
   * */
    struct bias
    {
      float values[3];
      uint64_t timestamp;
      sns_std_sensor_sample_status status;
    };
  /**
   * @brief: creates a calibration object for sensors
   *         having calibration property
   *
   * @param:  in, calibration suid, sensor datatype, qsh interface id
   *
   * All @param are mandatory.
   */
    calibration(suid sensor_uid, std::string datatype, unsigned int qsh_intf_id);

  /**
   * @brief: deletes the created timer.
   */
    ~calibration();

  /**
   * @brief: returns latest calibration bias
   *         that is older than the timestamp
   *
   * @param: in, timestamp for which bias is required
   */
    bias get_bias(uint64_t timestamp);

  /**
   * @brief: performs suid discovery and resends
   *         calibration request
   */
    void reactivate();

  /**
   * @brief: activate calibration sensor
   */
    void activate();

  /**
   * @brief: deactivate calibration sensor
   */
    void deactivate();
  private:
    suid _sensor_uid;
    std::string _datatype;
    ISession* _qmiSession;
    std::unique_ptr<worker> _worker;
    std::unique_ptr<sensors_diag_log> _diag;
    std::list<bias> _biases;
    /* status based on calibration accuracy */
    sns_std_sensor_sample_status _sample_status;
    ISession::eventCallBack _event_cb;
    ISession::respCallBack _resp_cb;

    bool _is_disable_in_progress = false;
    bool _resp_received ;
    std::recursive_mutex _resp_mutex;
    std::condition_variable_any _resp_cv;
    std::mutex _request_mutex;
    std::atomic_int _request_count = 0;
    std::atomic_bool _is_active = false;
    unsigned int _qsh_intf_id;

  /**
   * @brief: sends calibration enable or disable request
   *         based on value of param
   *
   * @param: in, send_enable_flag - if true send enable request
   *             if false send disable request
   *
   * @out: true on success, false on failure
   */
    bool send_req(bool send_enable_flag);
  /**
   * @brief: handles ISession response callback
   */
    void handle_resp_cb(uint32_t resp_value, uint64_t client_connect_id);

  /**
   * @brief: handles ISession event callback
   */
    void handle_event_cb(const uint8_t *data, size_t size, uint64_t ts);
};
