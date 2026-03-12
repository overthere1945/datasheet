/*
 * Copyright (c) 2021-2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

#ifndef SNS_WEARABLES_TARGET_AON
#include "qsh_interface.h"

class qsh_glink : public qsh_interface
{
public:
  qsh_glink(qsh_glink_config config);
  ~qsh_glink();
  int register_cb(sensor_uid suid, qsh_resp_cb resp_cb, qsh_error_cb error_cb, qsh_event_cb event_cb);
  void send_request(sensor_uid suid, bool use_jumbo_request, std::string encoded_buffer);
};

#else
#include <stdexcept>
#include <cinttypes>
#include <stdint.h>
#include <string>
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
#include "qsh_register_cb.h"
#include "sensors_log.h"
#include "worker.h"
#include "suid_lookup.h"
#include "utils/SystemClock.h"
#include "sns_client.pb.h"
#include "sns_glink_client_api_v01.h"
#include "qsh_interface.h"

#define MAX_NUM_GLINK_CLIENTS   255
#define MAX_GLINK_OPEN_RETRIES  3
#define MAX_GLINK_WRITE_RETRIES 30
#define MAX_GLINK_READ_RETRIES  3
#define POLL_TIMEOUT_IN_MS     -1 // -1 : Infinite timeout
#define WAKEUP_SESSION_ID       0
#define NON_WAKEUP_SESSION_ID   1
#define MAX_NUM_SESSION_ID      2
#define GLINK_CHNL_PREFIX       "/dev/glinkpkt_slate_"

enum qsh_conn_status {
  QSH_CONNECTION_STATUS_INIT = 0,
  QSH_CONNECTION_STATUS_ACTIVE = 1,
};

class qsh_glink : public qsh_interface {
public:
  qsh_glink(qsh_glink_config config);
  ~qsh_glink();
  int register_cb(sensor_uid suid, qsh_resp_cb resp_cb, qsh_error_cb error_cb, qsh_event_cb event_cb);
  void send_request(sensor_uid suid, bool use_jumbo_request, std::string encoded_buffer);

private:
  static std::string _chnl_name;
  static atomic<int> _chnl_fd;
  static int  _wakeup_pipe[2];
  static std::mutex _glink_read_mutex;
  static bool is_thread_created;
  static std::thread _glink_read_threadhdl;
  static sns_wakelock *_wakelock_inst;
  static std::mutex _clientid_conn_db_mutex;
  std::vector<sensor_uid> _resp;
  std::mutex _resp_mutex;
  static uint32_t _glink_err_cnt;
  qsh_conn_status _conn_status;
  bool _is_ftrace_enabled;

  atomic<bool> _reconnecting;
  atomic<bool> _connection_closed;
  std::mutex _disable_req_mutex;
  std::vector<sensor_uid> _disable_req_vector;
  static std::vector<uint8_t> _free_client_req_id_db;
  static std::vector<uint8_t> _reserved_client_req_id_db;
  std::map<qsh_register_cb*, sensor_uid> _suid_map_table;
  std::map<uint8_t, sensor_uid> _clientid_db;
  static std::map<uint8_t, qsh_glink*> _clientid_conn_db;
  sns_aon_client_report_ind_msg_v01 *_ind = nullptr;
  sns_aon_client_jumbo_report_ind_msg_v01 *_ind_jumbo = nullptr;
  /*  worker task  */
  worker _worker;

  static void glink_connect(std::string chnl_name);
  static void glink_disconnect(bool thread_exit);
  static int glink_open(std::string chnl_name);
  static int glink_read(int fd, uint8_t* buf, size_t buf_len);
  static void glink_thread_routine();
  static void glink_error_handler(int error);

  /**
    * Handle a sns_aon_client_report_ind_msg_v01 or sns_aon_client_jumbo_report_ind_msg_v01
    * indication message received from SEE's Client Manager.
    *
    * @param[i] msg_id One of SNS_CLIENT_REPORT_IND_V01 or SNS_CLIENT_JUMBO_REPORT_IND_V01
    * @param[i] ind_buf The message buffer
    * @param[i] ind_buf_len Length of ind_buf
    */
  void glink_indication_handler( unsigned int msg_id, sns_glink_packet glink_packet);
  void glink_response_handler(sns_aon_client_resp_msg_v01* resp_msg);
  int  glink_write(int fd, uint8_t* buf, size_t buf_len);
  void glink_release_connection(uint8_t session_id);
  void create_ind_memory();
  void destroy_ind_memory();
  bool is_suid_already_registered(sensor_uid required_suid);
  int  trigger_event_cb(unsigned int idl_msg_id, qsh_event_cb current_event_cb, uint64_t ts);
  qsh_register_cb* get_register_cb(sensor_uid required_suid);
  int  update_suid(unsigned int msg_id, sensor_uid &out_suid);
  void destroy_map_table();
  void update_suid_map_table(sensor_uid required_suid);
  static void initialize_client_req_id_db();
  uint8_t allocate_client_req_id();
  void free_client_req_id(uint8_t client_req_id);
  uint8_t get_clientreq_id(sensor_uid required_suid);
  void update_clientid_conn_db();
};

#endif
