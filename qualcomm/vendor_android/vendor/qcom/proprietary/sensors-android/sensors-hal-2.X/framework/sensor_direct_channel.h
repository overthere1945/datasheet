/*============================================================================
  @file sensor_direct_channel.h

  @brief
  direct_channel class interface for HAL

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
============================================================================*/

#pragma once

#include <cstdlib>
#include <map>
#include <memory>
#include <atomic>
#include <cutils/native_handle.h>
#include <android/hardware/graphics/mapper/IMapper.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>

#include "sns_direct_channel.pb.h"
#include "sns_direct_channel.h"

#include "suid.h"

using suid = com::quic::sensinghub::suid;

class direct_channel_client {
  public:
    direct_channel_client(int sensor_handle, suid sensor_uid, bool calibrated, int sensor_type)
    {
      _sensor_handle = sensor_handle;
      _sensor_uid = sensor_uid;
      _calibrated = calibrated;
      _sensor_type = sensor_type;
    }
    std::string config_req_str(unsigned int sample_period_us);
    std::string remove_req_str();

  private:
    suid _sensor_uid;
    bool _calibrated;
    float _sr_hz;
    int _sensor_handle;
    int _sensor_type;
};

class direct_channel {
public:
  /**
   * @brief constructor
   *
   * @param[in] mem_handle handle of share memory
   * @param[in] mem_size size of share memory block
   */

  direct_channel(const struct native_handle *mem_handle,
      const size_t mem_size);
  
  /**
   * @brief destructor
   */
  ~direct_channel();

  /**
   * @brief get channel handle on remote side
   *
   * @return int handle
   */
    int get_low_lat_handle();

    /**
     * @brief get channel handle on Android side
     *
     * @return int handle
     */
    int get_client_channel_handle();

  /**
   * @brief get file descriptor of share memory
   *
   * @return int fd
   */
  int get_buffer_fd();

  /**
   * @brief check if memory buffer is already register to current channel object
   *
   * @param[in] handle of memory block to be validated
   *
   * @return bool true if this block of memory is already registered, false otherwise
   */
  bool is_same_memory(const struct native_handle *mem_handle);

  /**
   * @brief configure sensor in particular direct report channel.
   *
   * @param[in] handle Handle of direct report channel
   * @param[in] timestamp_offset Timestamp offset between AP and QSH.
   * @param[in] sensor_suid Sensor to configure
   * @param[in] sample_period_us Sensor sampling period, set this to zero will stop the sensor.
   * @param[in] flags Configure flag of sensor refer to "SNS_LLCM_FLAG_X"
   * @param[in] sensor_handle Sensor identifier used in Android.
   *
   * @return int 0 on success, negative on failure
   */
  int config_channel(int64_t timestamp_offset,
      const suid* sensor_suid, unsigned int sample_period_us,
      unsigned int flags, int sensor_handle, int sensor_type);

  /**
   * @brief Disable all sensors in particular channel.
   *
   * @param[in] handle Handle of direct report channel
   *
   * @return int 0 on success, negative on failure
   */
  int stop_channel();

  /**
   * @brief Update the timestamp offset between AP and QSH
   *
   * @param[in] new_offset Timestamp offset
   */
  int update_offset(int64_t new_offset);

  /**
   * @brief reset direct channel resource
   *
   */
  static void reset(void);

  private:
    static remote_handle64 _remote_handle_fd;
    native_handle *_mem_native_handle;
    int _buffer_size;
    void* _buffer_ptr;
    uint64_t _offset;
    int32_t init_rpc_channel();
    std::string get_channel_req_str();
    std::string update_offset_req_str(int64_t new_offset);
    int32_t config_req_str(int32_t sensor_handle,float sr);
    AIMapper *_AIMapper = nullptr;
    buffer_handle_t importedHandle;
    /**
     * @brief initialize imapper
     *
     * @return int 0 on success, negative on failure
     */
    int init_mapper(void);
    bool get_buffer_fd(buffer_handle_t buffer_handle, uint64_t *outBufferId);
    static remote_handle64 _sns_rpc_handle64;

    int _channel_handle =-1;
    std::map<int, std::unique_ptr<direct_channel_client>> _clients_map;

    int client_channel_handle;
    static int client_channel_handle_counter;
    static std::atomic<bool> _rpc_initialized;
};
