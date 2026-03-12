/******************************************************************************
* File: sns_device_mode_example.cpp
*
* Copyright (c) 2023 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/

/*==============================================================================
    Include Files
==============================================================================*/
#include "sns_device_mode.h"
#include "sns_device_mode.pb.h"
#include <string>
#include "remote.h"
#include <unistd.h>
#include "AEEStdErr.h"
#include <iostream>
#include <inttypes.h>
using namespace std;

/*=============================================================================
    Macros
=============================================================================*/
#define DEVICE_MODE_NUMBER 0
#define DEVICE_MODE_NUMBER_STATE 1

std::atomic<bool> is_device_mode_handle_opened = false;
static remote_handle64 device_mode_remote_handle = 0;
remote_handle64 remote_handle_fd = 0;
string uri = "file:///libsns_device_mode_skel.so?sns_device_mode_skel_handle_invoke&_modver=1.0";

/****
    * @brief open remote handle for sensorspd - adsp
    */
void frpc_init()
{
    uri +="&_dom=adsp";
    if (remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=adsp", &remote_handle_fd)) {
        printf("failed to open remote handle for sensorspd - adsp\n");
    }
}

/****
    * @brief open sns_device_mode handle for sensorspd
    */
void sns_device_mode_handle_open()
{
    int nErr = AEE_SUCCESS;
    remote_handle64 handle_l;
    if (AEE_SUCCESS == (nErr = sns_device_mode_open(uri.c_str(), &handle_l))) {
    printf("sns_device_mode_open success for sensorspd - handle_l is %ud\n" , (unsigned int)handle_l);
    device_mode_remote_handle = handle_l;
    is_device_mode_handle_opened = true;
    } else {
    device_mode_remote_handle = 0;
    }
}

/****
    * @brief encode request
    * @param mode_number - DEVICE_MODE_NUMBER
    * @param mode_state - DEVICE_MODE_NUMBER_STATE
    * @param encoded_request - string to update encoded request
    */
bool get_encoded_data(int mode_number, int mode_state, string &encoded_request)
{
    printf("Current Device Mode number is %d " ,mode_number);
    printf("Current Device Mode state is %d " ,mode_state);
    sns_device_mode_event endoed_event_msg;
    sns_device_mode_event_mode_spec *mode_spec = endoed_event_msg.add_device_mode();
    if(NULL == mode_spec){
      return false;
    }
    mode_spec->set_mode((sns_device_mode)mode_number);
    mode_spec->set_state((sns_device_state)mode_state);
    endoed_event_msg.SerializeToString(&encoded_request);
    return true;
}

/****
    * @brief close remote handle
    */
void frpc_deinit()
{
    if (0 != remote_handle64_close(remote_handle_fd)) {
     printf("remote_handle64_close with %" PRIu64 " failed", remote_handle_fd);
    }
}

/*=============================================================================
    Main Test function
=============================================================================*/
int main()
{
    /*steps to implement device mode feature
    ----> Open fastrpc handle
    ----> Open sns_device_mode handle
    ----> Encode request
    ----> Send device mode and state to DSP in pb-encoded format
    ----> Releasing all the allocated resources*/
    uint32 result = 0;
    string encoded_request = "" ;

    frpc_init();
    sns_device_mode_handle_open();
    if(false == is_device_mode_handle_opened) {
      printf("sendDeviceModeIndication Error  sns_device_mode_init failed");
      return -1;
    }

    /*Refer sns_device_mode.proto to get device mode number and device mode state */
    if(false == get_encoded_data((int)DEVICE_MODE_NUMBER,(int)DEVICE_MODE_NUMBER_STATE, encoded_request)){
      printf("sendDeviceModeIndication Error while encoding data ");
      return -1;
    }

    sns_device_mode_set(device_mode_remote_handle, (const char*)encoded_request.c_str(), encoded_request.length(), &result);
    if(true == is_device_mode_handle_opened) {
      sns_device_mode_close(device_mode_remote_handle);
      is_device_mode_handle_opened = false;
    }
    frpc_deinit();
    return 0;
}