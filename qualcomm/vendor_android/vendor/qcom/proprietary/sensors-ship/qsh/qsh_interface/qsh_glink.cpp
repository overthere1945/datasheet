/*
 * Copyright (c) 2021-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifndef SNS_WEARABLES_TARGET_AON
#include "qsh_glink.h"
//#include "sensors_trace.h"

qsh_glink::qsh_glink(qsh_glink_config config)
{}

qsh_glink::~qsh_glink()
{}

int qsh_glink::register_cb(sensor_uid suid, qsh_resp_cb resp_cb, qsh_error_cb error_cb, qsh_event_cb event_cb)
{
  return 0;
}
void qsh_glink::send_request(sensor_uid suid, bool use_jumbo_request, std::string encoded_buffer)
{}

#else
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
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include "sensors_log.h"
#include "worker.h"
#include "suid_lookup.h"
#include "qsh_glink_error.h"
#include "qsh_glink.h"
#include "sns_client.pb.h"
#include "sns_glink_client_api_v01.h"
#include "sns_suid.pb.h"
#include "sensors_trace.h"
#include "sensors_ssr.h"

static const uint64_t NSEC_PER_SEC = 1000000000ull;
const uint32_t GLINK_ERRORS_HANDLED_ATTEMPTS = 10; //TODO
std::thread qsh_glink::_glink_read_threadhdl;
bool qsh_glink::is_thread_created = false;
atomic<int> qsh_glink::_chnl_fd = -1;
int qsh_glink::_wakeup_pipe[2];
sns_wakelock *qsh_glink::_wakelock_inst = NULL;
std::mutex qsh_glink::_glink_read_mutex;
std::string qsh_glink::_chnl_name = "";
std::mutex qsh_glink::_clientid_conn_db_mutex;
std::map<uint8_t, qsh_glink*> qsh_glink::_clientid_conn_db;
uint32_t qsh_glink::_glink_err_cnt = 0;
std::vector<uint8_t> qsh_glink::_free_client_req_id_db;
std::vector<uint8_t> qsh_glink::_reserved_client_req_id_db;

qsh_glink::qsh_glink(qsh_glink_config config) :
    _is_ftrace_enabled(false),
    _reconnecting(false),
    _connection_closed(false)
{
  sns_logd("qsh_glink_int conn = 0x%llx constructor", (uint64_t)this);
  create_ind_memory();
  glink_connect(config.trasnport_name);
  if(_wakelock_inst == nullptr)
    _wakelock_inst = sns_wakelock::get_instance();
  _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_INIT;
}

qsh_glink::~qsh_glink()
{
  sns_logi("qsh_glink_int conn = 0x%llx destructor", (uint64_t)this);
  _connection_closed = true;
  update_clientid_conn_db();
  glink_disconnect(true);
  destroy_ind_memory();
  destroy_map_table();
  _disable_req_vector.clear();
  _resp.clear();
}

void qsh_glink::update_clientid_conn_db()
{
  sns_logd("qsh_glink_int conn = 0x%llx %s", (uint64_t)this, __func__);
  _clientid_conn_db_mutex.lock();
  for(auto it = _clientid_conn_db.begin(); it != _clientid_conn_db.end();)
  {
    if(it->second == this)
    {
      auto entry = it;
      it++;
      _clientid_conn_db.erase(entry);
    }
    else
      it++;
  }
  _clientid_conn_db_mutex.unlock();
  sns_logd("Exiting %s", __func__);
}

void qsh_glink::destroy_map_table()
{
  for(auto itr = _suid_map_table.begin(); itr != _suid_map_table.end();)
  {
    qsh_register_cb* current_cb = itr->first;
    qsh_register_cb* temp = current_cb;
    itr++;
    _suid_map_table.erase(current_cb);
    delete temp;
    temp = nullptr;
  }
}

void qsh_glink::initialize_client_req_id_db()
{
    for(int count = 1; count <= MAX_NUM_GLINK_CLIENTS; count++)
    {
        _free_client_req_id_db.push_back(count);
    }
    sns_logi("qsh_glink client reqid db initialized size %d", _free_client_req_id_db.size());
}

uint8_t qsh_glink::allocate_client_req_id()
{
    uint8_t client_req_id;
    _resp_mutex.lock();
    sns_logi("qsh_glink conn = 0x%llx client allocate_client_req_id size %d",
      (uint64_t)this, _free_client_req_id_db.size());
    if(0 == _free_client_req_id_db.size())
    {
        _resp_mutex.unlock();
        throw runtime_error("qsh_glink exceeded maximum number of glink clients");
    }

    client_req_id = _free_client_req_id_db[0];
    _free_client_req_id_db.erase(_free_client_req_id_db.begin());
    _reserved_client_req_id_db.push_back(client_req_id);
    _resp_mutex.unlock();
    sns_logi("qsh_glink conn = 0x%llx allocated _client_req_id = %d",
      (uint64_t)this, client_req_id);
    return client_req_id;
}

void qsh_glink::free_client_req_id(uint8_t client_req_id)
{
    sns_logd("qsh_glink conn = 0x%llx releasing _client_req_id = %d, waiting for _s_mutex",
      (uint64_t)this, client_req_id);
    _resp_mutex.lock();
    _reserved_client_req_id_db.erase(std::remove(_reserved_client_req_id_db.begin(),
      _reserved_client_req_id_db.end(), client_req_id));
    _free_client_req_id_db.push_back(client_req_id);
    _resp_mutex.unlock();
}

void qsh_glink::create_ind_memory()
{
  _ind = (sns_aon_client_report_ind_msg_v01 *)calloc(1, sizeof(sns_aon_client_report_ind_msg_v01));
  if(nullptr == _ind)
  {
    sns_loge("qsh_glink_int error while creating memory for _ind");
    return;
  }
  _ind_jumbo = (sns_aon_client_jumbo_report_ind_msg_v01 *)calloc(1, sizeof(sns_aon_client_jumbo_report_ind_msg_v01));
  if(nullptr == _ind_jumbo)
  {
    sns_loge("qsh_glink_int error while creating memory for ind_jumbo");
    return;
  }
}

void qsh_glink::destroy_ind_memory()
{
  if(nullptr != _ind)
  {
    free(_ind);
    _ind = nullptr;
  }

  if(nullptr != _ind_jumbo)
  {
    free(_ind_jumbo);
    _ind_jumbo = nullptr;
  }
}

int qsh_glink::glink_open(std::string chnl_name)
{
  int fd = 0;
  int retry_count = 0;
  std::string dev_node = GLINK_CHNL_PREFIX + chnl_name;

  do
  {
    sns_logd("qsh_glink_int about to open glink channel on dev node = %s", dev_node.c_str());
    fd = ::open(dev_node.c_str(), O_RDWR|O_NONBLOCK);
    if(fd > 0)
    {
      sns_logd("qsh_glink_int glink open successful, fd = %d", fd);
      break;
    }
    else
    {
      if(errno == ETIMEDOUT)
      {
        retry_count++;
        fd = -ETIMEDOUT;
        sns_loge("qsh_glink_int glink channel open timeout on dev node = %s, retries count =  %d", dev_node.c_str(), retry_count);
        sleep(1);
      }
      else
      {
        sns_loge("qsh_glink_int open on dev node = %s failed with errno = %d", dev_node.c_str(), errno);
        fd = -errno;
        break;
      }
    }
  }while (retry_count != MAX_GLINK_OPEN_RETRIES);

  return fd;
}

void qsh_glink::glink_connect(std::string chnl_name)
{
  sns_logd("qsh_glink_int glink_connect waiting for _s_glink_read_mutex");
  _glink_read_mutex.lock();

  if(_chnl_fd > 0)
  {
    sns_logi("qsh_glink_int channel is already opened");
    _glink_read_mutex.unlock();
    return;
  }

  if( (_chnl_fd = glink_open(chnl_name)) < 0 )
  {
    sns_loge("qsh_glink_int glink_open failed");
    _glink_read_mutex.unlock();
    throw glink_error(_chnl_fd, "qsh_glink_int open failed");
  }

  _chnl_name = chnl_name;

  if((_chnl_fd > 0) && (!is_thread_created))
  {
    /*Create PIPE to send custom message to glink poll thread during channel close */
    if(pipe(_wakeup_pipe) == -1)
    {
      _glink_read_mutex.unlock();
      throw glink_error(_wakeup_pipe[0], "qsh_glink pipe creation failed");
    }
    _glink_read_threadhdl = std::thread(glink_thread_routine);
    is_thread_created = true;
  }
  initialize_client_req_id_db();
  _glink_read_mutex.unlock();
}

int qsh_glink::glink_read(int fd, uint8_t* buf, size_t buf_len)
{
  int ret_val = 0;
  int read_bytes = 0;
  int retry_count = 0;
  bool read_packet_header = false;
  glink_packet_header *packet_header = NULL;

  sns_logd("qsh_glink_int glink_read fd = %d buf_len = %zu", fd, buf_len);
  while(read_bytes < buf_len)
  {
    ret_val = ::read(fd, buf, buf_len);
    if(ret_val == -1)
    {
      if (EAGAIN == errno)
      {
        sns_loge("qsh_glink_int read EAGAIN has occured on fd = %d", fd);

        retry_count++;
        if(MAX_GLINK_READ_RETRIES == retry_count)
        {
          _glink_err_cnt++;
          sns_loge("qsh_glink_int read MAX_GLINK_READ_RETRIES = %d has finished & g_glink_err_cnt = %d",
                    MAX_GLINK_READ_RETRIES, _glink_err_cnt);
          return -EAGAIN;
        }
        continue;
      }
      else
      {
        _glink_err_cnt++;

        sns_loge("qsh_glink_int read failed with errno = %d", errno);
        return -errno;
      }
    }

    sns_logd("qsh_glink_int partial read bytes = %d", ret_val);

    read_bytes += ret_val;
    buf += ret_val;

    if(false == read_packet_header)
    {
      if(read_bytes > sizeof(glink_packet_header))
      {
        packet_header = (glink_packet_header *)(buf - read_bytes);
        buf_len = packet_header->packet_len;
        sns_logd("qsh_glink_int pkt size to be read = %d", buf_len);
        read_packet_header = true;
      }
    }
  }

  sns_logd("qsh_glink_int complete read bytes = %d", read_bytes);
  return read_bytes;
}

void qsh_glink::glink_thread_routine()
{
  struct pollfd poll_fd[2];
  sns_glink_packet glink_packet;
  int msg_id  = 0;
  ssize_t ret_val = 0;
  uint8_t ch;
  bool exit = false;

  while(!exit)
  {
    poll_fd[0].fd     = _wakeup_pipe[0];
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd     = _chnl_fd;
    poll_fd[1].events = POLLIN | POLLHUP;

    sns_logd("qsh_glink waiting on poll wakeup_pipe %d glink_fd %d", poll_fd[0].fd, poll_fd[1].fd);
    ret_val = poll(poll_fd, 2, POLL_TIMEOUT_IN_MS);
    if(ret_val > 0)
    {
      if(poll_fd[0].revents & POLLIN)
      {
        ::read(poll_fd[0].fd, &ch, 1);
        if(ch == 'd')
        {
            sns_logd("qsh_glink explicitly requested thread close");
            exit = true;
        }
      }
      else if(poll_fd[1].revents & POLLIN)
      {
        sns_logd("qsh_glink_int received POLLIN event");
        ret_val = glink_read(poll_fd[1].fd, (uint8_t *)&glink_packet, sizeof(glink_packet));

        if(ret_val > 0)
        {
          msg_id = glink_packet.packet_hdr.msg_id;
          sns_logd("qsh_glink_int waiting for _s_glink_read_mutex");
          _glink_read_mutex.lock();
          qsh_glink* conn = nullptr;
          switch(msg_id)
          {
            case SNS_AON_CLIENT_RESP_V01:
            {
              sns_logd("qsh_glink_int received SNS_AON_CLIENT_RESP_V01 ");
              sns_aon_client_resp_msg_v01 *resp_msg_v01 =
                  (sns_aon_client_resp_msg_v01 *)&(glink_packet.sns_aon_client_msg);
              _clientid_conn_db_mutex.lock();
              auto itr = _clientid_conn_db.find(resp_msg_v01->client_request_id);
              if(itr != _clientid_conn_db.end() &&
                 itr->second->_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
              {
                conn = itr->second;
                _clientid_conn_db_mutex.unlock();
                conn->glink_response_handler(resp_msg_v01);
              }
              else
              {
                _clientid_conn_db_mutex.unlock();
                sns_loge("qsh_glink_int _client_id %d not found in lookup table or conn not active yet", resp_msg_v01->client_request_id);
              }
              break;
            }
            case SNS_AON_CLIENT_REPORT_IND_V01:
            {
              sns_logd("qsh_glink_int received SNS_AON_CLIENT_REPORT_IND_V01");
              sns_aon_client_report_ind_msg_v01 *ind_msg_v01 =
                        (sns_aon_client_report_ind_msg_v01 *)&(glink_packet.sns_aon_client_msg);
              _clientid_conn_db_mutex.lock();
              auto itr = _clientid_conn_db.find(ind_msg_v01->client_request_id);
              if(itr != _clientid_conn_db.end() &&
                 itr->second->_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
              {
                conn = itr->second;
                _clientid_conn_db_mutex.unlock();
                conn->glink_indication_handler(msg_id, glink_packet);
              }
              else
              {
                _clientid_conn_db_mutex.unlock();
                sns_loge("qsh_glink_int _client_id %d not found in lookup table or conn not active yet", ind_msg_v01->client_request_id);
              }
              break;
            }
            case SNS_AON_CLIENT_JUMBO_REPORT_IND_V01:
            {
              sns_logd("qsh_glink_int received SNS_AON_CLIENT_JUMBO_REPORT_IND_V01");
              sns_aon_client_jumbo_report_ind_msg_v01 *jumbo_report_ind_msg_v01 =
                 (sns_aon_client_jumbo_report_ind_msg_v01 *)&(glink_packet.sns_aon_client_msg);
              _clientid_conn_db_mutex.lock();
              auto itr  = _clientid_conn_db.find(jumbo_report_ind_msg_v01->client_request_id);
              if(itr != _clientid_conn_db.end() &&
                 itr->second->_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
              {
                conn = itr->second;
                _clientid_conn_db_mutex.unlock();
                conn->glink_indication_handler(msg_id, glink_packet);
              }
              else
              {
                _clientid_conn_db_mutex.unlock();
                sns_loge("qsh_glink_int _client_id %d not found in lookup table or conn not active yet", jumbo_report_ind_msg_v01->client_request_id);
              }
              break;
            }
            case SNS_AON_CLIENT_INFO_RESP_V01:
            {
              sns_logd("qsh_glink_int SNS_AON_CLIENT_INFO_RESP_V01 is received");
              break;
            }
            default:
            {
              sns_loge("qsh_glink_int invalid msg id");
            }
          }
          _glink_read_mutex.unlock();
        }
        else
        {
          sns_loge("qsh_glink_int read has failed with errno = %d, g_glink_err_cnt = %d", errno, _glink_err_cnt);
        }
      }
      else if(poll_fd[1].revents & POLLHUP)
      {
        sns_loge("qsh_glink_int POLLHUP event is received, AON CLIENT SSR occured");
        glink_error_handler(POLLHUP);
      }
      else
      {
        sns_loge("qsh_glink_int received unregistered pollevent = %d", (int)poll_fd[1].revents);
      }
    }
    else if(ret_val == 0)
    {
      sns_loge("qsh_glink_int poll time-out");
    }
    else
    {
      sns_loge("qsh_glink_int poll failed with errno = %d, g_glink_err_cnt = %d",
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
  sns_logd("qsh_glink thread exit");
}

void qsh_glink::glink_response_handler(sns_aon_client_resp_msg_v01* resp_msg)
{
  sns_logd("qsh_glink_int %s start", __func__);
  if (_connection_closed)
  {
    sns_logi("qsh_glink_int response is coming while connection is being closed");
    return;
  }

  sensor_uid suid;
  _resp_mutex.lock();
  if(false == _resp.empty())
  {
    suid = _resp.front();
    _resp_mutex.unlock();
    qsh_register_cb* register_cb_ptr = get_register_cb(suid);
    if(nullptr == register_cb_ptr)
    {
      sns_loge("qsh_glink_int Registered callback returned null");
      return;
    }
    qsh_resp_cb current_resp_cb = register_cb_ptr->get_resp_cb();
    if(current_resp_cb && resp_msg->result_valid && resp_msg->client_id_valid)
    {
      sns_logd("qsh_glink_int sending response to the client");
      current_resp_cb(resp_msg->result, resp_msg->client_id);
    }
    _resp_mutex.lock();
    _resp.erase(_resp.begin());
  }
  _resp_mutex.unlock();
  update_suid_map_table(suid);
}

void qsh_glink::update_suid_map_table(sensor_uid required_suid)
{
  _disable_req_mutex.lock();
  unsigned int index = 0;

  for(index = 0; index < _disable_req_vector.size(); index++)
  {
    sensor_uid current_suid = _disable_req_vector.at(index);
    if((required_suid.low == current_suid.low) &&
       (required_suid.high == current_suid.high))
    {
      qsh_register_cb* current_cb = get_register_cb(current_suid);
      qsh_register_cb* temp = current_cb;
      _suid_map_table.erase(current_cb);
      uint8_t clientreq_id = get_clientreq_id(current_suid);
      _clientid_db.erase(clientreq_id);
      _clientid_conn_db_mutex.lock();
      _clientid_conn_db.erase(clientreq_id);
      _clientid_conn_db_mutex.unlock();
      free_client_req_id(clientreq_id);
      delete temp;
      temp = nullptr;
      sns_logd("qsh_glink_int registered callback successfully deleted for clientreq_id %d",
          clientreq_id);
      break;
    }
  }

  if(index != _disable_req_vector.size())
  {
    _disable_req_vector.erase(_disable_req_vector.begin() + index);
  }
  _disable_req_mutex.unlock();
}

void qsh_glink::glink_error_handler(int error)
{
  sns_logi("qsh_glink_int glink_error_handler received error = %d", error);
  if(error == POLLHUP)
  {
    sns_logd("qsh_glink_int Inside POLLHUP error handler");
    _clientid_conn_db_mutex.lock();
    for(auto& it : _clientid_conn_db)
    {
      it.second->_reconnecting = true;
    }
    _clientid_conn_db_mutex.unlock();
    glink_disconnect(false);

    try
    {
      sns_logd("qsh_glink_int glink error, trying to reconnect");
      glink_connect(_chnl_name);
    }
    catch (const exception& e)
    {
      sns_loge("qsh_glink_int could not reconnect: %s", e.what());
      return;
    }

    _clientid_conn_db_mutex.lock();
    for(auto& it : _clientid_conn_db)
    {
      qsh_glink* conn = it.second;
      conn->_worker.add_task([conn]
      {
        conn->_reconnecting = false;
        if (conn->_connection_closed == true)
        {
          sns_logd("qsh_glink_int conn = 0x%llx sensor deactivated during ssr", (uint64_t)conn);
          return;
        }

        for(auto itr = conn->_suid_map_table.begin(); itr != conn->_suid_map_table.end(); ++itr)
        {
          qsh_register_cb* current_cb = itr->first;
          qsh_error_cb current_error_cb = current_cb->get_error_cb();
          if(current_error_cb)
          {
            current_error_cb(QSH_INTERFACE_RESET);
          }
        }
      });
    }
    _clientid_conn_db_mutex.unlock();
  }
  else
  {
    sns_logv("qsh_glink_int waitig for _s_glink_read_mutex");
    _glink_read_mutex.lock();
    _glink_err_cnt++;
    if((_glink_err_cnt > GLINK_ERRORS_HANDLED_ATTEMPTS) && !trigger_ssr())
    {
      sns_logd("qsh_glink_int triggred ssr g_glink_err_cnt = %d", _glink_err_cnt);
      _glink_err_cnt = 0;
    }
    _glink_read_mutex.unlock();
  }
}

void qsh_glink::glink_disconnect(bool thread_exit)
{
  if(_clientid_conn_db.size() > 0 && thread_exit)
  {
    sns_logi("Not the last conn. Do not close the chnl");
    return;
  }

  if(thread_exit)
  {
    if(::write(_wakeup_pipe[1], "d", 1) < 0)
    {
      sns_loge("qsh_glink write err on wakup pipe rrrno = %d no way to join readthread", errno);
    }
    _glink_read_threadhdl.join();
    is_thread_created = false;
    _free_client_req_id_db.clear();
    _reserved_client_req_id_db.clear();
    ::close(_wakeup_pipe[0]);
    ::close(_wakeup_pipe[1]);
  }
  if(::close(_chnl_fd) != 0)
  {
    sns_loge("qsh_glink_int close on _s_chnl_fd = %d failed with errno = %d", _chnl_fd.load(), errno);
  }
   _chnl_fd = -1;
}

int qsh_glink::update_suid(unsigned int msg_id, sensor_uid &out_suid)
{
  sns_client_event_msg pb_event_msg;
  if(SNS_AON_CLIENT_REPORT_IND_V01 == msg_id )
  {
    sns_logv("qsh_glink_int Parsing from msg_id SNS_AON_CLIENT_REPORT_IND_V01");
    pb_event_msg.ParseFromArray(_ind->payload, _ind->payload_len);
  }
  else if(SNS_AON_CLIENT_JUMBO_REPORT_IND_V01 == msg_id)
  {
    sns_logv("qsh_glink_int Parsing from msg_id SNS_AON_CLIENT_JUMBO_REPORT_IND_V01");
    pb_event_msg.ParseFromArray(_ind_jumbo->payload, _ind_jumbo->payload_len);
  }
  else
  {
    sns_loge("qsh_glink_int wrong msg id");
    return -1;
  }

  if( false == pb_event_msg.has_suid())
  {
    sns_logi("qsh_glink_int Message does not have suid");
    return -1;
  }

  sns_logv("qsh_glink_int Done parsing message");
  sns_std_suid suid_msg = pb_event_msg.suid();
  out_suid.low = suid_msg.suid_low();
  out_suid.high = suid_msg.suid_high();

  return 0;
}

qsh_register_cb* qsh_glink::get_register_cb(sensor_uid required_suid)
{
  for(auto itr = _suid_map_table.begin(); itr != _suid_map_table.end(); ++itr)
  {
    qsh_register_cb* current_cb = itr->first;
    sensor_uid current_suid;
    current_suid.low = (itr->second).low;
    current_suid.high = (itr->second).high;
    if(required_suid.low == current_suid.low &&
       required_suid.high == current_suid.high)
    {
      return current_cb;
    }
  }

  return nullptr;
}
uint8_t qsh_glink::get_clientreq_id(sensor_uid required_suid)
{
  for(auto itr = _clientid_db.begin(); itr != _clientid_db.end(); ++itr)
  {
    uint8_t client_id = itr->first;
    sensor_uid current_suid;
    current_suid.low = (itr->second).low;
                current_suid.high = (itr->second).high;
    if(required_suid.low == current_suid.low &&
       required_suid.high == current_suid.high)
    {
      return client_id;
    }
  }
  return 0;
}


int qsh_glink::trigger_event_cb(unsigned int idl_msg_id, qsh_event_cb current_event_cb, uint64_t ts)
{
  if(SNS_AON_CLIENT_REPORT_IND_V01 == idl_msg_id )
  {
    current_event_cb(_ind->payload, _ind->payload_len, ts);
    memset(_ind->payload, 0 , _ind->payload_len);
    return 0;
  }
  else if (SNS_AON_CLIENT_JUMBO_REPORT_IND_V01 == idl_msg_id)
  {
    current_event_cb( _ind_jumbo->payload, _ind_jumbo->payload_len, ts);
    memset(_ind_jumbo->payload, 0 , _ind_jumbo->payload_len);
    return 0;
  }
  else
  {
    sns_loge("qsh_glink_int Not a valid msg_id");
    return -1;
  }
}

void qsh_glink::glink_indication_handler(unsigned int msg_id, sns_glink_packet glink_packet)
{
  sns_logd("qsh_glink_int conn = 0x%llx received indication", (uint64_t)this);

  if(_connection_closed)
  {
    sns_logd("qsh_glink_int conn = 0x%llx indication is coming while connection is being closed", (uint64_t)this);
    return;
  }

  uint64_t sample_received_ts = android::elapsedRealtimeNano();
  sns_client_event_msg pb_event_msg;

  sns_logv("qsh_glink_int msg_id %d " , msg_id);
  if(msg_id == SNS_AON_CLIENT_REPORT_IND_V01)
  {
    sns_aon_client_report_ind_msg_v01 *ind_msg_v01 =
        (sns_aon_client_report_ind_msg_v01 *)&(glink_packet.sns_aon_client_msg);

    _ind->client_id = ind_msg_v01->client_id;
    _ind->client_request_id = ind_msg_v01->client_request_id;
    _ind->payload_len = ind_msg_v01->payload_len;
    memcpy(_ind->payload, ind_msg_v01->payload, ind_msg_v01->payload_len);
  }
  else if(msg_id == SNS_AON_CLIENT_JUMBO_REPORT_IND_V01)
  {
    sns_aon_client_jumbo_report_ind_msg_v01 *jumbo_report_ind_msg_v01 =
        (sns_aon_client_jumbo_report_ind_msg_v01 *)&(glink_packet.sns_aon_client_msg);

    _ind_jumbo->client_id = jumbo_report_ind_msg_v01->client_id;
    _ind_jumbo->client_request_id = jumbo_report_ind_msg_v01->client_request_id;
    _ind_jumbo->payload_len = jumbo_report_ind_msg_v01->payload_len;
    memcpy(_ind_jumbo->payload, jumbo_report_ind_msg_v01->payload, jumbo_report_ind_msg_v01->payload_len);
  }
  else
  {
    sns_loge("qsh_glink_int not a valid report indication msg_id");
    return;
  }

  sensor_uid suid;
  int ret = update_suid(msg_id, suid);
  if(ret < 0)
  {
    sns_loge("qsh_glink_int first level decoding suid failed");
    return;
  }

  qsh_register_cb* register_cb_ptr = get_register_cb(suid);
  if(nullptr == register_cb_ptr)
  {
    sns_loge("Registered callback is null");
    return;
  }

  qsh_event_cb current_event_cb = register_cb_ptr->get_event_cb();
  ret = trigger_event_cb(msg_id, current_event_cb, sample_received_ts);
  sns_logv("qsh_glink_int triggered event callback, got value %d", ret);
  return;
}

bool qsh_glink::is_suid_already_registered(sensor_uid required_suid)
{
  for(auto itr = _suid_map_table.begin(); itr!= _suid_map_table.end(); ++itr)
  {
    sensor_uid current_suid;
    current_suid.low = (itr->second).low;
    current_suid.high = (itr->second).high;
    if(required_suid.low == current_suid.low &&
       required_suid.high == current_suid.high)
    {
      return true;
    }
  }

  return false;
}

int qsh_glink::register_cb(sensor_uid suid, qsh_resp_cb resp_cb, qsh_error_cb error_cb, qsh_event_cb event_cb)
{
  sns_logv("qsh_glink_int %s start " , __func__);
  uint8_t clientreq_id = 0;
  qsh_register_cb *cb_ptr = new qsh_register_cb(resp_cb, error_cb, event_cb);
  if(nullptr == cb_ptr)
  {
    sns_loge("qsh_glink_int Could not create qsh_register_cb");
    return -1;
  }

  bool is_registered = is_suid_already_registered(suid);
  if(false == is_registered)
  {
    _suid_map_table.insert(std::pair<qsh_register_cb *, sensor_uid>(cb_ptr, suid));
    clientreq_id = allocate_client_req_id();
    sns_logd("clientreq_id = %d", clientreq_id);
    _clientid_db.insert(std::pair<uint8_t, sensor_uid>(clientreq_id, suid));
    _clientid_conn_db_mutex.lock();
    _clientid_conn_db.insert(std::pair<uint8_t, qsh_glink*>(clientreq_id, this));
    _clientid_conn_db_mutex.unlock();
  }
  else
  {
    sns_loge("qsh_glink_int This suid is already registered ");
    return -1;
  }

  sns_logv("qsh_glink_int %s completed " , __func__);
  return 0;
}

int qsh_glink::glink_write(int fd, uint8_t* buf, size_t buf_len)
{
  ssize_t ret_val = 0;
  int write_bytes = 0;
  int retry_count = 0;

  sns_logd("qsh_glink_int conn = 0x%llx write fd = %d buf_len = %zu, waiting for _s_glink_read_mutex",
                       (uint64_t)this, fd, buf_len);
  _glink_read_mutex.lock();
  if(_chnl_fd == -1)
  {
    sns_loge("qsh_glink_int conn = 0x%llx write is not permitted as_s_chnl_fd is -1", (uint64_t)this);
    _glink_read_mutex.unlock();
    return 0;
  }

  SNS_TRACE_BEGIN("sensors::glink_write");
  if(_wakelock_inst != nullptr)
  {
    sns_logd("acquiring wakelock");
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
        sns_loge("qsh_glink_int conn = 0x%llx write EAGAIN has occured", (uint64_t)this);
        if(MAX_GLINK_WRITE_RETRIES == retry_count )
        {
          _glink_err_cnt++;
          sns_loge("qsh_glink_int conn = 0x%llx write MAX_GLINK_WRITE_RETRIES = %d has finished on fd = %d & g_glink_err_cnt = %d",
                    (uint64_t)this, MAX_GLINK_WRITE_RETRIES, fd, _glink_err_cnt);
          _glink_read_mutex.unlock();
          if(_wakelock_inst != nullptr)
          {
            sns_logd("qsh_glink_int releasing wakelock");
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
        sns_loge("qsh_glink_int conn = 0x%llx write has failed with errno = %d & g_glink_err_cnt = %d",
                        (uint64_t)this, errno, _glink_err_cnt);
        _glink_read_mutex.unlock();
        if(_wakelock_inst != nullptr)
        {
          sns_logd("qsh_glink_int releasing wakelock");
          _wakelock_inst->put_n_locks(1);
        }
        SNS_TRACE_END();

        if(_conn_status == qsh_conn_status::QSH_CONNECTION_STATUS_ACTIVE)
          _conn_status = qsh_conn_status::QSH_CONNECTION_STATUS_INIT;
        return -errno;
      }
    }
    sns_logd("qsh_glink_int conn = 0x%llx partial bytes written= %d", (uint64_t)this, ret_val);
    buf_len     -= ret_val;
    buf         += ret_val;
    write_bytes += ret_val;
  }
  _glink_read_mutex.unlock();
  sns_logi("qsh_glink_int conn = 0x%llx complete bytes written= %d retries %d", (uint64_t)this, write_bytes, retry_count);
  if(_wakelock_inst != nullptr)
  {
    sns_logd("qsh_glink_int releasing wakelock");
    _wakelock_inst->put_n_locks(1);
  }
  SNS_TRACE_END();
  return ret_val;
}

void qsh_glink::send_request(sensor_uid suid, bool use_jumbo_request, std::string encoded_buffer)
{
  sns_glink_packet glink_packet;
  sns_client_request_msg pb_req_msg_decoded;
  int ret_val = 0;
  int encoded_msg_len;
  int client_req_id = get_clientreq_id(suid);
  bool isDisablereq = false;

  if(client_req_id == 0)
  {
    sns_loge("qsh_glink clientid for suid.high %llx suid.low %llx  is not found in lookup table",
      suid.high,suid.low);
    throw glink_error(ret_val,"glink_write failed");
  }
  if (_reconnecting)
  {
    sns_loge("qsh_glink_int conn = 0x%llx is reconnecting, cannot send request", (uint64_t)this);
    return;
  }
  sns_logd("qsh_glink_int conn = 0x%llx is sending request", (uint64_t)this);
  if((encoded_msg_len = encoded_buffer.size()) > SNS_AON_CLIENT_REQ_LEN_MAX_V01)
  {
    sns_loge("qsh_glink_int conn = 0x%llx request payload size is too large", (uint64_t)this);
    throw runtime_error("qsh_glink request payload size too large");
  }
  pb_req_msg_decoded.ParseFromString(encoded_buffer);
  sns_logd("qsh_glink_int conn = 0x%llx session_id = %d client_req_id %d low.suid %llx msgid = %d ",
      (uint64_t)this, pb_req_msg_decoded.mutable_susp_config()->delivery_type(),
      client_req_id, suid.low, pb_req_msg_decoded.msg_id());
  isDisablereq = (SNS_CLIENT_MSGID_SNS_CLIENT_DISABLE_REQ == pb_req_msg_decoded.msg_id()) ? true : false;

  glink_packet.packet_hdr.msg_id                                =  SNS_AON_CLIENT_REQ_V01;
  glink_packet.packet_hdr.version                               =  1;
  glink_packet.sns_aon_client_msg.req_msg_v01.session_id        =  pb_req_msg_decoded.mutable_susp_config()->delivery_type();
  glink_packet.sns_aon_client_msg.req_msg_v01.client_request_id =  client_req_id;
  glink_packet.sns_aon_client_msg.req_msg_v01.use_jumbo_report  =  use_jumbo_request;
  glink_packet.sns_aon_client_msg.req_msg_v01.payload_len       =  encoded_buffer.length();
  memcpy(glink_packet.sns_aon_client_msg.req_msg_v01.payload,
         encoded_buffer.c_str(), encoded_buffer.length());

  if(isDisablereq) {
    _disable_req_mutex.lock();
    _disable_req_vector.push_back(suid);
    _disable_req_mutex.unlock();
  }
  _resp_mutex.lock();
  _resp.push_back(suid);
  _resp_mutex.unlock();

  uint64_t start_ts_nsec = android::elapsedRealtimeNano();
  ret_val = glink_write(_chnl_fd, (uint8_t *)&glink_packet,
                  sizeof(glink_packet_header) + sizeof(sns_aon_client_req_msg_v01));
  if(ret_val < 0)
  {
    sns_loge("qsh_glink_int conn = 0x%llx glink_write has failed", (uint64_t)this);
    if (isDisablereq) {
      _disable_req_mutex.lock();
      _disable_req_vector.pop_back();
      _disable_req_mutex.unlock();
    }
    _resp_mutex.lock();
    _resp.pop_back();
    _resp_mutex.unlock();
    glink_error_handler(ret_val);
    throw glink_error(ret_val,"glink_write failed");
  }

  uint64_t stop_ts_nsec = android::elapsedRealtimeNano();
  if((stop_ts_nsec - start_ts_nsec) > 2*NSEC_PER_SEC)
  {
    sns_logi("qsh_glink_int conn = 0x%llx time taken to complete glink_write is %lf secs",
              (uint64_t)this, (double)((stop_ts_nsec - start_ts_nsec)/(NSEC_PER_SEC)));
  }
}

#endif
