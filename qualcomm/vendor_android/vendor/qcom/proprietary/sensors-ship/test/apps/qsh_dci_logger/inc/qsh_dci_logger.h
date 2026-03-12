/*============================================================================
  Copyright (c) 2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file qsh_dci_logger.h
  @brief Sensors interface file for using DCI to forward PRINTs to Logcat.
============================================================================*/

#pragma once

#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <thread>

#define QSH_MAX_FILE_NAME_LEN 128
#define QSH_NUM_SSIDs_PRIOs 6
#define QSH_MAX_NUM_ARGS 9
#define QSH_NUM_PIPE_FDs 2

#define QSH_LOGD(...)                                            \
{                                                                \
  __android_log_print(ANDROID_LOG_DEBUG, "QSH_LOG", __VA_ARGS__);\
  { fprintf(stdout, __VA_ARGS__); }                              \
}

#define QSH_LOGE(...)                                            \
{                                                                \
  __android_log_print(ANDROID_LOG_ERROR, "QSH_LOG", __VA_ARGS__);\
  { fprintf(stderr, __VA_ARGS__); }                              \
}

/* Container for Logger Data and Functions */
class qsh_dci_logger
{
    public:
        qsh_dci_logger(void);
       ~qsh_dci_logger(void);

        int init(int print_to_console, const char *diag_mask_file);
        int deinit(void);

        static void get_default_mask_file(char *mask_file_path, size_t length);

    private:
        bool     _thread_ready, _pipes_ready, _diag_ready;

        uint8_t  _command, _type, _num_args, _drop_count, _hour, _minute;
        uint16_t _line_no, _ssid;
        uint32_t _priority;

        int      _prnt_msg, _proc_type;
        int      _msg_fds[QSH_NUM_PIPE_FDs], _len_fds[QSH_NUM_PIPE_FDs];

        double   _second;

        std::thread *_thread;

        static const char constexpr *_default_mask_file =
            "/data/vendor/sensors/diag_cfg/qsh_F3_logs.cfg";

        const char *_ssid_str, *_prio_str, *_log_dir_path;
        const char *_ssid_strings[QSH_NUM_SSIDs_PRIOs], *_priority_strings[QSH_NUM_SSIDs_PRIOs];

        uint64_t _time_stamp;
        uint64_t _args[QSH_MAX_NUM_ARGS];

        void handle_flag_sequence(uint8_t * const msg_buf, size_t msg_len);
        void process_diag_data(const uint8_t *msg_buf, size_t length);
        void process_message_data(void);

        static int diag_data_cb(uint8_t *ptr, int len, void *cxt);
};
