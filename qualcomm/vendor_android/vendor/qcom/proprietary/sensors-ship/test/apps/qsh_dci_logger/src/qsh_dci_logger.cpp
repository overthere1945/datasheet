/*============================================================================
  Copyright (c) 2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file qsh_dci_logger.cpp
  @brief Sensors implementation file for using DCI to forward PRINTs to Logcat.
============================================================================*/

#include "msg.h"
#include "msgcfg.h"
#include "diagpkt.h"
#include "diag_lsm.h"
#include "qsh_dci_logger.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>

#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif

#define QSH_MAX_F3_LEN 768 /* HDR = 128; FMT = 512; FILE = 128; */
#define QSH_FMT_STR_MIN_OFFSET 20 /* Minimum offset of print format string */
#define QSH_PRINT_FMT "[%-10s:%7s][%03d:%02d:%06.3f][%-24s:%5d] "
#define QSH_LOGM(console, fmt, ...)                    \
{                                                      \
  __android_log_print(ANDROID_LOG_INFO, "QSH_LOG",     \
                      fmt, __VA_ARGS__);               \
  if (console) {                                       \
      char buf[QSH_MAX_F3_LEN] = {0};                  \
      snprintf(buf, QSH_MAX_F3_LEN, fmt, __VA_ARGS__); \
      fprintf(stdout, "%s\n", buf); fflush(stdout); }  \
}

/**
* @brief get_default_mask_file,
*        Static function to get the default Sensors DIAG CFG File.
*
* @param[i] mask_file_path.
*           Pointer to return the full path of default mask file.
*
* @param[i] length.
*           Length of mask file path to buffer to be written.
*
* @param[o] void.
*
*/
void qsh_dci_logger::get_default_mask_file(char *mask_file_path, size_t length)
{
    if(NULL != mask_file_path)
    {
        strlcpy(mask_file_path, _default_mask_file, length);
    }
}

/**
* @brief qsh_dci_logger,
*        Default constructor for the class
*        Create Pipes, spawn the logger thread and initializes DCI.
*
* @param[i] void.
*
* @param[o] void.
*
*/
qsh_dci_logger::qsh_dci_logger()
{
    _ssid_strings[0] = "FRAMEWORK";
    _ssid_strings[1] = "PLATFORM";
    _ssid_strings[2] = "SENSOR_INT";
    _ssid_strings[3] = "SENSOR_EXT";
    _ssid_strings[4] = "SNS";
    _ssid_strings[5] = "UNKNOWN";

    _priority_strings[0] = "LOW";
    _priority_strings[1] = "MEDIUM";
    _priority_strings[2] = "HIGH";
    _priority_strings[3] = "ERROR";
    _priority_strings[4] = "FATAL";
    _priority_strings[5] = "UNKNOWN";

    _proc_type    = MSM;
    _log_dir_path = "/data/vendor/sensors";

    _diag_ready   = false;
    _pipes_ready  = false;
    _thread_ready = false;
}

/**
* @brief ~qsh_dci_logger,
*        Default destructor for the class
*        Ensures Pipes, logger thread and DCI are released.
*
* @param[i] void.
*
* @param[o] void.
*
*/
qsh_dci_logger::~qsh_dci_logger()
{
    deinit();
}

/**
* @brief init,
*        Function to initialize DIAG DCI interface to receive log callbacks.
*
* @param[i] print_to_console.
*           Flag to indicate if logs are also to printed to console.
*
* @param[i] diag_mask_file.
*           The path of the user defined DIAG Mask config file.
*
* @param[o] status.
*           0 = success; 1 = failure.
*/
int qsh_dci_logger::init(int print_to_console, const char *diag_mask_file)
{
    bool status = false;

    /* Open Pipes for storing messages and length. */
    if (pipe(_msg_fds) == -1 || pipe(_len_fds) == -1)
    {
        QSH_LOGE("qsh_dci_logger: pipe int failed, err: %d\n", errno);
        return 1;
    }
    _pipes_ready = true;

    /* Get the handle to Diag. */
    status = Diag_LSM_Init(NULL);
    if (!status)
    {
        QSH_LOGE("qsh_dci_logger: Diag LSM Init failed, err: %d\n", errno);
        return 1;
    }

    /* Register the callback function for receiving data from MSM. */
    diag_register_callback(diag_data_cb, (void*)this);

    /* Switch to Callback mode to receive Diag data in this application. */
    diag_switch_logging(CALLBACK_MODE, NULL);
    QSH_LOGD("qsh_dci_logger: Activated forwarding of F3s to Logcat from MSM\n");

    /* Override DCI Library default logging directory to Sensors */
    strlcpy(output_dir[MSM], _log_dir_path, sizeof(output_dir[MSM]));

    _prnt_msg     = print_to_console;

    /* Update default mask file with user defined file */
    if(!diag_mask_file)
    {
        diag_mask_file = _default_mask_file;
    }

    /* Set DIAG Masks from config file. */
    status = diag_set_masks(MSM, diag_mask_file);
    if (status)
    {
        QSH_LOGE("qsh_dci_logger: Error reading mask file: %s\n", diag_mask_file);
        return 1;
    }

    _diag_ready = true;

    /* Start thread for processing incoming F3 Messages. */
    _thread = new std::thread(&qsh_dci_logger::process_message_data, this);

    return 0;
}

/**
* @brief deinit,
*        Function to de-initialize DIAG DCI interface.
*
* @param[i] void.
*
* @param[o] status.
*           0 = success; 1 = failure.
*/
int qsh_dci_logger::deinit()
{
    size_t length = 0;
    ssize_t ret_l = 0;
    bool status = false;

    if(_diag_ready)
    {
        /* We are done using the Callback Mode, switch back to USB Mode. */
        diag_switch_logging(USB_MODE, NULL);

        status = Diag_LSM_DeInit(); /* Release the handle to Diag. */
        if (!status)
        {
            QSH_LOGE("qsh_dci_logger: Unable to deinit diag driver, err: %d\n", errno);
            return 1;
        }
        _diag_ready = false;
        QSH_LOGD("qsh_dci_logger: Deactivated DIAG interface.\n");
    }

    if(_thread_ready)
    {
        ret_l = write(_len_fds[1], &length, sizeof(size_t)); /* Break the reading loop */
        if(sizeof(size_t) != ret_l)
        {
            QSH_LOGE("qsh_dci_logger: Error Break the reading loop.\n");
        }
        _thread->join(); /* Wait for thread to clean up */
        delete _thread;
        QSH_LOGD("qsh_dci_logger: Deactivated logging thread.\n");
    }

    if(_pipes_ready)
    {
        /* Close Pipe FDs */
        close(_msg_fds[0]);
        close(_msg_fds[1]);
        close(_len_fds[0]);
        close(_len_fds[1]);
        _pipes_ready = false;
        QSH_LOGD("qsh_dci_logger: Deactivated message queues.\n");
    }

    return 0;
}

/**
* @brief process_message_data,
*        Thread Function to process received F3 Messages and forward to logcat.
*
* @param[i] void.
*
* @param[o] void.
*
*/
void qsh_dci_logger::process_message_data()
{
    uint8_t *msg_buf = NULL;
    ssize_t  read_bytes = 0;
    size_t   ext_fmt_len = 0, msg_len = 0;
    uint8_t  payload_buffer[QSH_MAX_F3_LEN] = {0};
    char    *msg_format = NULL, *msg_file_name = NULL;

    _thread_ready = true;
    do /* Process incoming F3 Messages until user signals exit. */
    {
        memset(payload_buffer, 0, QSH_MAX_F3_LEN);

        /* Read Message Length */
        read_bytes = 0; msg_len = 0;
        do
        {
            read_bytes = read(_len_fds[0], &msg_len, sizeof(size_t));
        } while (-1 == read_bytes && EINTR == errno);

        if (sizeof(size_t) != read_bytes || msg_len > QSH_MAX_F3_LEN)
        {
            QSH_LOGE("qsh_dci_logger: Read Error | Logger Exiting...\n");
            break; /* Break loop and exit thread */
        }

        if (0 == msg_len)
        {
            QSH_LOGD("qsh_dci_logger: Exit Signal | Logger Exiting...\n");
            break; /* Break loop and exit thread */
        }

        /* Keep minimum room for additional formatting to be appended
         * to original format string (account for variable args). */
        msg_buf = payload_buffer +
                  strlen(QSH_PRINT_FMT) - QSH_FMT_STR_MIN_OFFSET;

        /* Read Message Payload */
        read_bytes = 0;
        do
        {
            read_bytes = read(_msg_fds[0], msg_buf + read_bytes, msg_len);
        } while (-1 == read_bytes && EINTR == errno);

        if(0 != msg_len - read_bytes)
        {
            QSH_LOGE("qsh_dci_logger: Read Error | Logger Exiting...\n");
            break; /* Break loop and exit thread */
        }

        handle_flag_sequence(msg_buf, msg_len); /* Handle 7E, 7D */

        _command    = msg_buf[0];
        _type       = msg_buf[1];
        _num_args   = msg_buf[2];
        _drop_count = msg_buf[3];
        _time_stamp = (uint64_t)msg_buf[11] << 54 | (uint64_t)msg_buf[10] << 48 |
                      (uint64_t)msg_buf[9]  << 40 | (uint64_t)msg_buf[8]  << 32 |
                      (uint64_t)msg_buf[7]  << 24 | (uint64_t)msg_buf[6]  << 16 |
                      (uint64_t)msg_buf[5]  << 8  | (uint64_t)msg_buf[4];
        _line_no    = (uint16_t)((uint16_t)msg_buf[13] << 8 | (uint16_t)msg_buf[12]);
        _ssid       = (uint16_t)((uint16_t)msg_buf[15] << 8 | (uint16_t)msg_buf[14]);
        _priority   = (uint32_t)msg_buf[19] << 24 | (uint32_t)msg_buf[18] << 16 |
                      (uint32_t)msg_buf[17] << 8  | (uint32_t)msg_buf[16];

        switch(_ssid)
        {
            case MSG_SSID_SNS_FRAMEWORK:
                _ssid_str = _ssid_strings[0];
                break;
            case MSG_SSID_SNS_PLATFORM:
                _ssid_str = _ssid_strings[1];
                break;
            case MSG_SSID_SNS_SENSOR_EXT:
                _ssid_str = _ssid_strings[3];
                break;
            case MSG_SSID_SNS_SENSOR_INT:
            case MSG_SSID_SNS:
            default:
                /* Skip SNS (Legacy), SENSOR_INT (Internal) and UNKNOWN (Non-Sensor) Logs.
                 * This switch case can be updated to add support for other SSIDs in future.
                 */
                continue;
        }

        switch (_priority)
        {
            case MSG_LEGACY_LOW :
                _prio_str = _priority_strings[0];
                break;
            case MSG_LEGACY_MED:
                _prio_str = _priority_strings[1];
                break;
            case MSG_LEGACY_HIGH:
                _prio_str = _priority_strings[2];
                break;
            case MSG_LEGACY_ERROR:
                _prio_str = _priority_strings[3];
                break;
            case MSG_LEGACY_FATAL:
                _prio_str = _priority_strings[4];
                break;
            default:
                _prio_str = _priority_strings[5];
                break;
        }

        memset(_args, 0, sizeof(_args));
        for (int i = 0; i < _num_args && i < QSH_MAX_NUM_ARGS; i++)
        {
            _args[i] =  msg_buf[23 + i*4] << 24 | msg_buf[22 + i*4] << 16 |
                        msg_buf[21 + i*4] << 8  | msg_buf[20 + i*4];
            _args[i] = _args[i] & 0xFFFFFFFF;
        }

        ext_fmt_len = strlen(QSH_PRINT_FMT);
        msg_format = (char*) (msg_buf + 20 + (4 * _num_args) - ext_fmt_len);
        strlcpy(msg_format, QSH_PRINT_FMT, ext_fmt_len);
        *(msg_format + ext_fmt_len - 1) = ' ';

        msg_file_name = msg_format + strlen(msg_format) + 1;
        _time_stamp   = 5 * ((_time_stamp >> 16)/4 + (_time_stamp & 0x0000FFFF)/167808);

        _hour   = _time_stamp/3600000;
        _minute = _time_stamp/60000  - _hour*60;
        _second = _time_stamp/1000.0 - _hour*3600 - _minute*60;

        QSH_LOGM(_prnt_msg, msg_format,
                 _ssid_str, _prio_str, _hour, _minute, _second, msg_file_name, _line_no,
                 _args[0], _args[1], _args[2], _args[3], _args[4], _args[5], _args[6], _args[7], _args[8]);
    }
    while(1);

    _thread_ready = false;
}

/**
* @brief process_diag_data,
*        Function to process incoming DIAG log packets.
*
* @param[i] msg_buf.
*           Pointer to received log packet data buffer.
*
* @param[i] length.
*           Length of received log packet in bytes.
*
* @param[o] void.
*
*/
void qsh_dci_logger::process_diag_data(const uint8_t *msg_buf, size_t length)
{
    ssize_t ret_l = sizeof(size_t), ret_p = length;

    /* Handle only F3 Messages */
    if (length > 20 && length < QSH_MAX_F3_LEN &&
        NULL != msg_buf && msg_buf[0] == 0x79 && msg_buf[1] == 0x00)
    {
        ret_l = write(_len_fds[1], &length, sizeof(size_t));
        ret_p = write(_msg_fds[1], msg_buf, length);
    }

    if(0 != sizeof(size_t) - ret_l || 0 != length - ret_p)
    {
        QSH_LOGE("qsh_dci_logger: Error writing logs to Pipes.\n");
    }
}

/**
* @brief handle_flag_sequence,
*        Function to replace 0x7D, 0x5E with 0x7E and 0x7D, 0x5D with 0x7D.
*
* @param[i] msg_buf.
*           Pointer to log packet data buffer.
*
* @param[i] length.
*           Length of log packet in bytes.
*
* @param[o] void.
*
*/
void qsh_dci_logger::handle_flag_sequence(uint8_t * const msg_buf, size_t msg_len)
{
    for (size_t i = 0; i < msg_len; i++)
    {
        if (msg_buf[i] == 0x7d)
        {
            if (msg_buf[i+1] == 0x5d)
            {
                msg_buf[i] = 0x7d;
            }
            if(msg_buf[i+1] == 0x5e)
            {
                msg_buf[i] = 0x7e;
            }

            /* Shift the payload bytes by 1 after removing flag sequence. */
            for (size_t j = 0; j < msg_len - i - 2; j++)
            {
                msg_buf[i+j+1] = msg_buf[i+j+2];
            }
            msg_buf[msg_len - 1] = '\0';
        }
    }
}

/**
* @brief diag_data_cb,
*        DCI Callback Function to handle incoming DIAG log packets.
*
* @param[i] ptr.
*           Pointer to received log packet data buffer.
*
* @param[i] len.
*           Length of received log packet in bytes.
*
* @param[i] cxt.
*           Pointer to retrive the context information in the callback.
*
* @param[o] status.
*           0 = success; -1 = failure.
*/
int qsh_dci_logger::diag_data_cb(uint8_t *ptr, int len, void *cxt)
{
    qsh_dci_logger *logger = NULL;
    if (NULL != ptr && NULL != cxt)
    {
        logger = (qsh_dci_logger*)cxt;
        if (logger->_proc_type != MSM)
        {
            QSH_LOGE("qsh_dci_logger: Received %d bytes from unexpected proc %d\n",
                     len, logger->_proc_type);
        }
        else
        {
            logger->process_diag_data(ptr, (size_t)len);
        }
    }

    return 0;
}
