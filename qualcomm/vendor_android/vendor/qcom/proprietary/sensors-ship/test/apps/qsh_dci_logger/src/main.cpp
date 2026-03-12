/*============================================================================
  Copyright (c) 2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file main.cpp
  @brief Sensors main file for forwarding PRINTs to Logcat.
============================================================================*/

#include "qsh_dci_logger.h"

#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <exception>

#include <getopt.h>
#include <unistd.h>


int wait_for_signal[2] = {0};
qsh_dci_logger *dci_handle = NULL;
std::terminate_handler original_handler = NULL;


/* Display Application Usage */
static void usage(char *progname, char *mask_file_path)
{
    QSH_LOGD("\nUsage for %s:\n", progname);
    QSH_LOGD("\n-h  --usage:\t Display supported options    (optional)\n");
    QSH_LOGD("\n-p  --f3msg:\t Printf forwarded F3 Messages (optional)\n");
    QSH_LOGD("\n-m  --mfile:\t Custom DIAG Mask CFG file    (optional)\n");
    QSH_LOGD("              \t Default DIAG Mask CFG file is: %s\n", mask_file_path);
    QSH_LOGD("\n");
}

/* Parse CLI Arguements */
static int parse_args(int argc, char **argv, int *print_to_console,
                      char *default_mask_file, char **mask_file_path)
{
    int command = 0, success = 0;
    struct option longopts[] =
    {
        {"usage", 0, NULL, 'h'},
        {"f3msg", 0, NULL, 'p'},
        {"mfile", 1, NULL, 'm'},
    };

    while ((command = getopt_long(argc, argv, "hpm:", longopts, NULL)) != -1)
    {
        switch (command)
        {
        case 'm':
            *mask_file_path = (char*)optarg;
            break;
        case 'p':
            *print_to_console = 1;
            break;
        case 'h':
        default:
            usage(argv[0], default_mask_file);
            success = -1;
            break;
        }
    }

    return success;
}

/* handle Ctrl+C Signal */
void sigint_handler_cb(int number)
{
    QSH_LOGD("\nqsh_dci_logger: CTRL+C [%d] detected, now exiting...\n", number);
    close(wait_for_signal[1]); /* Close Pipe Write FD */
}

/* Handle Abnormal exit */
void terminate_handler_cb()
{
    QSH_LOGD("qsh_dci_logger: Unhandled Exception...\n");
    dci_handle->deinit();
    delete dci_handle;

    if(original_handler)
    {
        original_handler();
    }
}

/* Main Function */
int main(int argc, char *argv[])
{
    int print_to_console = 0, status = 1;
    char *mask_file_path = NULL,
          default_mask_file[QSH_MAX_FILE_NAME_LEN] = {0};

    /* Pipe for receiving waiting on user Signal */
    if (pipe(wait_for_signal) == -1)
    {
        QSH_LOGE("qsh_dci_logger: pipe int failed, err: %d\n", errno);
        exit(1);
    }

    qsh_dci_logger::get_default_mask_file(default_mask_file,
                                          sizeof(default_mask_file));
    status = parse_args(argc, argv, &print_to_console,
                        default_mask_file, &mask_file_path);
    if (status)
    {
        exit(0);
    }

    QSH_LOGD("qsh_dci_logger: Mask File = %s\n", mask_file_path);
    QSH_LOGD("qsh_dci_logger: Print Messages = %d\n", print_to_console);

    /* Object for qsh_dci_logger */
    dci_handle = new qsh_dci_logger();

    /* Initialize DIAG DCI interface */
    status = dci_handle->init(print_to_console, mask_file_path);
    if (status)
    {
        QSH_LOGE("qsh_dci_logger: init DCI interface failed.\n");
        exit(status);
    }

    /* Now any logs will be sent to the registered callback function. */
    QSH_LOGD("qsh_dci_logger: Press CTRL+C to exit...\n");
    fflush(stdout);

    /* Handle abnormal program termination */
    original_handler = std::set_terminate(terminate_handler_cb);

    /* Handle user termination signal */
    signal(SIGINT, sigint_handler_cb);

    /* Wait for user termination signal */
    while (-1 == read(wait_for_signal[0], NULL, 1) && EINTR == errno);

    /* Close Pipe Read FD */
    close(wait_for_signal[0]);

    /* De-initialize DIAG DCI interface */
    status = dci_handle->deinit();
    if (status)
    {
        QSH_LOGE("qsh_dci_logger: de-init DCI interface failed.\n");
        exit(status);
    }

    delete dci_handle;

    return 0;
}
