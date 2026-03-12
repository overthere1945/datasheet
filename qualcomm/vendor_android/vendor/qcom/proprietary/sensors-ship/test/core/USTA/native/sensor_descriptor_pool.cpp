/*============================================================================
  Copyright (c) 2017, 2021-2022 , 2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.


  @file sensor_descriptor_pool.cpp
  @brief
  sensor_descriptor_pool definition.
============================================================================*/

#include "sensor_descriptor_pool.h"
#include <dirent.h>
#include <sensors_log.h>

using namespace std;

void add_to_proto_vector(FILE* &proto_file, char* file,
    vector<FileDescriptorProto>& vfdp);
void sort_proto_vector(vector<FileDescriptorProto> &vfdp,
    vector<FileDescriptorProto>& vfdp_sorted);

// global descriptorpool
DescriptorPool pool;
bool is_pool_generated = false;
usta_rc build_descriptor_pool()
{
  if(true == is_pool_generated)
    return USTA_RC_SUCCESS;
  vector<FileDescriptorProto> vfdp;
  // vector containing file descriptor proto of each .proto file
  vector<FileDescriptorProto> vfdp_sorted;
  // sorted vector in order of dependency
  bool google_descriptor_proto_found = false;
  usta_rc rc = USTA_RC_SUCCESS;
  DIR* FD;
  struct dirent* in_file;
  FILE    *file_strm;
#ifdef __ANDROID_API__
  string path("/vendor/etc/sensors/proto/");
#else
  string path("/etc/sensors/proto/");
#endif

  if (NULL == (FD = opendir (path.c_str())))
  {
   sns_loge("Failed to open input directory\n");
    rc = USTA_RC_FAILED;
    return rc;
  }
  while ((in_file = readdir(FD)))
  {
    string file_path;
    if (!strcmp(in_file->d_name,".")||!strcmp(in_file->d_name,"..")) continue;

    file_path = path+in_file->d_name;
    file_strm = fopen(file_path.c_str(), "r");

    sns_logd(" start Building descriptor pool for %s", in_file->d_name);

    // special case for decriptor.proto
    string desc_proto("descriptor.proto");
    if (desc_proto.compare(in_file->d_name)==0)
    {
      google_descriptor_proto_found = true;
      add_to_proto_vector(file_strm, (char *)"google/protobuf/descriptor.proto", vfdp);
    }
    else
    {
      add_to_proto_vector(file_strm, in_file->d_name, vfdp);
    }
  }
  if (!google_descriptor_proto_found)
  {
    sns_loge(" Google's descriptor.proto no found");
    return USTA_RC_FAILED;
  }


  sort_proto_vector(vfdp, vfdp_sorted);

  sns_logd("\n sorted dependecy list are \n");

  for (auto &it : vfdp_sorted)
  {
    sns_logd(" proto file is %s", it.name().c_str());
  }

  // build descriptor pool using sorted list
  for (auto &it : vfdp_sorted)
  {
    const FileDescriptor* fdesc = pool.BuildFile(it);
    if (fdesc == NULL)
    {
      sns_logd(" error in generating descriptor pool for %s", it.name().c_str());
    }
  }
  is_pool_generated = true;
  return rc;

}

void add_to_proto_vector(FILE* &proto_file, char* file,
    vector<FileDescriptorProto>& vfdp)
{

  int file_desc = fileno(proto_file);
  FileInputStream proto_input_stream(file_desc);
  Tokenizer tokenizer(&proto_input_stream, NULL);
  FileDescriptorProto file_desc_proto;
  Parser parser;

  if (!parser.Parse(&tokenizer, &file_desc_proto))
  {
    sns_logd("Cannot parse .proto file: %s", file);
  }
  fclose(proto_file);
  if (!file_desc_proto.has_name())
  {
    file_desc_proto.set_name(file);
  }
  vfdp.push_back(file_desc_proto);
}

bool dependency_present(string s, vector<FileDescriptorProto>& vfdp_sorted)
{
  bool found = false;;
  for (auto &it : vfdp_sorted)
  {
    if (it.name().compare(s) == 0) found = true;
  }

  return found;
}

void sort_proto_vector(vector<FileDescriptorProto> &vfdp,
    vector<FileDescriptorProto>& vfdp_sorted)
{
  vector<FileDescriptorProto>::iterator it_insert;
  vector<FileDescriptorProto>::iterator it;

  bool dependency_met = false;
  while (!vfdp.empty())
  {

    for (it = vfdp.begin(); it != vfdp.end(); ++it)
    {
      dependency_met = true;
      if (it->dependency_size() == 0)
      {
        it_insert = vfdp_sorted.begin();
        break;
      }

      for (int i = 0; i < it->dependency_size(); i++)
      {
        if (!(dependency_present(it->dependency().Get(i), vfdp_sorted)))
        {
          dependency_met = false;
          break;
        }
      }
      if (dependency_met)
      {
        it_insert = vfdp_sorted.end();
        break;
      }
    }

    if (dependency_met)
    {
      vfdp_sorted.insert(it_insert, *it);
      vfdp.erase(it);
    }
  }
}
