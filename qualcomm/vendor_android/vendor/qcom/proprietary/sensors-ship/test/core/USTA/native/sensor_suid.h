/*============================================================================
  Copyright (c) 2017,2022-2023 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor_suid.h

  @brief
  SuidSensor class definition.
============================================================================*/
#pragma once

#include "ISession.h"
//#include "qsh_interface.h"
#include "sensor_common.h"
#include "sensor_message_parser.h"
/**
 * type alias for  event callback from SuidSensor
 * @param suid_low is the lower 64 bit of suid of returned sensor
 * @param suid_high is the higher 64bit of suid of returned sensor
 * @param is_last is boolen value signifying this suid is last suid
 *        for a required data_type
 */
typedef std::function<void( uint64_t& suid_low,
    uint64_t& suid_high, bool is_last)> handle_suid_event_cb;

using com::quic::sensinghub::session::V1_0::ISession;
using suid = com::quic::sensinghub::suid;

class SuidSensor
{
private:
  suid sensor_uid;
  handle_suid_event_cb      suid_event_cb;
  ISession*             _conn = nullptr;
  ISession::eventCallBack                  _suid_event_cb = nullptr;
  ISession::respCallBack                   _suid_resp_cb = nullptr;
  ISession::errorCallBack                  _suid_error_cb = nullptr;
  MessageParser* sensor_parser;
  // used to log suid event which has all the sensor list
  FILE*        fileStreamPtr;
  std::string       logfile_name;
  // receiving response from qsh_client
  void sensor_event_cb(const uint8_t *encoded_data_string,
      size_t encoded_string_size,
      uint64_t time_stamp);
public:

  /**
   * This creates a connection with QSH via class qsh_interface
   *
   * @param suid_event_cb is call back function of type handle_suid_event_cb,
   *        which will be called as many number of times as the number of suid
   *        returned for a particular data_type
   */
  SuidSensor(handle_suid_event_cb suid_event_cb, ISession* conn = NULL);
  /**
   * @brief This kills the connection with QSH
   */
  ~SuidSensor();
  /**
   * @brief sends a request to suid sensor for a suid list for a given datatype
   *
   *
   * @param data_type is the data type corresponding to which SUID's are
   *         required for complete sensor list, this should be '\0'
   */
  usta_rc send_request(std::string& data_type);
  /**
   * @brief Returns client processor list
   */
  std::vector<std::string> get_client_processor_list();
    /**
   * @brief Returns wakeup delivery list
   */
  std::vector<std::string> get_wakeup_delivery_list();
    /**
   * @brief Returns resampler type list
   */
  std::vector<std::string> get_resampler_type_list();
    /**
   * @brief Returns threshold type list
   */
  std::vector<std::string> get_threshold_type_list();


};
