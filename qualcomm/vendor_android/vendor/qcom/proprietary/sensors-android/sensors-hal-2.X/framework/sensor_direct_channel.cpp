/*============================================================================
  @file direct_channel.cpp

  @brief
  direct_channel class implementation.

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
============================================================================*/
#include <string>
#include "sensor_direct_channel.h"
#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <dlfcn.h>
#include <inttypes.h>

#include "sns_direct_channel.pb.h"
#include "sns_std_type.pb.h"
#include "sns_std.pb.h"
#include "sns_std_sensor.pb.h"
#include "sensors_hal.h"
#include "sensors_timeutil.h"
#include "rpcmem.h"
#include <sensors_target.h>

const int32_t SNS_LLCM_FLAG_CALIBRATED_STREAM     = 0x4;
int direct_channel::client_channel_handle_counter = 0;

using ::aidl::android::hardware::graphics::common::BufferUsage;
using ::aidl::android::hardware::graphics::common::StandardMetadataType;

remote_handle64 direct_channel::_remote_handle_fd = 0;
remote_handle64 direct_channel::_sns_rpc_handle64 = 0;
std::atomic<bool> direct_channel::_rpc_initialized = false;
#define US_TO_HZ(var) 1000000/(var)

std::string direct_channel_client::config_req_str(unsigned int sample_period_us)
{
  sns_logi("%s Start" , __func__);
  std::string channel_config_msg_str;
  sns_direct_channel_config_msg channel_config_msg;
  std::string sensor_config_str;
  sns_direct_channel_set_client_req *client_req = channel_config_msg.mutable_set_client_req();
  sns_std_sensor_config sensor_config;

  //get sub fields
  sns_direct_channel_stream_id *stream_id = client_req->mutable_stream_id();
  sns_direct_channel_set_client_req_structured_mux_channel_stream_attributes *stream_attributes = client_req->mutable_attributes();
  sns_std_request *std_request = client_req->mutable_request();

  _sr_hz = US_TO_HZ(sample_period_us);

  //set config request
  client_req->set_msg_id(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG);

  // set streamid parameters
  stream_id->set_resampled(true);
  stream_id->set_calibrated(_calibrated);
  stream_id->mutable_suid()->set_suid_high(_sensor_uid.high);
  stream_id->mutable_suid()->set_suid_low(_sensor_uid.low);

  // set stream sttribute parameters
  stream_attributes->set_sensor_type(_sensor_type);
  stream_attributes->set_sensor_handle(_sensor_handle);

  // set the payload for the request
  sensor_config.set_sample_rate(_sr_hz);
  sensor_config.SerializeToString(&sensor_config_str);
  std_request->set_is_passive(false);
  std_request->set_payload( sensor_config_str);

  channel_config_msg.SerializeToString(&channel_config_msg_str);
  return channel_config_msg_str;
}

std::string direct_channel_client::remove_req_str()
{
  sns_logi("%s Start" , __func__);
  std::string channel_config_msg_str;
  sns_direct_channel_config_msg channel_config_msg;

  sns_direct_channel_remove_client_req *remove_client_req = channel_config_msg.mutable_remove_client_req();

  sns_direct_channel_stream_id *stream_id = remove_client_req->mutable_stream_id();
  stream_id->set_calibrated(_calibrated);
  stream_id->set_resampled(true);
  stream_id->mutable_suid()->set_suid_high(_sensor_uid.high);
  stream_id->mutable_suid()->set_suid_low(_sensor_uid.low);

  channel_config_msg.SerializeToString(&channel_config_msg_str);
  return channel_config_msg_str;
}

direct_channel::direct_channel(const struct native_handle *mem_handle,
    const size_t mem_size)
{
    sns_logi("%s Start" , __func__);
    int ret = 0;
    _channel_handle = -1;
    if (init_rpc_channel()) {
      /*communication to remote side is failed*/
      sns_loge("%s: init_rpc_channel failed!", __FUNCTION__);
      return;
    }

    if (init_mapper()) {
      /* Kick out if mapper initialization fails */
      sns_loge("%s: init_ampper failed!", __FUNCTION__);
      return;
    }

    /* Check the native_handle for validity */
    if (mem_handle->numFds < 1) {
      sns_loge("%s: Unexpected numFds. Expected at least 1. Received %d.", __FUNCTION__,
          mem_handle->numFds);
      return;
    }


    /* Copy the native_handle */
    _mem_native_handle = native_handle_clone(mem_handle);
    if(NULL == _mem_native_handle){
        sns_loge("%s: _mem_native_handle is NULL", __FUNCTION__);
        return;
    }

    sns_logi("(mem_handle/):hnd %p(%d) (cloned handle/):local_hnd %p(%d)",
               (void *)mem_handle, mem_handle->data[0], _mem_native_handle, _mem_native_handle->data[0]);

    _buffer_size = (int)mem_size;
    if (nullptr != _AIMapper) {
      AIMapper_Error error = AIMAPPER_ERROR_NONE;
      error = _AIMapper->v5.importBuffer(_mem_native_handle, &importedHandle);
      if(AIMAPPER_ERROR_NONE != error) {
        sns_loge("failed to import Buffer with error val=%d" , error);
        return;
      }

      auto buffer = const_cast<native_handle_t*>(importedHandle);
      const ARect region{0, 0, static_cast<int32_t>(_buffer_size), 1};
      int acquireFenceHandle =0;
      error = _AIMapper->v5.lock(buffer,
          static_cast<uint64_t>(BufferUsage::CPU_WRITE_OFTEN) |
          static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN) |
          static_cast<uint64_t>(BufferUsage::SENSOR_DIRECT_DATA),
          region, acquireFenceHandle , &_buffer_ptr);

      if(AIMAPPER_ERROR_NONE != error) {
        _AIMapper->v5.freeBuffer(buffer);
        native_handle_close(_mem_native_handle);
        native_handle_delete(_mem_native_handle);
        _mem_native_handle = NULL;
        sns_loge("failed to lock the Buffer with error val=%d", error);
        return;
      }
    } else {
        sns_loge("_AIMapper not available");
        return;
    }
    /* Increment the Android handle counter and check it for validity. Then
        assign the resulting Android handle to this Direct Channel */
    ++client_channel_handle_counter;
    if (client_channel_handle_counter <= 0) {
      client_channel_handle_counter = 1;
    }
    client_channel_handle = client_channel_handle_counter;

    /* Clear out the buffer */
    memset(_buffer_ptr, 0, (size_t)_buffer_size);

#if 0
    /* Map the shared memory to a fastRPC addressable file descriptor */
    reg_buf_attr(_buffer_ptr, _buffer_size, this->get_buffer_fd(), 0);
    reg_buf(_buffer_ptr, _buffer_size, this->get_buffer_fd());
#endif
    /* create channel for further processing */
    std::string req_msg = get_channel_req_str();
    ret = sns_direct_channel_create(_sns_rpc_handle64, (const unsigned char *)req_msg.c_str(), req_msg.length(), &_channel_handle);
    if (ret != 0) {
      sns_loge("%s: sns_low_lat_stream_init failed! ret %d", __FUNCTION__, ret);
      _channel_handle = -1;

      if(nullptr != _AIMapper) {
        auto buffer = const_cast<native_handle_t*>(importedHandle);
        _AIMapper->v5.freeBuffer(buffer);
      }
      native_handle_close(_mem_native_handle);
      native_handle_delete(_mem_native_handle);
      _mem_native_handle = NULL;
    }
    sns_logv("%s: complete ret=%d", __FUNCTION__, ret);
    return;
}


direct_channel::~direct_channel()
{
  sns_logi("%s Start" , __func__);
  if ( 0 == sns_direct_channel_delete(_sns_rpc_handle64 , _channel_handle))
    sns_logd("sns_direct_channel_delete success _sns_rpc_handle64/_sns_rpc_handle64 : %" PRIu64 "/%" PRIu64,
                            _remote_handle_fd, _sns_rpc_handle64);
  else
    sns_loge("sns_direct_channel_delete failed  _remote_handle_fd/_sns_rpc_handle64 : %" PRIu64"/%" PRIu64,
                            _remote_handle_fd, _sns_rpc_handle64);

  if(_mem_native_handle != NULL) {
    auto buffer = const_cast<native_handle_t*>(importedHandle);

    if(nullptr != _AIMapper){
      int acquireFenceHandle;
      _AIMapper->v5.unlock(buffer, &acquireFenceHandle);
      _AIMapper->v5.freeBuffer(buffer);
    }

    native_handle_close(_mem_native_handle);
    native_handle_delete(_mem_native_handle);
    _mem_native_handle = NULL;
  }
  sns_logi("%s End " , __func__);
}

int32_t direct_channel::init_rpc_channel()
{
  int ret = 0;
  std::string uri = sns_direct_channel_URI "&_dom=adsp";
  if (sensors_target::is_slpi()) {
    uri = sns_direct_channel_URI "&_dom=sdsp";
  }

  // below is required to be called per HLOS process and first call before any communication.
  // with out this skel will try to open in root pd instead of sensor pd and can throw hashing related errors
  if (!_rpc_initialized) {

    int remote_handle_open;
    if (sensors_target::is_slpi())
        remote_handle_open = remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=sdsp", &_remote_handle_fd);
    else
        remote_handle_open = remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=adsp", &_remote_handle_fd);

    if (0 == remote_handle_open) {
      if (0 == sns_direct_channel_open(uri.c_str(), &_sns_rpc_handle64)) {
        sns_logd("sns_direct_channel_open success _remote_handle_fd/_sns_rpc_handle64 : %" PRIu64 "/%" PRIu64,
          _remote_handle_fd, _sns_rpc_handle64);
      } else {
        sns_loge("sns_direct_channel_open failed  _remote_handle_fd/_sns_rpc_handle64 : %" PRIu64 "|%" PRIu64,
          _remote_handle_fd, _sns_rpc_handle64);
        ret = -1;
        if (0 != remote_handle64_close(_remote_handle_fd)) {
          sns_loge("remote_handle64_close with %" PRIu64 " failed", _remote_handle_fd);
        }
      }
    } else {
      sns_loge("remote_handle64_open failed _remote_handle_fd/_sns_rpc_handle64 : %" PRIu64 "/%" PRIu64,
                                          _remote_handle_fd, _sns_rpc_handle64);
      ret = -1;
    }
    if (ret == 0) {
       sns_logi("rpc is initialized successfully");
       _rpc_initialized = true;
    }
  } else {
    sns_logi("rpc is already initialized");
  }
  return ret;
}
std::string direct_channel::get_channel_req_str()
{
  sns_logi("%s Start" , __func__);
  std::string req_msg_str;
  sns_direct_channel_create_msg req_msg;

  req_msg.set_channel_type(DIRECT_CHANNEL_TYPE_STRUCTURED_MUX_CHANNEL);
  req_msg.set_client_proc(SNS_STD_CLIENT_PROCESSOR_APSS);
  req_msg.mutable_buffer_config()->set_size(_buffer_size);
  req_msg.mutable_buffer_config()->set_fd(this->get_buffer_fd());
  req_msg.SerializeToString(&req_msg_str);

  return req_msg_str;
}

std::string direct_channel::update_offset_req_str(int64_t new_offset)
{
  sns_logi("%s Start" , __func__);
  std::string channel_config_msg_str;
  sns_direct_channel_config_msg channel_config_msg;
  sns_direct_channel_set_ts_offset *ts_offset = channel_config_msg.mutable_set_ts_offset();

  ts_offset->set_ts_offset(new_offset);
  channel_config_msg.SerializeToString(&channel_config_msg_str);
  return channel_config_msg_str;
}

int direct_channel::update_offset(int64_t new_offset)
{
  sns_logi("%s Start" , __func__);
  int ret = 0;
  std::string req_msg = update_offset_req_str(new_offset);
  sns_logd("update_offset with %lld", (long long)new_offset);
  ret = sns_direct_channel_config(_sns_rpc_handle64, _channel_handle, (const unsigned char *)req_msg.c_str(), req_msg.length());
  if (0 != ret)
    sns_loge("sns_direct_channel_config/update_offset failed %d", ret);

  return ret;
}

int direct_channel::config_channel(int64_t timestamp_offset, const suid* sensor_suid,
                                          unsigned int sample_period_us, unsigned int flags,
                                          int sensor_handle, int sensor_type)
{
  sns_logv("%s: ", __FUNCTION__);
  std::map<int, std::unique_ptr<direct_channel_client>>::iterator it;

  if (0 == sample_period_us) {
    it = _clients_map.find(sensor_handle);
    if (it != _clients_map.end()) {
      /*client is available on this channel. So this can be removed as stop request received */
      std::string rmv_msg = _clients_map[sensor_handle]->remove_req_str();
      sns_logi("%s  remove_req_str for a sensor_handle %d" , __func__, sensor_handle);
      if( 0 != sns_direct_channel_config(_sns_rpc_handle64, _channel_handle, (const unsigned char *)rmv_msg.c_str(), rmv_msg.length())) {
        sns_loge("remove_req_str failed for existing client(%p)/sensor_handle(%d)/direct channel(%d)", _clients_map[sensor_handle].get(), sensor_handle, _channel_handle);
        return -1;
      }
      _clients_map.erase(sensor_handle);
    }
  } else {
    it = _clients_map.find(sensor_handle);
    if (it != _clients_map.end()) {
      /*available client so update the existing request*/
      std::string req_msg = _clients_map[sensor_handle]->config_req_str(sample_period_us);
      sns_logi("%s  config req - updating on the existing channel" , __func__);
      if( 0 != sns_direct_channel_config(_sns_rpc_handle64, _channel_handle, (const unsigned char *)req_msg.c_str(), req_msg.length())) {
        sns_loge("config_req failed for existing client(%p)/sensor_handle(%d)/direct channel(%d)", _clients_map[sensor_handle].get(), sensor_handle, _channel_handle);
        return -1;
      }
    } else {
      /*new client , create it and send request , store it if request is success */
      std::unique_ptr<direct_channel_client> client;
      client   = std::make_unique<direct_channel_client>(sensor_handle, *sensor_suid, flags&SNS_LLCM_FLAG_CALIBRATED_STREAM, sensor_type);
      sns_logi("new client(%p) added sensor_handle(%d)/direct channel(%d)", client.get(), sensor_handle, _channel_handle);
      if (client) {
        std::string req_msg = client->config_req_str(sample_period_us);
        sns_logi("%s  config req - Creatting new config requuest " , __func__);
        if( 0 == sns_direct_channel_config(_sns_rpc_handle64, _channel_handle, (const unsigned char *)req_msg.c_str(), req_msg.length())) {
          _clients_map[sensor_handle] = std::move(client);
          } else {
            sns_logi("config_req failed for client(%p)/sensor_handle(%d)/direct channel(%d)", client.get(), sensor_handle, _channel_handle);
            return -1;
        }
      }
      else {
        sns_logi("failed to create client for sensor_handle(%d)/direct channel(%d)", sensor_handle, _channel_handle);
        return -1;
      }
    }

    /*update the offset request*/
    if( 0 != update_offset(timestamp_offset)) {
      sns_loge("update_offset failed for sensor_handle(%d)/direct channel(%d)", sensor_handle, _channel_handle);
      return -1;
    }
  }
  return 0;
}

int direct_channel::stop_channel()
{
  int ret = 0;
  sns_logv("%s: ", __FUNCTION__);
  std::map<int, std::unique_ptr<direct_channel_client>>::iterator it;
  int idx = 0;
  for (auto it = _clients_map.begin(); it != _clients_map.end(); ++it) {
    std::unique_ptr<direct_channel_client> temp = std::move(it->second);
    sns_logi("delete client(%p)/direct channel(%d)", temp.get(), _channel_handle);
    if(nullptr == temp.get()) {
      sns_logi("Current client is already removed as part of config request");
      continue;
    }
    sns_logi("%s  Remove  req " , __func__);
    std::string msg = temp->remove_req_str();
    if( 0 != sns_direct_channel_config(_sns_rpc_handle64, _channel_handle, (const unsigned char *)msg.c_str(), msg.length())) {
      sns_loge("config_req remove stream failed client(%p)/direct channel(%d)", temp.get(), _channel_handle);
      return -1;
    }
    temp.reset();
    _clients_map.erase(idx);
    idx++;
  }
  return ret;
}

int direct_channel::get_low_lat_handle()
{
  return _channel_handle;
}

int direct_channel::get_client_channel_handle()
{
  return client_channel_handle;
}

int direct_channel::get_buffer_fd()
{
    if (_mem_native_handle) {
        return _mem_native_handle->data[0];
    } else {
        return -1;
    }
}

bool direct_channel::get_buffer_fd(buffer_handle_t buffer_handle, uint64_t *outBufferId)
{

  using Value = typename android::hardware::graphics::mapper::StandardMetadata<StandardMetadataType::BUFFER_ID>::value;

  sns_logi("get_buffer_fd start ");

  std::vector<uint8_t> metadata_buffer;
  metadata_buffer.resize(512);

  int32_t desired_size = 0;
  if(nullptr != _AIMapper) {
  desired_size = _AIMapper->v5.getStandardMetadata(buffer_handle,
          static_cast<int64_t>(StandardMetadataType::BUFFER_ID),
          metadata_buffer.data(), metadata_buffer.size());
  } else {
    sns_loge("_AIMapper is not available ");
    return false;
  }

  if(desired_size < 0) {
    sns_loge("Buffer seems to be invalid");
    return false;
  }

  if(desired_size > metadata_buffer.size()) {
    metadata_buffer.resize(metadata_buffer.size());
    desired_size = _AIMapper->v5.getStandardMetadata(buffer_handle,
              static_cast<int64_t>(StandardMetadataType::BUFFER_ID),
              metadata_buffer.data(), metadata_buffer.size());
  }

  auto value = Value::decode(metadata_buffer.data(), desired_size);
  if (value.has_value()) {
    *outBufferId = *value;
    return true;
  } else {
    sns_loge("Able to read the buffer but it doesn't have the value ");
    return false;
  }
}

bool direct_channel::is_same_memory(const struct native_handle *mem_handle)
{
  bool ret = false;
  uint64_t id1 = 0, id2 = 0;

  buffer_handle_t temp_imported_handle;
  AIMapper_Error error = AIMAPPER_ERROR_NONE;
  if (nullptr != _AIMapper) {
    error = _AIMapper->v5.importBuffer(mem_handle, &temp_imported_handle);
    if(AIMAPPER_ERROR_NONE != error) {
      sns_loge("failed to import Buffer with error val=%d" , error);
      return false;
    }
  }else {
    sns_loge("_AIMapper is not available ");
    return false;
  }

  ret = get_buffer_fd(const_cast<native_handle_t*>(temp_imported_handle) , &id1);
  if(false == ret) {
    sns_loge("failed to get the buffer id for new buffer");
    return false;
  }
  ret = get_buffer_fd(const_cast<native_handle_t*>(importedHandle) , &id2);
  if(false == ret) {
    sns_loge("failed to get the buffer id for previous buffer");
    return false;
  }

  if (nullptr != _AIMapper) {
    _AIMapper->v5.freeBuffer(temp_imported_handle);
  }

  return (id1 == id2);
}

int direct_channel::init_mapper(void)
{
  int ret = 0;

  sns_logi("init_mapper start ");

  AIMapper_Error error = AIMapper_loadIMapper(&_AIMapper);
  if(error != AIMAPPER_ERROR_NONE || nullptr == _AIMapper) {
    sns_loge("%s failed to load Mapper ", __FUNCTION__);
    return -1;
  }

  return 0;
}
void direct_channel::reset()
{
  sns_logi("reset rpc resources");
  if (_rpc_initialized) {
    if (0 == sns_direct_channel_close(_sns_rpc_handle64)) {
      sns_logd("sns_direct_channel_close success _remote_handle_fd/_sns_rpc_handle64 : %" PRIu64 "/%" PRIu64,
                              _remote_handle_fd, _sns_rpc_handle64);
    }
    _sns_rpc_handle64 = 0;
    remote_handle64_close(_remote_handle_fd);
    _remote_handle_fd = 0;
    _rpc_initialized = false;
    sns_logi("reset is completed");
  } else {
    sns_logi("rpc was not initialized, not need to reset");
  }
  sns_logi("reset is successful");
}
