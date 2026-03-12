/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdexcept>
#include <cinttypes>
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "glinkSession.h"
#include "glinkSessionError.h"
#include "sns_client.pb.h"
#include "sensors_ssr.h"
#include "suid_lookup.h"

using namespace ::com::quic::sensinghub::session::V1_0::implementation;

#define SNS_GLINK_MAX_REQUEST_SIZE 1024
#define SNS_GLINK_MAX_INDICATION_SIZE 4096
#define SNS_GLINK_ACK_TIMEOUT_MAX_RETRY 100
#define SNS_GLINK_ACK_TIMEOUT_MS 5
static const uint64_t NSEC_PER_SEC = 1000000000ull;
const uint32_t GLINK_ERRORS_HANDLED_ATTEMPTS = 10; //TODO
bool glinkSession::_is_thread_created = false;
std::atomic<int> glinkSession::_chnl_fd = -1;
std::atomic<uint32_t> glinkSession::_ack_conn_hndl = -1;
std::atomic<bool> glinkSession::_is_req_success = false;
sns_wakelock* glinkSession::_wakelock_inst = NULL;
std::string glinkSession::_chnl_name = "";
uint32_t glinkSession::_glink_err_cnt = 0;
std::atomic<uint32_t> glinkSession::_glink_clients = 0;
int glinkSession::_wakeup_pipe[2];
std::mutex glinkSession::_clientid_conn_db_mutex;
std::mutex glinkSession::_glink_read_mutex;
std::unordered_map<uint32_t, std::unordered_map<suid, glinkSession*, cb_sensorUID_hash>> glinkSession::_clientid_conn_db;
std::unordered_map<suid, vector<uint32_t>, cb_sensorUID_hash> glinkSession::_suid_conn_handles_db;
std::mutex glinkSession::_conn_hndl_mutex;
std::deque<stream_req_info> glinkSession::_resp_queue;
std::mutex glinkSession::_resp_queue_mutex;
std::vector<std::string> glinkSession::_glink_chnls_list;
std::thread glinkSession::_glink_read_threadhdl;

glinkSession::glinkSession(int no_of_chnls, int hub_id) :
    _is_ftrace_enabled(false),
    _reconnecting(false),
    _connection_closed(false)
{
  sns_logd("glinkSession conn = 0x%llx constructor", (uint64_t)this);
  if(0 == _glink_chnls_list.size())
  {
    for(int id = 0; id < no_of_chnls; id++)
    {
      string chnl_name = "qsh_" + to_string(hub_id) + "_" + to_string(id);
      _glink_chnls_list.push_back(chnl_name);
    }
  }
  _worker = make_unique<worker>();
}

int glinkSession::open()
{
  try
  {
     glink_connect();
     if(_wakelock_inst == nullptr)
       _wakelock_inst = sns_wakelock::get_instance();
     _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_INIT;
  }
  catch(const exception& e)
  {
    sns_loge("%s failed: %s", __func__, e.what());
     return -1;
  }
  return 0;
}

void glinkSession::close()
{
  _connection_closed = true;
  /* Delete this object's entry in _client_id_conn_db */
  _cb_map_table_mutex.lock();
  for(auto& it : _callback_map_table)
  {
    update_clientid_conn_db(it.first);
  }
  _callback_map_table.clear();
  _cb_map_table_mutex.unlock();
  glink_disconnect(true);
}

glinkSession::~glinkSession()
{
  sns_logi("glinkSession conn = 0x%llx destructor", (uint64_t)this);
  _callback_map_table.clear();
  _worker.reset();
}

int glinkSession::glink_open()
{
  int fd = -1;
  int retry_count = 0;
  bool is_open_flock_success = false;

  for(auto name : _glink_chnls_list)
  {
    string local_name = GLINK_CHNL_PREFIX + name;
    sns_logi("glinkSession about to open glink channel on node = %s", name.c_str());

    do
    {
      fd = ::open(local_name.c_str(), O_RDWR|O_NONBLOCK);
      if(fd >= 0)
        break;
      else
      {
        if(errno == ETIMEDOUT)
        {
          retry_count++;
          fd = -ETIMEDOUT;
          sns_loge("glinkSession glink channel open timeout on node = %s, retries count =  %d", name.c_str(), retry_count);
          sleep(1);
        }
        else
        {
          sns_loge("glinkSession open on node = %s failed with errno = %d, %s", name.c_str(), errno, strerror(errno));
          fd = -errno;
          break;
        }
      }
    } while (retry_count != MAX_GLINK_OPEN_RETRIES);

    if(fd == -ETIMEDOUT)
    {
      break;
    }
    else if(fd < 0)
    {
      sns_loge("not able to open channel = %s", local_name.c_str());
      continue;
    }
    else
    {
      sns_logi("glinkSession: glink open successful, fd = %d", fd);
      if(flock(fd, LOCK_EX|LOCK_NB) != 0)
      {
        sns_loge("flock failed, trying for next channel");
        ::close(fd);
        continue;
      }
      else
      {
        sns_logi("flock acquired successfully");
        _chnl_name = name;
        is_open_flock_success = true;
        break;
      }
    }
  }

  if(!is_open_flock_success)
  {
    sns_logi("glinkSession: all glink channels are occupied at the moment");
    return -1;
  }
  return fd;
}

void glinkSession::glink_connect()
{
  sns_logd("glinkSession glink_connect waiting for _s_glink_read_mutex");
  _glink_read_mutex.lock();

  if(_chnl_fd > 0)
  {
    sns_logi("glinkSession channel is already opened");
    glinkSession::_glink_clients++;
    _glink_read_mutex.unlock();
    return;
  }

  if((_chnl_fd = glink_open()) >  0)
  {
    sns_logi("Glink open successful");
  }

  if(_chnl_fd < 0)
  {
    _glink_read_mutex.unlock();
    throw glink_error(_chnl_fd, "glinkSession open failed");
  }

  glinkSession::_glink_clients++;
  if((_chnl_fd > 0) && (!_is_thread_created))
  {
    /*Create PIPE to send custom message to glink poll thread during channel close */
    if(pipe(_wakeup_pipe) == -1)
    {
      _glink_read_mutex.unlock();
      throw glink_error(_wakeup_pipe[0], "glinkSession pipe creation failed");
    }
    _glink_read_threadhdl = std::thread(glink_thread_routine);
    _is_thread_created = true;
  }
  _glink_read_mutex.unlock();
}

int glinkSession::glink_read(int fd, uint8_t* buf, size_t buf_len)
{
  int ret_val = -1;
  int retry_count = 0;

  sns_logi("glinkSession glink_read fd = %d buf_len = %zu", fd, buf_len);
  while(ret_val == -1)
  {
    ret_val = ::read(fd, buf, buf_len);
    if(ret_val == -1)
    {
      if (EAGAIN == errno)
      {
        sns_loge("glinkSession read EAGAIN has occured on fd = %d", fd);
        retry_count++;
        if(MAX_GLINK_READ_RETRIES == retry_count)
        {
          _glink_err_cnt++;
          sns_loge("glinkSession read MAX_GLINK_READ_RETRIES = %d has finished & g_glink_err_cnt = %d",
                    MAX_GLINK_READ_RETRIES, _glink_err_cnt);
          return -EAGAIN;
        }
        continue;
      }
      else
      {
        _glink_err_cnt++;
        sns_loge("glinkSession read failed with errno = %d, %s", errno, strerror(errno));
        return -errno;
      }
    }
  }

  sns_logi("glinkSession complete read bytes = %d", ret_val);
  return ret_val;
}

void glinkSession::glink_thread_routine()
{
  struct pollfd poll_fd[2];
  uint8_t* encoded_msg = nullptr;
  sns_client_glink_msg pb_glink_msg;
  ssize_t ret_val = 0;
  uint8_t ch;
  bool exit = false;

  while(!exit)
  {
    poll_fd[0].fd     = _wakeup_pipe[0];
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd     = _chnl_fd;
    poll_fd[1].events = POLLIN | POLLHUP;

    sns_logi("glinkSession waiting on poll wakeup_pipe %d glink_fd %d", poll_fd[0].fd, poll_fd[1].fd);
    ret_val = poll(poll_fd, 2, POLL_TIMEOUT_IN_MS);
    if(ret_val > 0)
    {
      if(poll_fd[0].revents & POLLIN)
      {
        ::read(poll_fd[0].fd, &ch, 1);
        if(ch == 'd')
        {
            sns_logi("glinkSession: explicitly requested thread close");
            exit = true;
        }
      }
      else if(poll_fd[1].revents & POLLIN)
      {
        sns_logi("glinkSession: received POLLIN event");
        encoded_msg = new uint8_t[SNS_GLINK_MAX_INDICATION_SIZE];
        ret_val = glink_read(poll_fd[1].fd, encoded_msg, SNS_GLINK_MAX_INDICATION_SIZE);

        if(ret_val > 0)
        {
          bool mutex_unlock_success = false;
          sns_logd("glinkSession: waiting for _s_glink_read_mutex");
          _glink_read_mutex.lock();
          sns_logd("glinkSession: acquired _s_glink_read_mutex");
          pb_glink_msg.ParseFromArray(encoded_msg, ret_val);
          delete encoded_msg;

          if(pb_glink_msg.has_connect_ack())
          {
            /* Ack event is received only for connect request. */
            sns_logd("glinkSession: received connect_ack event ");
            _ack_conn_hndl = pb_glink_msg.connect_ack().has_connection_handle() ?
                              pb_glink_msg.connect_ack().connection_handle() : -1;
            sns_logi("glinkSession: received conn_hndl = %d",
                        pb_glink_msg.connect_ack().has_connection_handle() ?
                              pb_glink_msg.connect_ack().connection_handle() : -1);
            _is_req_success.store(true);
          }
          else if(pb_glink_msg.has_resp())
          {
            sns_logd("glinkSession: received resp event");
            sns_client_glink_resp resp_msg = pb_glink_msg.resp();
            sns_logd("glinkSession: Acquiring _resp_queue_mutex");
            _resp_queue_mutex.lock();
            if(!_resp_queue.empty())
            {
              auto resp_info = _resp_queue.front();
              _resp_queue.pop_front();
              _resp_queue_mutex.unlock();
              auto conn = get_conn(resp_info.conn_hndl, resp_info.sensor_uid);
              if(conn == nullptr)
              {
                sns_loge("glinkSession: conn is null. Cannot send the resp");
              }
              else
              {
                if(conn->_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE) {
                  conn->glink_response_handler(resp_info, resp_msg);
                  mutex_unlock_success = true;
                }
                else {
                  sns_loge("glinkSession: connection not yet active. Dropping packet with conn = 0x%llx and conn_handle %d",
                                       (uint64_t)conn, resp_info.conn_hndl);
                }
              }
            }
            else
            {
              sns_loge("glinkSession: response queue is empty");
              _resp_queue_mutex.unlock();
            }
          }
          else if(pb_glink_msg.has_ind())
          {
            sns_client_glink_ind ind_msg = pb_glink_msg.ind();
            uint32_t ind_conn_hndl;
            suid sensors_uid;
            if(ind_msg.has_connection_handle())
            {
              ind_conn_hndl = ind_msg.connection_handle();
              sns_logd("glinkSession: received indication for conn_hndl = %d", ind_conn_hndl);
            }
            else
            {
              sns_loge("glinkSession: ind does not have connection handle. Cannot notify event.");
              _glink_read_mutex.unlock();
              return;
            }
            if(ind_msg.has_event())
            {
              if(get_suid(sensors_uid, ind_msg) != -1)
              {
                auto conn = get_conn(ind_conn_hndl, sensors_uid);
                if(conn != nullptr)
                {
                  if(conn->_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
                  {
                    conn->glink_indication_handler(sensors_uid, ind_msg);
                    mutex_unlock_success = true;
                  }
                  else
                    sns_loge("glinkSession: connection not yet active. Dropping packet with conn = 0x%llx and conn_handle %d",
                                       (uint64_t)conn, ind_conn_hndl);
                }
              }
              else
              {
                sns_loge("glinkSession: ind does not have value. Cannot notify event.");
              }
            }
          }
          if(!mutex_unlock_success)
            _glink_read_mutex.unlock();
        }
        else
        {
          sns_loge("glinkSession: read has failed with errno = %d, g_glink_err_cnt = %d", errno, _glink_err_cnt);
        }
      }
      else if(poll_fd[1].revents & POLLHUP)
      {
        sns_loge("glinkSession: POLLHUP event is received, AON CLIENT SSR occured");
        glink_error_handler(POLLHUP);
      }
      else
      {
        sns_loge("glinkSession received unregistered pollevent = %d", (int)poll_fd[1].revents);
      }
    }
    else if(ret_val == 0)
    {
      sns_loge("glinkSession: poll time-out");
    }
    else
    {
      sns_loge("glinkSession: poll failed with errno = %d, g_glink_err_cnt = %d",
                         errno, _glink_err_cnt);
    }

    if (0 > ret_val)
    {
      glink_error_handler(ret_val);
    }
    else
    {
      if(_glink_err_cnt > 0)
      {
        _glink_err_cnt = 0;
      }
    }
  }
  sns_logd("glinkSession thread exit");
}

void glinkSession::glink_response_handler(stream_req_info& curr_resp, sns_client_glink_resp& resp_msg)
{
  sns_logd("glinkSession: %s start", __func__);
  if (_connection_closed)
  {
    sns_logi("glinkSession response is coming while connection is being closed");
    _glink_read_mutex.unlock();
    return;
  }

  sns_logv("glinkSession: Acquiring _cb_map_table_mutex");
  _cb_map_table_mutex.lock();
  auto it = _callback_map_table.find(curr_resp.sensor_uid);
  if(it == _callback_map_table.end())
  {
    sns_logd("glinkSession: %s suid NOT found in _callback_map_table table suid_low=%" PRIu64 " suid_high=%" PRIu64 ", this=%px" ,
                                __func__, curr_resp.sensor_uid.low, curr_resp.sensor_uid.high, this);
    _cb_map_table_mutex.unlock();
    sns_logv("glinkSession: Releasing _cb_map_table_mutex");
    _glink_read_mutex.unlock();
    return;
  }
  respCallBack current_resp_cb = it->second.get_resp_cb();

  if(current_resp_cb && resp_msg.has_error() && resp_msg.has_connection_handle())
  {
    sns_logd("glinkSession: %s suid found in _callback_map_table table and trigerring resp_cb suid_low=%" PRIu64 " suid_high=%" PRIu64 " client_connect_id=%d , this=%px" ,
            __func__, curr_resp.sensor_uid.low, curr_resp.sensor_uid.high, resp_msg.connection_handle(), this);
    current_resp_cb(resp_msg.error(), resp_msg.connection_handle());
  }
  _glink_read_mutex.unlock();

  if(true == curr_resp.is_disable_req)
  {
    _callback_map_table.erase(it);
    update_clientid_conn_db(curr_resp.sensor_uid);
  }
  _cb_map_table_mutex.unlock();

  sns_logd("glinkSession: %s Ended this=%px" , __func__, this);
}

void glinkSession::glink_error_handler(int error)
{
  sns_logi("glinkSession: glink_error_handler received error = %d", error);
  if(error == POLLHUP)
  {
    sns_logd("glinkSession: Inside POLLHUP error handler. Acquiring _resp_queue_mutex");
    _resp_queue_mutex.lock();
    _resp_queue.clear();
    _resp_queue_mutex.unlock();
    sns_logv("glinkSession: Acquiring _clientid_conn_db_mutex");

    _clientid_conn_db_mutex.lock();
    for(auto& it : _clientid_conn_db)
    {
      auto& suid_conn_map = it.second;
      for(auto itr : suid_conn_map)
      {
        itr.second->_reconnecting = true;
      }
    }
    _clientid_conn_db_mutex.unlock();
    sns_logv("glinkSession: Released _clientid_conn_db_mutex");
    glink_disconnect(false);

    try
    {
      sns_logd("glinkSession: glink error, trying to reconnect");
      glink_connect();
      sns_logi("glinkSession: connection re-established");
    }
    catch (const exception& e)
    {
      sns_loge("glinkSession: could not reconnect: %s", e.what());
      return;
    }

    sns_logv("glinkSession: Acquiring _clientid_conn_db_mutex");
    _clientid_conn_db_mutex.lock();
    for(auto& it : _clientid_conn_db)
    {
      auto& suid_conn_map = it.second;
      for(auto itr : suid_conn_map)
      {
        glinkSession* conn = itr.second;
        conn->_worker->add_task([conn] {
          conn->_reconnecting = false;
          if (conn->_connection_closed == true)
          {
            sns_logd("glinkSession: conn = 0x%llx sensor deactivated during ssr", (uint64_t)conn);
            return;
          }

          for(auto iter = conn->_callback_map_table.begin(); iter != conn->_callback_map_table.end(); ++iter)
          {
            errorCallBack err_cb = (iter->second).get_error_cb();
            if(nullptr == err_cb)
            {
              continue;
            }
            err_cb(ISession::RESET);
          }
        });
      }
    }
    _clientid_conn_db_mutex.unlock();
    sns_logv("glinkSession: Released _clientid_conn_db_mutex");
  }
  else
  {
    sns_logv("glinkSession: Acquiring _glink_read_mutex");
    _glink_read_mutex.lock();
    _glink_err_cnt++;
    if((_glink_err_cnt > GLINK_ERRORS_HANDLED_ATTEMPTS) && !trigger_ssr())
    {
      sns_logd("glinkSession: triggred ssr g_glink_err_cnt = %d", _glink_err_cnt);
      _glink_err_cnt = 0;
    }
    _glink_read_mutex.unlock();
    sns_logv("glinkSession: Released _glink_read_mutex");
  }
}

void glinkSession::glink_disconnect(bool thread_exit)
{
  sns_logv("glinkSession: Acquiring _glink_read_mutex");
  _glink_read_mutex.lock();
  if(thread_exit)
  {
    glinkSession::_glink_clients--;
    if(glinkSession::_glink_clients > 0)
    {
      sns_logi("glinkSession: Not the last conn. Do not close the chnl");
      _glink_read_mutex.unlock();
      return;
    }
  }

  if(thread_exit)
  {
    if(::write(_wakeup_pipe[1], "d", 1) < 0)
    {
      sns_loge("glinkSession: write err on wakup pipe errrno = %d no way to join readthread", errno);
    }
    else
    {
    _glink_read_threadhdl.join();
    _is_thread_created = false;
    _resp_queue.clear();
    ::close(_wakeup_pipe[0]);
    ::close(_wakeup_pipe[1]);
    }
  }
  if(::close(_chnl_fd) != 0)
  {
    sns_loge("glinkSession: close on _s_chnl_fd = %d failed with errno = %d", _chnl_fd.load(), errno);
  }
  _chnl_fd = -1;
  _chnl_name = "";
  _glink_read_mutex.unlock();
}

int glinkSession::get_suid(suid &out_suid, sns_client_glink_ind& ind_msg)
{
  sns_client_event_msg pb_event_msg = ind_msg.event();
  if( false == pb_event_msg.has_suid())
  {
    sns_logi("glinkSession: Message does not have suid");
    return -1;
  }

  sns_logv("glinkSession: Done parsing ind message");
  out_suid.low = pb_event_msg.suid().suid_low();
  out_suid.high = pb_event_msg.suid().suid_high();
  return 0;
}

void glinkSession::glink_indication_handler(suid& sensor_uid, sns_client_glink_ind& ind_msg)
{
  sns_logd("glinkSession: conn = 0x%llx received indication", (uint64_t)this);

  if(_connection_closed)
  {
    sns_logd("glinkSession: conn = 0x%llx indication is coming while connection is being closed", (uint64_t)this);
    _glink_read_mutex.unlock();
    return;
  }

  uint64_t sample_received_ts = android::elapsedRealtimeNano();

  sns_logv("glinkSession: Acquiring _cb_map_table_mutex");
  _cb_map_table_mutex.lock();
  auto it = _callback_map_table.find(sensor_uid);
  if(it == _callback_map_table.end())
  {
    sns_loge("glinkSession: No callbacks registered for suid_low=%" PRIu64 " suid_high=%" PRIu64"  ",
                    sensor_uid.low, sensor_uid.high);
    _cb_map_table_mutex.unlock();
    _glink_read_mutex.unlock();
    return;
  }
  eventCallBack current_event_cb = it->second.get_event_cb();
  _cb_map_table_mutex.unlock();
  if(nullptr == current_event_cb)
  {
    sns_loge("glinkSession: event cb is not registered ");
    _glink_read_mutex.unlock();
    return;
  }

  string pb_ind_msg_encoded;
  ind_msg.event().SerializeToString(&pb_ind_msg_encoded);
  _glink_read_mutex.unlock();
  current_event_cb((uint8_t*)pb_ind_msg_encoded.c_str(), pb_ind_msg_encoded.length(), sample_received_ts);
  sns_logv("glinkSession: triggered event callback");
}

uint32_t glinkSession::get_free_conn_handle(suid sensor_uid)
{
  sns_logd("glinkSession: %s called", __func__);
  uint32_t conn_handle = -1;

  /* Acquire mutex so that only one conn request is sent at a time */
  sns_logv("glinkSession: Acquiring _clientid_conn_db_mutex");
  _clientid_conn_db_mutex.lock();
  if(_clientid_conn_db.size() == 0 ||
       ((_suid_conn_handles_db.count(sensor_uid) != 0) &&
        (_suid_conn_handles_db[sensor_uid].size() == _clientid_conn_db.size())))
  {
    /* request new conn handle */
    sns_client_glink_msg pb_glink_msg;
    sns_client_glink_connect* pb_glink_connect = new sns_client_glink_connect;
    string pb_glink_msg_encoded;
    pb_glink_msg.set_allocated_connect(pb_glink_connect);
    sns_logi("glinkSession: acquired glink connect");
    pb_glink_msg.SerializeToString(&pb_glink_msg_encoded);
    sns_logi("glinkSession: set the message to string");
    if(send_glink_req(sensor_uid, pb_glink_msg_encoded, false) != -1)
    {
      sns_logi("glinkSession: Request sent. Waiting for ack event");
      bool timeout = true;
      int max_retry = SNS_GLINK_ACK_TIMEOUT_MAX_RETRY;
      do
      {
        if(_is_req_success.load())
        {
          _is_req_success.store(false);
          timeout = false;
          conn_handle = _ack_conn_hndl;
          sns_logi("glinkSession: assigning conn_hndl = %d", conn_handle);
          _ack_conn_hndl = -1;
          break;
        }
        else
        {
          this_thread::sleep_for(chrono::milliseconds(SNS_GLINK_ACK_TIMEOUT_MS));
        }
        max_retry--;
      } while(max_retry > 0);

      if(timeout)
      {
        sns_loge("Timed out waiting for conn ack event");
        conn_handle = -1;
      }
    }
    _clientid_conn_db_mutex.unlock();
    sns_logv("glinkSession: Released _clientid_conn_db_mutex");
  }
  else
  {
    _clientid_conn_db_mutex.unlock();
    sns_logv("glinkSession: Released _clientid_conn_db_mutex");
    conn_handle = find_free_conn_handle(sensor_uid);
  }
  sns_logd("glinkSession: %s ended", __func__);
  return conn_handle;
}

uint32_t glinkSession::find_free_conn_handle(suid sensor_uid)
{
  sns_logv("glinkSession: Acquiring _clientid_conn_db_mutex");
  lock_guard<mutex> lk(_clientid_conn_db_mutex);
  auto it = _clientid_conn_db.begin();
  if(_suid_conn_handles_db.count(sensor_uid) == 0)
  {
    return it->first;
  }

  auto conn_hndls = _suid_conn_handles_db[sensor_uid];
  for(; it != _clientid_conn_db.end(); it++)
  {
    uint32_t hndl = it->first;
    if(find(conn_hndls.begin(), conn_hndls.end(), hndl) == conn_hndls.end())
    {
      return hndl;
    }
  }
  return -1;
}

void glinkSession::update_suid_conn_hndl_db(uint32_t hndl, suid& sensor_uid)
{
  if(_suid_conn_handles_db.count(sensor_uid) == 0)
    return;

  try {
    auto& hndls = _suid_conn_handles_db.at(sensor_uid);
    auto it = find(hndls.begin(), hndls.end(), hndl);
    if(it != hndls.end())
    {
      hndls.erase(it);
      sns_logd("glinkSession: erased hndl = %d from suid_low=%" PRIu64 " ", hndl, sensor_uid.low);
      if(hndls.size() == 0)
      {
        auto itr = _suid_conn_handles_db.find(sensor_uid);
        if(itr != _suid_conn_handles_db.end())
          _suid_conn_handles_db.erase(itr);
      }
    }
  }
  catch(const out_of_range& ex)
  {
    sns_loge("glinkSession: conn_hndl %d does not exist for suid_low=%" PRIu64 " suid_high=%" PRIu64"  ",
                            hndl, sensor_uid.low, sensor_uid.high);
  }
  sns_logd("Exiting %s", __func__);
}

void glinkSession::send_disconnect_req(suid &sensor_uid, uint32_t conn_hndl)
{
  sns_logd("glinkSession: Sending disconnect req for conn_hndl = %d", conn_hndl);
  sns_client_glink_msg pb_glink_req;
  string pb_glink_req_encoded;

  pb_glink_req.mutable_disconnect()->set_connection_handle(conn_hndl);
  pb_glink_req.SerializeToString(&pb_glink_req_encoded);
  send_glink_req(sensor_uid, pb_glink_req_encoded, false);
  sns_logd("glinkSession: Exited %s", __func__);
}

glinkSession* glinkSession::get_conn(uint32_t& conn_hndl, suid& sensor_uid)
{
  glinkSession* conn = nullptr;
  try
  {
    auto &suid_conn_mp = _clientid_conn_db.at(conn_hndl);
    try
    {
      conn = suid_conn_mp.at(sensor_uid);
    }
    catch(const out_of_range& ex)
    {
      sns_loge("glinkSession: conn object does not exist for conn_hndl %d and suid_low=%" PRIu64 " suid_high=%" PRIu64"  ",
                                conn_hndl, sensor_uid.low, sensor_uid.high);
      return nullptr;
    }
  }
  catch(const out_of_range& ex)
  {
      sns_loge("glinkSession: conn_hndl %d does not exist for suid_low=%" PRIu64 " suid_high=%" PRIu64"  ",
                                  conn_hndl, sensor_uid.low, sensor_uid.high);
      return nullptr;
  }
  return conn;
}

void glinkSession::update_clientid_conn_db(suid sensor_uid)
{
  sns_logd("glinkSession: Entering %s. Acquiring _clientid_conn_db_mutex.", __func__);

  _clientid_conn_db_mutex.lock();
  for(auto it = _clientid_conn_db.begin(); it != _clientid_conn_db.end(); )
  {
    bool entry_updated = false;
    auto& suid_conn_mp = it->second;
    sns_logd("glinkSession: conn_hndl = %d", it->first);
    auto itr = suid_conn_mp.find(sensor_uid);
    if(itr != suid_conn_mp.end() && this == itr->second)
      {
        suid_conn_mp.erase(itr);
        sns_logd("Erasing entry for conn %llx from _clientid_conn_db", (uint64_t)this);
        update_suid_conn_hndl_db(it->first, sensor_uid);
        if(0 == suid_conn_mp.size())
        {
          send_disconnect_req(sensor_uid, it->first);
          sns_logd("Erasing entry from _clientid_conn_db");
          it = _clientid_conn_db.erase(it);
          entry_updated = true;
        }
        break;
      }
    if(false == entry_updated)
      it++;
  }
  _clientid_conn_db_mutex.unlock();
  sns_logd("glinkSession: Exiting %s. Released _clientid_conn_db_mutex.", __func__);
}

int glinkSession::setCallBacks(suid sensor_uid, ISession::respCallBack resp_cb, ISession::errorCallBack error_cb, ISession::eventCallBack event_cb)
{
  sns_logd("glinkSession: %s start this=%llx" , __func__, (uint64_t)this);
  uint32_t conn_hndl = -1;

  sns_logv("glinkSession: Acquiring _cb_map_table_mutex");
  lock_guard<mutex> lk(_cb_map_table_mutex);
  auto it = _callback_map_table.find(sensor_uid);
  if (it == _callback_map_table.end())
  {
    sns_logi("glinkSession: suid(low=%" PRIu64 ",high=%" PRIu64 ") is new. Register callbacks", sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr)
    {
      sns_loge("glinkSession:All callbacks are null for new suid. No need to register it");
      return -1;
    }
    qsh_register_cb cb(resp_cb, error_cb, event_cb);
    _callback_map_table.insert(std::pair<suid, qsh_register_cb>(sensor_uid, cb));
    conn_hndl = get_free_conn_handle(sensor_uid);
    sns_logi("glinkSession conn_hndl = %d", conn_hndl);
    if(conn_hndl == -1)
    {
      return -1;
    }

    sns_logv("glinkSession: Acquiring _clientid_conn_db_mutex");
    _clientid_conn_db_mutex.lock();
    _clientid_conn_db[conn_hndl][sensor_uid] = this;
    _suid_conn_handles_db[sensor_uid].push_back(conn_hndl);
    _clientid_conn_db_mutex.unlock();
    sns_logv("glinkSession: Released _clientid_conn_db_mutex");
  }
  else
  {
    sns_logi("glinkSession: suid(low=%" PRIu64 ",high=%" PRIu64 ") is alread registered", sensor_uid.low, sensor_uid.high);
    if (resp_cb == nullptr && error_cb == nullptr && event_cb == nullptr)
    {
      sns_logi("glinkSession: all callbacks are null, unregister it");
      _callback_map_table.erase(it);
      update_clientid_conn_db(sensor_uid);
      return -1;
    }
    else
    {
      sns_logi("glinkSession: update callbacks with new one");
      qsh_register_cb cb(resp_cb,error_cb,event_cb);
      it->second = cb;
    }
  }
  sns_logd("glinkSession: %s completed, this=%px ", __func__, this);
  return 0;
}


int glinkSession::glink_write(int fd, uint8_t* buf, size_t buf_len)
{
  ssize_t ret_val = 0;
  int write_bytes = 0;
  int retry_count = 0;

  sns_logd("glinkSession conn = 0x%llx write fd = %d buf_len = %zu, waiting for _s_glink_read_mutex",
                       (uint64_t)this, fd, buf_len);

  _glink_read_mutex.lock();
  if(_chnl_fd == -1)
  {
    sns_loge("glinkSession conn = 0x%llx write is not permitted as _s_chnl_fd is -1", (uint64_t)this);
    _glink_read_mutex.unlock();
    return 0;
  }

  SNS_TRACE_BEGIN("sensors::glink_write");
  if(_wakelock_inst != nullptr)
  {
    sns_logd("glinkSession acquiring wakelock");
    _wakelock_inst->get_n_locks(1);
  }

  if(_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_INIT)
    _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE;

  while((buf_len != 0) && ((ret_val = ::write(fd, buf, buf_len)) != 0))
  {
    if(-1 == ret_val)
    {
      if (EAGAIN == errno||(EBUSY == errno))
      {
        retry_count++;
        sns_loge("glinkSession conn = 0x%llx write EAGAIN has occured", (uint64_t)this);
        if(MAX_GLINK_WRITE_RETRIES == retry_count )
        {
          _glink_err_cnt++;
          sns_loge("glinkSession conn = 0x%llx write MAX_GLINK_WRITE_RETRIES = %d has finished on fd = %d & g_glink_err_cnt = %d",
                    (uint64_t)this, MAX_GLINK_WRITE_RETRIES, fd, _glink_err_cnt);
          _glink_read_mutex.unlock();
          if(_wakelock_inst != nullptr)
          {
            sns_logd("glinkSession releasing wakelock");
            _wakelock_inst->put_n_locks(1);
          }
          SNS_TRACE_END();

          if(_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
            _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_INIT;
          return -errno;
        }
        usleep(5*1000);
        continue;
      }
      else
      {
        _glink_err_cnt++;
        sns_loge("glinkSession conn = 0x%llx write has failed with errno = %d & g_glink_err_cnt = %d",
                        (uint64_t)this, errno, _glink_err_cnt);
        _glink_read_mutex.unlock();
        if(_wakelock_inst != nullptr)
        {
          sns_logd("glinkSession releasing wakelock");
          _wakelock_inst->put_n_locks(1);
        }
        SNS_TRACE_END();

        if(_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
          _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_INIT;
        return -errno;
      }
    }
    sns_logd("glinkSession conn = 0x%llx partial bytes written= %d", (uint64_t)this, ret_val);
    buf_len     -= ret_val;
    buf         += ret_val;
    write_bytes += ret_val;
  }
  _glink_read_mutex.unlock();
  sns_logi("glinkSession conn = 0x%llx complete bytes written= %d retries %d", (uint64_t)this, write_bytes, retry_count);
  if(_wakelock_inst != nullptr)
  {
    sns_logd("glinkSession releasing wakelock");
    _wakelock_inst->put_n_locks(1);
  }
  SNS_TRACE_END();
  return ret_val;
}

uint32_t glinkSession::find_assigned_conn_handle(suid sensor_uid)
{
  for(auto it = _clientid_conn_db.begin(); it != _clientid_conn_db.end(); it++)
  {
    auto &suid_conn_map = it->second;
    if(suid_conn_map.count(sensor_uid) != 0 && suid_conn_map.at(sensor_uid) == this)
    {
      return it->first;
    }
  }
  return -1;
}

int glinkSession::send_glink_req(suid sensor_uid, std::string encoded_buffer, bool is_resp_expected)
{
  int ret_val = 0;
  bool is_disable_req = false;

  if(_reconnecting)
  {
    sns_loge("glinkSession: conn = 0x%llx is reconnecting, cannot send request", (uint64_t)this);
    return -1;
  }
  sns_logd("glinkSession: conn = 0x%llx is sending request", (uint64_t)this);
  if((encoded_buffer.size()) > SNS_GLINK_MAX_REQUEST_SIZE)
  {
    sns_loge("glinkSession: conn = 0x%llx request payload size is too large", (uint64_t)this);
    throw runtime_error("glinkSession request payload size too large");
  }

  /* Convert const uint8_t* to uint8_t* */
  const uint8_t* const_arr = reinterpret_cast<const uint8_t*>(encoded_buffer.c_str());
  uint8_t* arr = const_cast<uint8_t*>(const_arr);
  uint64_t start_ts_nsec = android::elapsedRealtimeNano();
  ret_val = glink_write(_chnl_fd, arr, encoded_buffer.length());
  if(ret_val < 0)
  {
    sns_loge("glinkSession: conn = 0x%llx glink_write has failed", (uint64_t)this);
    if(is_resp_expected)
    {
      sns_logd("glinkSession: Acquiring _resp_queue_mutex");
      _resp_queue_mutex.lock();
      _resp_queue.pop_back();
      _resp_queue_mutex.unlock();
      glink_error_handler(ret_val);
    }
    return -1;
  }

  uint64_t stop_ts_nsec = android::elapsedRealtimeNano();
  if((stop_ts_nsec - start_ts_nsec) > 2*NSEC_PER_SEC)
  {
    sns_logi("glinkSession: conn = 0x%llx time taken to complete glink_write is %lf secs",
              (uint64_t)this, (double)((stop_ts_nsec - start_ts_nsec)/(NSEC_PER_SEC)));
  }
  return 0;
}

int glinkSession::sendRequest(suid sensor_uid, std::string encoded_buffer)
{
  sns_logd("glinkSession: conn = 0x%llx is sending request", (uint64_t)this);
  sns_client_request_msg pb_req_msg_decoded;
  sns_client_request_msg pb_req_msg;
  sns_client_glink_msg pb_glink_msg;
  string encoded_glink_buffer;
  stream_req_info req_info;
  bool is_disable_req;
  uint8_t* buf;

  uint32_t conn_hndl = find_assigned_conn_handle(sensor_uid);
  if(conn_hndl == -1)
  {
    return -1;
  }

  pb_req_msg_decoded.ParseFromString(encoded_buffer);

  sns_logd("glinkSession: conn = 0x%llx, session_id = %d, low.suid %llx, msgid = %d ",
            (uint64_t)this, pb_req_msg_decoded.mutable_susp_config()->delivery_type(),
                      pb_req_msg_decoded.mutable_suid()->suid_low(), pb_req_msg_decoded.msg_id());
  is_disable_req = (SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ == pb_req_msg_decoded.msg_id()) ? true : false;
  req_info.sensor_uid.low = sensor_uid.low;
  req_info.sensor_uid.high = sensor_uid.high;
  req_info.is_disable_req = is_disable_req;
  req_info.conn_hndl = conn_hndl;

  sns_logd("glinkSession: Acquiring _resp_queue_mutex");
  _resp_queue_mutex.lock();
  _resp_queue.push_back(req_info);
  _resp_queue_mutex.unlock();

  pb_glink_msg.mutable_req()->set_connection_handle(conn_hndl);
  pb_glink_msg.mutable_req()->mutable_request()->CopyFrom(pb_req_msg_decoded);
  pb_glink_msg.SerializeToString(&encoded_glink_buffer);
  return send_glink_req(sensor_uid, encoded_glink_buffer, true);
}

