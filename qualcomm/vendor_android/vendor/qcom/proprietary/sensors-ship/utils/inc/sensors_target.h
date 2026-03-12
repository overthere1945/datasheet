/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#pragma once

#include <string>
#include <sys/stat.h>
#include <dirent.h>

/*Class to detect sensors target DSP*/
class sensors_target {
public:
  typedef enum {
    DSP_TYPE_UNKNOWN,
    DSP_TYPE_SLPI,
    DSP_TYPE_ADSP,
  } dsp_type;

  static dsp_type get_dsp_type() {
    static bool is_probed = false;
    static dsp_type dsp = DSP_TYPE_UNKNOWN;
    DIR *remote_proc_dir = nullptr;
    if (is_probed) {
      return dsp;
    } else {
      remote_proc_dir = opendir("/sys/class/remoteproc");
      if(nullptr == remote_proc_dir) {
        return dsp;
      }

      struct dirent* dir_entry;
      while((dir_entry = readdir(remote_proc_dir))){
        if(0 == strcmp(dir_entry->d_name, ".") ||
           0 == strcmp(dir_entry->d_name, "..")) {
          continue;
        }

        std::string remoteproc_name = "" ;
        remoteproc_name = remoteproc_name + "/sys/class/remoteproc/" + dir_entry->d_name + "/name";
        FILE *firmware_name_ptr = fopen(remoteproc_name.c_str(), "r");
        if(nullptr == firmware_name_ptr) {
          continue;
        }
        fseek(firmware_name_ptr, 0 , SEEK_END);
        int firmware_name_size = ftell(firmware_name_ptr);
        fseek(firmware_name_ptr , 0 , SEEK_SET);

        std::string firmware_name = "";
        firmware_name.resize(firmware_name_size);

        fread(&firmware_name[0], 1, firmware_name_size,  firmware_name_ptr);
        if(firmware_name_ptr) {
          fclose(firmware_name_ptr);
        }

        int pos = -1;
        pos = firmware_name.find("adsp", 0);
        if(-1 != pos) {
          dsp = DSP_TYPE_ADSP;
          is_probed = true;
          break;
        }
        pos = -1;
        pos = firmware_name.find("slpi", 0);
        if(-1 != pos) {
          dsp = DSP_TYPE_SLPI;
          is_probed = true;
          break;
        }
      }

      if(remote_proc_dir) {
        closedir(remote_proc_dir);
      }

      return dsp;
    }
  }

  static bool is_slpi() {
    if (DSP_TYPE_SLPI == get_dsp_type())
      return true;
    else
      return false;
  }

  static bool is_adsp() {
    if (DSP_TYPE_ADSP == get_dsp_type())
      return true;
    else
      return false;
  }

  static std::string get_ssr_path() {
    std::string ssr_path ="";
    dsp_type dsp = get_dsp_type();
    if (DSP_TYPE_SLPI == dsp)
      ssr_path = slpi_sysfs_node + "ssr";
    else if (DSP_TYPE_ADSP == dsp)
      ssr_path = adsp_sysfs_node + "ssr";
    return ssr_path;
  }

private:
static inline const std::string slpi_sysfs_node = "/sys/kernel/boot_slpi/";
static inline const std::string adsp_sysfs_node = "/sys/kernel/boot_adsp/";
};
