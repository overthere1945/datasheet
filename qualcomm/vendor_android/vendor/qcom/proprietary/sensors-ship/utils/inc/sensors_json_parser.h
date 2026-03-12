/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

#include <unordered_map>
#include <vector>
#include <string>

class sensors_json_parser
{
public:
  sensors_json_parser();
  ~sensors_json_parser(){};

  /**
   * @brief get handle to sensors_json_parser get_instance
   * @return parser_instance&
   */
  static sensors_json_parser& get_instance()
  {
      static sensors_json_parser parser_instance;
      return parser_instance;
  }

  /* reads sensing_hub_config file & starts parsing */
  int load_file();
  /* returns list of supported hub ids */
  std::vector<int> get_hub_id_list();
  /* gets the hub id for a given hub name */
  int get_hub_id(std::string hub_name);
  /* fetches the comm attributes for a hub id */
  int get_comm_handle_attrs(int hub_id);

private:

  std::unordered_map<std::string, int> _sensing_hub_name_id_map;
  std::unordered_map<int, std::pair<std::string, int>> _sensing_hub_comm_map;
  int _curr_line_number;
  std::vector<int> _hub_ids;

  bool _read_comm_attr;
  bool _is_client_comm_grp;

  int _sensing_hub_id;
  std::string _platform_id;
  std::string _client_comm_type;
  int _client_comm_value;

  bool _sensing_hub_name_id_map_filled;
  bool _comm_map_filled;
  bool _is_file_loaded;

  int whitespace_len(char const* str, uint32_t str_len);

  uint32_t parse_pair(char* json, uint32_t json_len, char** key,
                                                      char** value);

  uint32_t parse_item(char* item_name, char* item_value, char* json, uint32_t json_len);

  void update_sensing_hub_info(const char* grp_name, char* key, char* value);

  int parse_group(char const* grp_name, char* json,
                          uint32_t json_len, uint32_t* len);

  int32_t parse_config(char* json, uint32_t json_len);

  int parse_file(char* json, uint32_t json_len);
};
