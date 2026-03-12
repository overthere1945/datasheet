/*============================================================================
  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc

  @file sensor_cxt.h
  @brief
  SensorCxt class definition.
============================================================================*/
#pragma once
#include "sensor_suid.h"
#include "sensor_common.h"
#include "sensor.h"
#include "sensor_descriptor_pool.h"
#include <list>
#include "sensor_direct_channel.h"
#include "SessionFactory.h"

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;
using suid = com::quic::sensinghub::suid;

typedef void (*display_samples_cb)(std::string display_samples, bool is_registry_sensor);

/**
 * @brief callback signature for the client to register, to get the sensor_event
 *
 * @param sensor_event: event data shared to client in the json view formate
 *        sensor_handle : sensor handle for the sample on this client_connect_id
 */
typedef void (*sensor_event_cb)(std::string sensor_event, unsigned int sensor_handle, unsigned int client_connect_id);


/**
 * @brief
 *
 * @param CREATE_CLIENT use this enum for registering callbacks using register_cb,
 *             This creates the new connection and start the current use case
 *
 * @param USE_CLIENT_CONNECT_ID - use this client ID to
 *        start new use case on the existing connection
 */
typedef enum client_request_type{
  CREATE_CLIENT = 0,
  USE_CLIENT_CONNECT_ID,
}client_request_type;

typedef struct register_cb {
  sensor_event_cb event_cb;
}register_cb;

typedef struct client_request {
  client_request_type req_type;
  union {
    register_cb cb_ptr;
    unsigned int client_connect_id;
  };
}client_request;

typedef enum sensor_channel_type {
   STRUCTURED_MUX_CHANNEL = 0,
   GENERIC_CHANNEL
}sensor_channel_type;

typedef enum sensor_client_type {
  SSC = 0,
  APSS,
  ADSP,
  MDSP,
  CDSP
}sensor_client_type;

typedef struct create_channel_info {
  unsigned long shared_memory_ptr;
  unsigned long shared_memory_size;
  sensor_channel_type channel_type_value;
  sensor_client_type client_type_value;
}create_channel_info;

typedef struct direct_channel_std_req_info
{
  unsigned int sensor_handle;
  std::string is_calibrated;
  std::string is_fixed_rate;
  std::string sensor_type;
  std::string batch_period;
#if 0
  std::string flush_period;
  std::string flush_only;
  std::string is_max_batch;
  std::string is_passive;
#endif
}direct_channel_std_req_info;

typedef struct direct_channel_delete_req_info
{
  unsigned int sensor_handle;
  std::string is_calibrated;
  std::string is_fixed_rate;
}direct_channel_delete_req_info;

typedef struct qsh_interface_info
{
  ISession *interface;
  sensors_diag_log *diag_log;
}qsh_interface_info;
/**
 * @brief singleton class to manage creation of sensor objects and handling
 *        the request from the clients
 *
 */
class SensorCxt {
public:
  ~SensorCxt();
  /**
   * @brief returns the SensorCxt instance. SensorCxt being singleton, all
   *        objects share common  instance
   */
  static SensorCxt* getInstance();
  /**
   * @brief gives the list of sensors supported by QSH in predefined format
   *
   * @param sensor_list is string reference provided by the client which
   *        would be populated with the sensor list in format as below:
   *        "<datatype sensor 0>-<vendor sensor 0>:
   *        <suid_low senor 0>:
   *        <suid_high sensor 0>,
   *
   *        <details of sensor 1>,
   *        <details of sensor 2>,
   *        ..."
   *        The order of the list becomes the handle which SensorCxt will
   *        understand to identify a sensor
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc get_sensor_list(std::vector<sensor_info>& sensor_list);
  /**
   * @brief gives the request messages in a predefined format for all the
   *         request messages supported by a sensor
   *
   * @param handle is the unique identifier to correspond to a sensor whose
   *        request messages are desired
   *
   * @param request_vector is an empty vector reference of string provided
   *        by the client. This empty vector will be populated with all the
   *        request messages supported by a sensor corresponding to provided
   *        handle. The request_vector format will be:
   *
   *        <msgName>:<msgID>{Name:<nameValue>;DataType:<datatypeValue>;
   *                           Label:<labelValue>;Default:<defaultValue>;
   *                            EnumValue:<enumValue1>,..;}
   *                          { <details of field 1> }
   *                          { <details of field 2> }
   *        " <details of next message>"
   *        " <details of next message>"
   *        ...
   *
   *        msgName       : Message Name as defined in proto file
   *        msgID         : Message Id corresponding to this message as defined
   *                        in proto file
   *        nameValue     : Name of field as present in the message
   *        datatypeValue : Data type of field : int32,uint32,uint64,double,float,
   *                        bool,enum,string,message
   *        labelValue    : label of the field : required/optional/repeated
   *        EnumValue<i>  : list of comma separate possible enum values, only if
   *                        datatypeValue is enum
   *
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc get_request_list(unsigned int handle,std::vector<msg_info> &request_msgs);
  /**
   * @brief gets the complete list of attributes of a particular sensor
   *
   * @param handle is the unique identifier to correspond to a sensor from whom
   *        the attribute is request
   *
   * @param attributes is string reference provided by the client which would
   *        be populated with the attributes in json format
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc get_attributes(unsigned int handle,std::string& attributes);
  /**
   * @brief removes a list of sensors from SensorCxt
   *
   * @param remove_sensor_listis a list of sensor handles to be removed
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc remove_sensors(std::vector<int>& remove_sensor_list);

  void enableStreamingStatus(unsigned int sensor_handle);
  void disableStreamingStatus(unsigned int sensor_handle);
  void update_display_samples_cb(display_samples_cb ptr_reg_callback);
  bool guard_sample_rate(send_req_msg_info& request_msg);

  /**
   * @brief direct_channel_open is used to create the channel
   *
   * @param create_channel_info is the set of parameters used for creating the channel
   *        create_channel_info consists of
   *
   * @param channel_handle is the out param for this method contains the channel number
   *        Returned from DSP

   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc direct_channel_open(create_channel_info create_info, unsigned long &channel_handle);

  /**
   * @brief direct_channel_close is used to remove the channel
   *
   * @param Channel_handle which supposed to remove
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc direct_channel_close(unsigned long channel_handle);

  /**
   * @brief direct_channel_update_offset_ts is used to update the
   *        offset time between Android and DSP on need basis
   *
   * @param Time stamp offset value in nsecs between Apps and DSP
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc direct_channel_update_offset_ts(unsigned long channel_handle, int64_t offset);

  /**
   * @brief direct_channel_send_request is used to send the
   *        stream request per sensor on a existing channel
   *
   * @param channel_handle is existing channel handle details
   *        on which stream supposed to start
   *
   * @param std_req_info is set of parameters to start stream request
   *
   * @param sensor_payload_field contains the sensor
   *        request messages specific fields.
   *        This data is normal values only.
   *        Not in the encoded form.
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc direct_channel_send_request(unsigned long channel_handle, direct_channel_std_req_info std_req_info, send_req_msg_info sensor_payload_field);

  /**
   * @brief direct_channel_delete_request is used to stop the
   *        stream per sensor on a existing channel
   *
   * @param delete_req_info consists of parameters to delete stream request
   *
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc direct_channel_delete_request(unsigned long channel_handle, direct_channel_delete_req_info delete_req_info);

  /**
   * @brief send_request is used to activate / configure / deactivate
   *        sensor on the given client_connect_id.
   *        this api returns the connection_connect_id,
   *        that can be used in subsequent requests to reconfigure or deactivate
   *
   * @param client_request: it is an input argument.
   *        for creating new Connection : Please use register_cb .
   *        this helps routing all the samples to in data_path to the register_cbs
   *
   *        for sending request on the existing connection: please use client_connect_id
   *        this will ensure send request will be handled on the given client_connect_id
   *
   *
   * @param sensor_handle is the unique identifier to correspond to a sensor for whom
   *        the request is intended
   *
   * @param send_req_msg_info: It is as per proto request mapped
   *        for that request msg ID for that particular sensor. please see structure in detail.
   *
   * @param client_req_msg_fileds: standard request fields that were defined in sns_client.proto.
   *        please see respective strcutre in detail
   *
   * @param log_file_name is the client supplied file name. The name of file
   *        will be:
   *        <datatype>-<vendor>-<log_file_name>.txt
   *        if logfile_name is empty, the file will be <datatype>-<vendor>.txt
   *
   * @return success : client_connect_id associated with request
   *         failure: Negative value indicating error code corresponding to failure type.
   */
  int send_request(client_request client_req,
        unsigned int sensor_handle,
        send_req_msg_info& request_msg,
        client_req_msg_fileds &std_fields_data,
        std::string& log_file_name);
   /**
   * @brief update_logging_flag is used to switch the
   *        logging level of logs from USTA.
   *
   * @param disable_flag consists a boolean value to enable or disable logs.
   */
  void update_logging_flag(bool disable_flag);
    /**
   * @brief Returns client processor list
   */
  inline std::vector<std::string> get_client_processor_list(){
    return _client_processor;
  }
    /**
   * @brief Returns wakeup delivery list
   */
  inline std::vector<std::string> get_wakeup_delivery_list(){
    return _wakeup_delivery_type;
  }
    /**
   * @brief Returns resampler type list
   */
  inline std::vector<std::string> get_resampler_type_list(){
    return _resampler_type;
  }
    /**
   * @brief Returns threshold type list
   */
  inline std::vector<std::string> get_threshold_type_list(){
    return _threshold_type;
  }

private:
  /* only one SensorsContext */
  static SensorCxt* self;
  static std::unique_ptr<sessionFactory> _session_factory_instance;
  SensorCxt();
  std::list<std::pair<Sensor*, int>> _sensors_hub_id_map_table;
  SuidSensor* suid_instance;
  std::condition_variable      cb_cond;
  bool                is_resp_arrived;
  std::mutex     cb_mutex;
  handle_suid_event_cb  suid_event_cb;
  handle_attribute_event_func attribute_event_cb;

  int  num_sensors;
  int  num_attr_received;
  int  _current_hub_id = -1;
  std::vector<int> _hub_ids;
  std::unordered_map<int, int> _hub_id_idx_map_table;

  void handle_suid_event(uint64_t& suid_low,uint64_t& suid_high,
      bool is_last_suid);
  void handle_attribute_event();
  void handle_attrib_error_event(ISession::error error_code);
  display_samples_cb ptr_display_samples_cb = nullptr;

  /*Private variables and methods for direct channel*/
  std::list<sensor_direct_channel *> direct_channel_instance_array;
  std::vector<unsigned long> direct_channel_handle_array;
  direct_channel_type get_channel_type(sensor_channel_type in);
  sns_std_client_processor get_client_type(sensor_client_type in);
  void get_suid_info(unsigned int handle,
      direct_channel_stream_info *stream_info_out);
  void get_payload_string(unsigned int handle,
      send_req_msg_info sensor_payload_field,
      std::string &encoded_data);
  int get_sensor_type(unsigned int handle, bool is_calibrated = false);
  sensor_direct_channel* get_dc_instance(unsigned long channel_handle);
  direct_channel_attributes_info attributes_info;

  /*Shared_conn_feature*/
  int send_request(ISession* interface_instance,
      sensors_diag_log* diag,
      Sensor* current_sensor,
      send_req_msg_info& request_msg,
      client_req_msg_fileds &std_fields_data,
      std::string& log_file_name);
  ISession* create_qsh_interface(int hub_id);
  void delete_qsh_interface_info(ISession *qsh_qmi);
  sensors_diag_log* get_diag_ptr(ISession *interface_instance);
  std::pair<Sensor*, int> get_sensor_data(unsigned int handle);
  void update_map_tables(int client_connect_id, bool is_disable_request);
  int get_sensor_handle(Sensor* required_sensor);
  void process_sensor_event(ISession* conn, std::string sensor_event, void* ptr);
  void process_sensor_error(ISession::error sensor_error);
  int get_key(ISession* conn);
  bool is_sensor_streaming(unsigned int sensor_handle);
  bool waitForResponse(std::mutex& _cb_mutex,std::condition_variable& _cb_cond, bool& cond_var);

  std::vector<qsh_interface_info> _qsh_interface_info_array;

  // Key: Client_connect_id, Value: interface ptr
  std::map<int, ISession *>        _interface_map_table;

  // Key: Client_connect_id, Value: diag ptr
  std::map<int, sensors_diag_log *>     _diag_map_table;

  // Key: Client_connect_id, Value: cb ptr
  std::map<int, register_cb>            _callback_map_table;

  // Key: Client_connect_id, Value: Vector of sensor_handle that are streaming
  std::map<int, std::vector<unsigned int>>   _client_connect_id_streaming_table;

  // Key: sensor_handle, Value: Client Connect ID
  std::map<int, int>                    _sensor_handle_map_table;
  worker*                               _cb_thread = nullptr;
  sensor_stream_event_cb                _sensor_event_cb = nullptr;
  sensor_stream_error_cb                _sensor_error_cb = nullptr;
  std::mutex                            _sync_mutex;
  std::mutex                            _map_mutex;

  sensors_diag_log*                     _diag_attrb;

  std::vector<std::string> _client_processor ;
  std::vector<std::string> _wakeup_delivery_type ;
  std::vector<std::string> _resampler_type;
  std::vector<std::string> _threshold_type;
};
