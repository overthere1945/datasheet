/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <sys/stat.h>
#include <cstring>
#include <cstdint>

#include "sensors_json_parser.h"
#include "sensors_log.h"

#ifdef SNS_WEARABLES_TARGET
#define CONFIG_FILE_PATH "/vendor/etc/sensors/hub1/config/sensing_hub_info.json"
#else
#define CONFIG_FILE_PATH "/vendor/etc/sensors/config/sensing_hub_info.json"
#endif

#define MAX_DATA_VALUE_LEN 15
#define COMM_TYPE_QMI    "0"
#define COMM_TYPE_GLINK  "1"

sensors_json_parser::sensors_json_parser():
  _read_comm_attr(false),
  _is_client_comm_grp(false),
  _sensing_hub_id(-1),
  _platform_id(""),
  _client_comm_type(""),
  _client_comm_value(-1),
  _sensing_hub_name_id_map_filled(false),
  _comm_map_filled(false),
  _is_file_loaded(false)
{
}

int sensors_json_parser::get_comm_handle_attrs(int hub_id)
{
  if(0 == _sensing_hub_comm_map.count(hub_id))
    return -1;

  return _sensing_hub_comm_map.at(hub_id).second;
}

std::vector<int> sensors_json_parser::get_hub_id_list()
{
  return _hub_ids;
}

int sensors_json_parser:: get_hub_id(std::string hub_name)
{
  if(0 == _sensing_hub_name_id_map.count(hub_name))
    return -1;

  return _sensing_hub_name_id_map.at(hub_name);
}

int sensors_json_parser::whitespace_len(char const* str, uint32_t str_len)
{
  uint32_t cidx = 0;

  while(0 != isspace(str[cidx]))
  {
    if('\n' == str[cidx])
    {
      _curr_line_number++;
    }
    if(cidx >= str_len)
    {
      return 0;
    }
    ++cidx;
  }
  return cidx;
}

uint32_t sensors_json_parser::parse_pair(char* json, uint32_t json_len, char** key,
                           char** value)
{
  uint32_t cidx = 0;
  char* end_string;
  bool failed = false;

  *value = NULL;

  if(cidx >= json_len || '\"' != json[cidx++])
  {
    sns_logd("parse_pair: Missing opening quote line no.: %d ", _curr_line_number);
    failed = true;
  }
  else if(NULL == (end_string = strchr(&json[cidx], '\"')))
  {
    sns_logd("parse_pair: Missing closing quote (%i)", _curr_line_number);
    failed = true;
  }
  else
  {
    *key = &json[cidx];
    cidx += (uintptr_t)end_string - (uintptr_t)&json[cidx];
    json[cidx++] = '\0'; // We want to safely use this string later
    cidx += whitespace_len(&json[cidx], json_len - cidx);

    if(cidx >= json_len || ':' != json[cidx++])
    {
      sns_logd("parse_pair: Missing colon Line No.: %d ", _curr_line_number);
      failed = true;
    }
    else
    {
      cidx += whitespace_len(&json[cidx], json_len - cidx);
      if(cidx < json_len && '\"' == json[cidx])
      {
        cidx++;
        *value = &json[cidx];
        end_string = strchr(&json[cidx], '\"');
        if(NULL == end_string)
        {
          sns_logd("parse_pair: Missing closing quote %d ", _curr_line_number);
          failed = true;
        }
        else
        {
          cidx += (uintptr_t)end_string - (uintptr_t)&json[cidx];
          json[cidx++] = '\0';
          cidx += whitespace_len(&json[cidx], json_len - cidx);
        }
      }
      else if(cidx < json_len && '{' != json[cidx] && '[' != json[cidx])
      {
        sns_logd("parse_pair: Next element is not a quote or opening bracket:%d ", _curr_line_number);
        failed = true;
      }
    }
    sns_logv("Parsed pair key:%s",*key);
  }

  return failed ? 0 : cidx;
}

uint32_t sensors_json_parser::parse_item(char* item_name, char* item_value, char* json, uint32_t json_len)
{
  uint32_t cidx = 0;
  uint32_t result = 0;

  if(cidx >= json_len || '{' != json[cidx++])
  {
    sns_logd("parse_item: Missing opening bracket Line no. %d ", _curr_line_number);
  }
  else
  {
    do
    {
      char* key, *value;
      uint32_t len;

      cidx += whitespace_len(&json[cidx], json_len - cidx);
      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key || NULL == value)
      {
        sns_logd("parse_item: Unable to parse pair Line no. %d ", _curr_line_number);
        return cidx;
      }
      else
      {
        if(0 == strcmp(key, "data"))
        {
          sns_logd("set item value:%s",value);
          strlcpy(item_value, value, MAX_DATA_VALUE_LEN);
        }
      }
      cidx += whitespace_len(&json[cidx], json_len - cidx);
    } while(cidx < json_len && ',' == json[cidx] && cidx++);

    if(cidx >= json_len || '}' != json[cidx++])
    {
      sns_logd("parse_item: Missing closing bracket Line no.  %d ", _curr_line_number);
      cidx = 0;
    }
  }
  return cidx;
}

void sensors_json_parser::update_sensing_hub_info(const char* grp_name, char* key, char* value)
{
  if(0 == strcmp(grp_name, ".client_comm"))
    _is_client_comm_grp = true;

  if(0 == strcmp(key, "platform_id"))
  {
     _platform_id = value;
  }
  else if(0 == strcmp(key, "sensing_hub_id"))
  {
     _sensing_hub_id = atoi(value);
     _hub_ids.push_back(_sensing_hub_id);
  }
  else if(0 == strcmp(key, "client_comm_type"))
  {
     _client_comm_type = value;
  }
  else if(_is_client_comm_grp && 0 == strcmp(key, "service_id"))
  {
    if(COMM_TYPE_QMI == _client_comm_type)
      _client_comm_value = atoi(value);
    _is_client_comm_grp = false;
  }
  else if(_is_client_comm_grp && strcmp(value, "apps") && strcmp(key, "link_name"))
  {
    _read_comm_attr = true;
    _is_client_comm_grp = false;
  }
  else if(0 == strcmp(key, "no_of_channels") && _read_comm_attr)
  {
    if(COMM_TYPE_GLINK ==_client_comm_type)
      _client_comm_value = atoi(value);
  }

  if((-1 !=_sensing_hub_id) && (!_platform_id.empty()))
  {
    _sensing_hub_name_id_map.insert({_platform_id, _sensing_hub_id});
    _sensing_hub_name_id_map_filled = true;
  }

  if(( -1 != _sensing_hub_id) && (!_client_comm_type.empty()) && (-1 != _client_comm_value ))
  {
    _sensing_hub_comm_map.insert({_sensing_hub_id, {(std::string)_client_comm_type, _client_comm_value}});
    _comm_map_filled= true;
  }

  if(_sensing_hub_name_id_map_filled && _comm_map_filled)
  {
    _sensing_hub_id = -1;
    _platform_id.clear();
    _client_comm_type.clear();
    _client_comm_value = -1;
    _read_comm_attr = false;
    _sensing_hub_name_id_map_filled = false;
    _comm_map_filled= false;
  }
}

int sensors_json_parser::parse_group(char const* grp_name, char* json,
                            uint32_t json_len, uint32_t* len)
{
  uint32_t cidx = 0;
  bool failed = false;

  *len = 0;

  if(cidx >= json_len || '{' != json[cidx++])
  {
    sns_loge("parse_group: Missing opening bracket Line no. %d ", _curr_line_number);
    return -1;
  }
  {
    do
    {
      char *key, *value;
      uint32_t len;

      cidx += whitespace_len(&json[cidx], json_len - cidx);
      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key)
      {
        sns_loge("%s: Unable to parse pair on line No %d ", __func__, _curr_line_number);
        return -1;
      }
      else if(NULL != value)
      {
        if(0 == strcmp(key, "owner"))
        {
          sns_logd("Owner of group:  %s ", value);
        }
        else
        {
          sns_loge("Owner of the group not found");
        }
      }
      else
      {
        if('.' == key[0])
        {
          parse_group(key, &json[cidx], json_len - cidx, &len);
          cidx += len;
        }
        else
        {
          char local_value[10];
          len = parse_item(key, &local_value[0], &json[cidx], json_len - cidx);
          sns_logv("local key:value %s:%s", key, local_value);
          update_sensing_hub_info(grp_name, key, local_value);
          cidx += len;
        }
        if(0 == len)
        {
          sns_loge("parse_group: Error parsing item:%s %d ", key, _curr_line_number);
          return -1;
        }
      }
      cidx += whitespace_len(&json[cidx], json_len - cidx);
    } while(',' == json[cidx] && cidx++);

    if(cidx >= json_len || '}' != json[cidx++])
    {
      sns_loge("parse_group: Missing closing bracket Line No. %d ", _curr_line_number);
      return -1;
    }
  }
  *len = cidx;
  return 0;
}

int32_t sensors_json_parser::parse_config(char* json, uint32_t json_len)
{
  int32_t cidx = 0;
  uint32_t len;
  char *key, *value;

  if(cidx >= json_len || '{' != json[cidx++])
  {
    sns_loge("parse_config: Missing open bracket %d ", _curr_line_number);
    cidx = 0;
  }
  else
  {
    do
    {
      cidx += whitespace_len(&json[cidx], json_len - cidx);
      if('}' == json[cidx])
        break;
    } while(cidx < json_len && ',' == json[cidx] && cidx++);
  }

  if(0 < cidx)
  {
    cidx += whitespace_len(&json[cidx], json_len - cidx);
  }
  if(0 < cidx && (cidx >= json_len || '}' != json[cidx++]))
  {
    sns_loge("parse_config: Missing closing bracket Line No. %d ", _curr_line_number);
    cidx = 0;
  }
  return cidx;
}

int sensors_json_parser::parse_file(char* json, uint32_t json_len)
{
  uint32_t cidx = 0;
  uint32_t len;
  int32_t ret;
  char *key, *value;

  _curr_line_number = 0;
  sns_logi("%s: started parsing sensing hub info json file", __func__);
  cidx = whitespace_len(json, json_len);
  if(cidx >= json_len || '{' != json[cidx++])
  {
    sns_loge("Missing open bracket in config file");
    return -1;
  }

  do {
    cidx += whitespace_len(&json[cidx], json_len - cidx);
    len = parse_pair(&json[cidx], json_len - cidx, &key, &value);

    if(0 == len || NULL == key || NULL != value)
    {
        sns_loge("Pair parsing failed");
        return -1;
    }
    cidx += len;
    if(0 == strcmp(key, "config"))
    {
      ret = parse_config(&json[cidx], json_len - cidx);
      if(0 == ret)
        sns_loge("Error parsing config field");
      cidx += ret;
    }
    else if(0 != parse_group(key, &json[cidx], json_len - cidx, &len))
    {
      sns_loge("Error parsing group: key=%s, %d", key, _curr_line_number);
      return -1;
    }
    else
    {
      cidx += len;
    }
    sns_logd("Parsing next group");
  } while(cidx < json_len && ',' == json[cidx] && cidx++);

  sns_logi("Parsing Complete");
  return 0;
}

/* call this function to start parsing */
int sensors_json_parser::load_file()
{

  if(_is_file_loaded)
  {
    sns_logi("config file already loaded");
    return 0;
  }

  struct stat buf;
  if(0 != stat(CONFIG_FILE_PATH, &buf))
  {
    sns_loge("stat failed for config file");
    return -1;
  }
  char* file_content = (char*)malloc(buf.st_size);
  if(nullptr == file_content)
  {
    sns_loge("failed to allocate memory for reading file content");
    return -1;
  }
  FILE* fptr = NULL;
  fptr = fopen(CONFIG_FILE_PATH, "r");
  if(NULL == fptr)
  {
    sns_loge("failed to open sensing_hub_config file");

    free(file_content);
    return -1;
  }
  if(fread(file_content, sizeof(char), buf.st_size, fptr) < buf.st_size)
  {
    sns_loge("failed to read sensing_hub_config");
    free(file_content);
    fclose(fptr);
    return -1;
  }
  fclose(fptr);
  sns_logi("parsing the contents of the file");
  if(0 != parse_file(file_content, buf.st_size))
  {
    free(file_content);
    return -1;
  }
  free(file_content);
  _is_file_loaded = true;
  return 0;
}
