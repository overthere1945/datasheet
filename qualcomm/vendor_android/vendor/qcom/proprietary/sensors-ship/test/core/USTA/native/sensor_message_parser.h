
/*============================================================================
  Copyright (c) 2017, 2021-2022 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor_message_parser.h
  @brief
  sensor_message_parser class declaration.
============================================================================*/

#pragma once
#include"sensor_common.h"

typedef struct enum_info
{
  std::string name ;
  int32  value;
}enum_info;

typedef struct field_info field_info;

typedef struct nested_msg_info
{
  std::string msg_name ;
  std::vector<field_info> fields;
}nested_msg_info;

typedef struct field_info
{
  std::string data_type ;
  std::string name ;
  std::string label ;
  bool has_default;
  bool is_enum;
  std::string default_val ;
  std::vector<enum_info> enum_list;  //optional, only if is_enum is true;
  bool is_user_defined_msg;
  std::vector<nested_msg_info> nested_msgs;
  std::vector<std::string> value;
}field_info;
typedef struct msg_info
{
  std::string msg_name ;
  std::string msgid ;
  std::vector<field_info> fields;
}msg_info;


typedef struct send_req_field_info send_req_field_info;

typedef struct send_req_nested_msg_info
{
  std::string msg_name ;
  std::vector<send_req_field_info> fields;
}send_req_nested_msg_info;

typedef struct send_req_field_info
{
  std::string name ;
  std::vector<std::string> value;
  std::vector<send_req_nested_msg_info> nested_msgs;
}send_req_field_info;

typedef struct send_req_msg_info
{
  std::string msg_name ;
  std::string msgid ;
  std::vector<send_req_field_info> fields;
}send_req_msg_info;

typedef struct payload_info
{
  std::string name ;
  std::vector<std::string> value;
}payload_info;

/**
 * @brief This class is a helper class to a sensor object. It is used to
 *        1. parse the proto files for generating request messages
 *        2. formulating request msgs for a given message name or msg id
 *        3. parse encoded request/events in json format
 *
 */
class MessageParser
{

public:

  MessageParser();   // constructor
  ~MessageParser();

  /**
   * @brief parse a request message of type "sns_client_request_msg".
   *        After this function call the request msg is parsed in json format
   *        and stored in a private string request_str
   *
   * @param data is the pointer to start of encoded request stream
   *
   * @param size is the size of data of encoded request stream
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc parse_request_message(std::string messagename, const char* data, int data_size);
  /**
   * @brief parse an event message of type "sns_client_event_msg". After this
   *        function call the event msg is parsed in json format and stored
   *        in a private string event_str
   *
   * @param data is the pointer to start of encoded event stream
   *
   * @param size is the size of data of encoded event stream
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc parse_event_message(const char* data, int data_size,
      std::vector<payload_info> &sensor_payload_info);
  /**
   * @brief sets the list of proto files associated with a sensor which has
   *        created obj of this class
   *
   * @param api is the list of proto files separated by delimiter '?'
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc set_api_info(std::vector<std::string>& api);
  /**
   * @brief gives the request messages in a predefined format for all the
   *        request messages supported by a sensor
   *
   * @param request_vector is an empty vector reference of string provided by
   *        the client. This empty vector will be populated with all the
   *        request messages supported by a sensor corresponding to provided
   *        handle. The returned request_vector format will be:
   *
   *        “<msgName>:<msgID>{Name:<nameValue>;DataType:<datatypeValue>;
   *                           Label:<labelValue>;Default:<defaultValue>;
   *                            EnumValue:<enumValue1>,..;}
   *
   *                          { <details of field 1> }
   *                          { <details of field 2> }”
   *        " <details of next message>"
   *        " <details of next message>"
   *        ...
   *
   *        msgName       : Message Name as defined in proto file
   *        msgID         : Message Id corresponding to this message as defined
   *                        in proto file
   *        nameValue     : Name of field as present in the message
   *        datatypeValue : Data type of field : int32,uint32,uint64,double,
   *                        float,bool,enum,string,message
   *        labelValue    : label of the field : required/optional/repeated
   *        EnumValue<i>  : list of comma separate possible enum values, only
   *                        if datatypeValue is enum
   *
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc get_request_msg(std::vector<msg_info>& request_vector);
  /**
   * @brief formulates request message for a sensor specific to a message name
   *        using reflection
   *
   * @param request_token is a vector reference having each fields of message.
   *        Note all the fields,including optional fields, are must for
   *        formulating this message.
   *
   * @param message_name is the name message from proto files for the message
   *        it need to formulate
   *
   * @param request_string is a string reference provided by sensor which will
   *        be populated with the required encoded request message.
   *
   * @return success or failure. Based on defined error code in enum: usta_rc
   */
  usta_rc formulate_request_msg(send_req_msg_info& req_msg,std::string& request_string);
  /**
   * @brief returns the last request string : request_str which is in json
   *       format
   *
   * @return string reference to request_str
   */
  std::string& get_request_json_string();
  /**
   * @brief returns the last event string : event_str which is in json format
   *
   * @return string reference to event_str
   */
  std::string& get_event_json_string();
private:
  std::string request_str="";
  std::string event_str = "";
  std::string output_str = "";
  uint32 space;
  bool protos_found;
  int current_msg_id;
  // stores all the request msgs for this instance in vector
  std::vector<msg_info> request_msgs;

  // stores all the nested request msgs for internal purpose
  std::vector<msg_info> nested_msgs_pool;
  int nested_msg_index;
  // list of proto vectors for this sensor
  std::vector<std::string> protos;
  std::map<int,std::string> msg_look_up;

  usta_rc generate_msg_look_up();
  // assuming msg_look_up is already populated
  usta_rc generate_request_msg_list();
  // used by generate_request_msg_list
  usta_rc parse_request_msg_fields(std::vector<msg_info> &msg_vector_type,
      const std::string& message, int index);
  // parse given message
  usta_rc parse_message(std::string message_name,const char* data, int data_size);

  void get_msg_fields(msg_info &each_msg, msg_info &msg_string);
  void get_nested_msg_fields(nested_msg_info &out_nested_msg);

  // utilities for parsing message
  void OutputMessage(Message* message);
  void OutputField(Message* message, const Reflection* reflection,
      const FieldDescriptor* field);
  void OutputFieldValue(Message* message, const Reflection* reflection,
      const FieldDescriptor* field, int index , std::string &out_value);
  void OutputRepeatedField(Message* message,
      const Reflection* reflection, const FieldDescriptor* field,
      std::vector<std::string> &repeated_values);
  void OutputFieldName(const FieldDescriptor* field);
  void OutputMessageOpaquePayload(const std::string& message_name,
      const std::string& value);
  void OutputUnknownFields(const UnknownFieldSet& unknown_fields);

  // utilities for formulating request message from a request
  //string coming from application


  usta_rc InputMessage(Message* message,std::vector<send_req_field_info>& fields);
  usta_rc InputFieldValue(Message* message,
      const Reflection* reflection, const FieldDescriptor* field,
      std::vector<send_req_field_info>& user_defined_fields);
  int get_required_field_index(const FieldDescriptor* field,std::vector<send_req_field_info>& user_defined_fields);

  // parsing attribute
  //void attribute_parser(string& attribute_name,const string& value);

  // utilities for generating output string in json format

  void OutputNameValueSeparator();
  void OutputMessageStart();
  void OutputMessageEnd();
  void OutputFieldSeparator();
  void OutputRepeatedFieldStart();
  void OutputRepeatedFieldEnd();
  void OutputAttributeValueSeparator();
  void get_protos_from_api(std::string api);
  bool is_payload_started ;
  std::vector<payload_info> sensor_payload_info;
  std::string suid_low ;
  std::string suid_high ;
  bool _is_api_set = false;
};
