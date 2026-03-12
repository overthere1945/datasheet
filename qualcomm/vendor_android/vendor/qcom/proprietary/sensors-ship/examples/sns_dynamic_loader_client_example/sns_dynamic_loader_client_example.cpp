/******************************************************************************
* File: sns_dynamic_loader_client_example.cpp
*
* Copyright (c) 2023 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
******************************************************************************/

#include "AEEStdErr.h"
#include "sns_dynamic_loader.h"
#include "rpcmem.h"
#include <sys/stat.h>
#ifdef __ANDROID_API__
#include <utils/Log.h>
#else
#include <log.h>
#endif
#include <string>

#ifdef USE_GLIB
#include <glib.h>
#define strlcpy g_strlcpy
#endif

using namespace std;
/*============================
 * static data
 * ==========================*/
static remote_handle64 remote_handle_fd = 0;
static remote_handle64 dynamic_loader_handle = -1;

/*============================
 * Macro definition
 * ==========================*/
#define LIB_NAME "sns_geomag_rv.so"
#define REG_FUNC "sns_geomag_rv_register"


/*============================
 * Helper functions
 * ==========================*/


/**
 * @brief    Open a remote fRPC handle in the specified domain
 */
void fastRPC_remote_handle_init(){
  printf("get_fastRPC_remote_handle Start\n");

  if (remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=adsp", &remote_handle_fd)) {
    printf("failed to open remote handle for sensorspd - adsp \n");
  }

  printf("get_fastRPC_remote_handle End\n");
}


/**
 * @brief    Opens a new dynamic loader handle. If this is the first handle, it creates the session as well.
 * @return   handle for communication
 */
static remote_handle64 dynamic_loader_open(){
  int nErr = AEE_SUCCESS;
  char *uri = sns_dynamic_loader_URI "&_dom=adsp";
  remote_handle64 handle_l;

  if (AEE_SUCCESS == (nErr = sns_dynamic_loader_open(uri, &handle_l))) {
    printf("sns_dynamic_loader_open success for sensorspd - handle_l is %u\n" , (unsigned int)handle_l);
  } else {
    printf("sns_dynamic_loader_open failed for sensorspd - handle_l is %u\n" , (unsigned int)handle_l);
  }
  printf("test_dynamic_loader_open() End \n");

  return handle_l;
}


/**
 * @brief    loads the specified dynamic library on DSP and registers it as a QSH sensor with the details provided
 * @param    handle_l :  handle opened for communication with DSP
 * @param    library : name of the library. Ex:-  sns_geomag_rv.so, sns_geomag_rv_encode.so, etc.
 * @param    reg_func : name of the sensor register function.  Ex:-  sns_geomag_rv_register
 */
void dynamic_load_sensor(remote_handle64 handle_l, string library, string reg_func)
{
  int nErr = AEE_SUCCESS;
  struct sns_dl_entry dl_entry;

  strlcpy(dl_entry.load_config.lib_name, library.c_str(), sizeof(dl_entry.load_config.lib_name));
  dl_entry.load_config.lib_buffer = NULL;
  dl_entry.load_config.lib_bufferLen = 0;
  dl_entry.has_reg_config = 1;      //hard-coded value, can be changed according to use-case
  dl_entry.reg_config.count = 1;    //hard-coded value, can be changed according to use-case
  strlcpy(dl_entry.reg_config.func, reg_func.c_str(), sizeof(dl_entry.reg_config.func));

  printf("Requesting load_library for %s\n", dl_entry.load_config.lib_name);
  if( AEE_SUCCESS != (nErr = sns_dynamic_loader_load_sensor(handle_l, &dl_entry)))
  {
    printf("Requesting load_library FAILED for %s, RC 0x%x, handle_l %lu\n", dl_entry.load_config.lib_name, nErr, handle_l);
  }

  rpcmem_free(dl_entry.load_config.lib_buffer);
}


/**
 * @brief    loads the specified dynamic library on DSP.
 * @param    handle_l :  handle opened for communication with DSP
 * @param    library : name of the library to be loaded.  Ex:-  sns_geomag_rv.so, sns_geomag_rv_encode.so, etc.
 */
void dynamic_load_library(remote_handle64 handle_l, string library)
{
  int nErr = AEE_SUCCESS;
  struct load_library_config load_config;

  strlcpy(load_config.lib_name, library.c_str(), sizeof(load_config.lib_name));
  load_config.lib_buffer = NULL;
  load_config.lib_bufferLen = 0;

  printf("Requesting load_library for %s\n", load_config.lib_name);
  if( AEE_SUCCESS != (nErr = sns_dynamic_loader_load_library(handle_l, &load_config)))
  {
    printf("Requesting load_library FAILED for %s, RC 0x%x, handle_l %lu\n", load_config.lib_name, nErr, handle_l);
  }

  rpcmem_free(load_config.lib_buffer);
}


/**
 * @brief   unloads the dynamic library loaded on DSP. If a sensor is unloaded, any active datastreams to that sensor will
 *          get the sensor invalid state error.
 * @param   handle_l :  handle opened for communication with DSP
 * @param   library : name of the library to be unloaded
 */
void dynamic_unload(remote_handle64 handle_l, string library)
{
  int nErr;
  struct sns_dl_handle dl_handle;
  strlcpy(dl_handle.lib_name, library.c_str(), sizeof(dl_handle.lib_name));

  printf("Requesting unload_library for %s\n", dl_handle.lib_name);
  if( AEE_SUCCESS != (nErr = sns_dynamic_loader_unload(handle_l, &dl_handle)))
  {
    printf("Requesting load_library FAILED for %s, RC 0x%x\n", dl_handle.lib_name, nErr);
  }
}


/**
 * @brief   Closes the handle. If this is the last handle to close, the session is closed as well.
 * @param   dynamic_loader_handle :  handle opened for communication with DSP
 */
void dynamic_loader_close(remote_handle64 handle_l)
{
  int nErr = AEE_SUCCESS;

  ALOGE("-------------------\n  closing sns_dynamic_loader \n ---------------------");
  if (AEE_SUCCESS != (nErr = sns_dynamic_loader_close(handle_l))) {
    ALOGE("ERROR 0x%x: Failed to close handle\n", nErr);
  }

  if(0 != remote_handle_fd)
    remote_handle64_close(remote_handle_fd);

  return;
}


int main(int argc, char **argv)
{
  /* ==============================
   * This is an example code and may be used as a reference for using the
   * fRPC APIs to load/unload dynamic libraries and sensors present at DSP side.
   *
   * For being able to load libraries and sensors dynamically,
   * create a fRPC handle ->  create dynamic loader handle  ->  perform your operation  ->
   * close the dynamic loader handle  ->  close fRPC handle
   * ==============================*/

  // initialize fRPC handle
  fastRPC_remote_handle_init();

  // Init sns_dynamic_loader handle
  dynamic_loader_handle = dynamic_loader_open();

  // load dynamic library
  dynamic_load_library(dynamic_loader_handle, LIB_NAME);

  // load sensor
  dynamic_load_sensor(dynamic_loader_handle, LIB_NAME, REG_FUNC);

  // unload library
  dynamic_unload(dynamic_loader_handle, LIB_NAME);

  // close sns_dynamic_loader handle
  dynamic_loader_close(dynamic_loader_handle);
  return 0;
}
