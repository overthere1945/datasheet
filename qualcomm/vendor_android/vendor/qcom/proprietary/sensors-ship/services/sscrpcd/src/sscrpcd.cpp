/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc
 *
 */
#define LOG_TAG "sscrpcd"
#ifndef VERIFY_PRINT_ERROR
#define VERIFY_PRINT_ERROR
#endif
#define VERIFY_PRINT_INFO 0

#include <stdio.h>
#ifdef __ANDROID_API__
#include <utils/Log.h>
#endif
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <remote.h>
#include <AEEStdErr.h>
#include <verify.h>
#ifdef __ANDROID_API__
#include <dirent.h>
#include <stdlib.h>
#endif

#ifndef ADSP_LIBHIDL_NAME
#ifdef SNS_VERSIONED_LIB_ENABLED
#define ADSP_LIBHIDL_NAME "libhidlbase.so.1"
#else
#define ADSP_LIBHIDL_NAME "libhidlbase.so"
#endif
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif // __clang__

typedef int (*adsp_default_listener_start_t)(int argc, char *argv[]);
typedef int (*remote_handle_control_t)(uint32_t req, void* data, uint32_t len);
#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
typedef int (*remote_system_request_t)(system_req_payload *req);
#endif

const char *get_lib() {
#ifdef SNS_VERSIONED_LIB_ENABLED
    return "libadsp_default_listener.so.1";
#else
    return "libadsp_default_listener.so";
#endif
}

int request_fastrpc_wakelock(void *adsphandler) {
  int nErr = 0;
#ifdef FASTRPC_WAKELOCK_CONTROL_SUPPORTED
  remote_handle_control_t handle_control;
  struct remote_rpc_control_wakelock data;

  data.enable = 1;

  if (NULL != (handle_control = (remote_handle_control_t)dlsym(adsphandler, "remote_handle_control"))) {
    nErr = handle_control(DSPRPC_CONTROL_WAKELOCK, (void*)&data, sizeof(data));
    if (nErr == AEE_EUNSUPPORTEDAPI) {
#ifdef __ANDROID_API__
      ALOGE("fastrpc wakelock request is not supported");
#endif
      /* this feature may not be supported by all targets
         treat this case as normal since we still can call listener_start */
      nErr = AEE_SUCCESS;
    } else if (nErr) {
#ifdef __ANDROID_API__
      ALOGE("failed to enable fastrpc wake-lock control, %x", nErr);
#endif
    }
  } else {
#ifdef __ANDROID_API__
    ALOGE("unable to find remote_handle_control, %s", dlerror());
#endif
    /* there should be no case where remote_handle_control doesn't exist */
    nErr = AEE_EFAILED;
  }
#endif
  return nErr;
}
#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
static inline bool is_dynamic_domain_system() {
  DIR *dir = opendir("/sys/kernel/fastrpc");
  if(dir) {
    closedir(dir);
    return true;
  }
  return false;
}
#endif

#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
void is_domain_available(remote_system_request_t remote_system_request, void* &adsphandler){
  if(NULL != (adsphandler = dlopen(get_lib(), RTLD_NOW))){
    if(NULL != (remote_system_request =
      (remote_system_request_t)dlsym(adsphandler, "remote_system_request")) && is_dynamic_domain_system()) {
      system_req_payload req;
      fastrpc_domain *domain = NULL;
      fastrpc_domains_info *sys = &req.sys;

      req.id = FASTRPC_GET_DOMAINS;
  query_num_domains:
      int nErr = remote_system_request(&req);
      if (nErr == AEE_EUNSUPPORTED) {
        /*
        * Dynamic domain discovery not supported, dlclose the current adsphandler.
        * and go to default_listener_start directly.
        */
#ifdef __ANDROID_API__
        ALOGE("Failed to get domains num");
#endif
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return;
      }
      int num_domains = sys->num_domains;
      if (!num_domains) {
#ifdef __ANDROID_API__
        ALOGE("No domains available to attach. Retry query after 25 ms...");
#endif
        usleep(25000);
        goto query_num_domains;
      }
      /* Allocate memory for domain info structure */
      sys->domains = nullptr;
      sys->domains = (fastrpc_domain*)calloc(num_domains, sizeof(fastrpc_domain));
      if(nullptr == sys->domains) {
#ifdef __ANDROID_API__
        ALOGE("Failed to allocate memory for domains");
#endif
        return;
      }
      sys->max_domains = num_domains;
      sys->flags = DOMAINS_LIST_FLAGS_SET_TYPE(sys->flags, FASTRPC_LPASS);
  query_domains:
      nErr = remote_system_request(&req);
      if (nErr) {
        /*
        * If failed to get domain info, free the previous allocated domain array
        * and requery the num domains available and try to get the domain info again
        */
#ifdef __ANDROID_API__
        ALOGE("Failed to get domains info");
#endif
        sys->num_domains = 0;
        free(sys->domains);
        goto query_num_domains;
      }
      domain = sys->domains;
      if (domain->type == FASTRPC_LPASS && domain->status) {
        free(sys->domains);
        if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
          ALOGE("dlclose failed");
#endif
        }
        return;
      } else {
        memset(sys->domains, 0, sizeof(fastrpc_domain) * num_domains);
#ifdef __ANDROID_API__
        ALOGE("Domain type %d is not up, retry query after 25ms...", FASTRPC_LPASS);
#endif
        usleep(25000);
        goto query_domains;
      }
    }else {
#ifdef __ANDROID_API__
      ALOGE("no remote_system_request symbol, error %s", dlerror());
#endif
      if(NULL != adsphandler && 0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
        ALOGE("dlclose failed");
#endif
      }
    }
  }else{
#ifdef __ANDROID_API__
    ALOGE("fail to open %s due to %s", get_lib(), dlerror());
#endif
  }
}
#endif

int main(int argc, char *argv[]) {

  int nErr = 0;
  void *adsphandler = NULL, *libhidlbaseHandler = NULL;
  adsp_default_listener_start_t listener_start;
#ifdef SNS_FRPC_DYNAMIC_PD_SUPPORTED
  remote_system_request_t remote_system_request;

  is_domain_available(remote_system_request, adsphandler);
#endif
#ifdef __ANDROID_API__
  libhidlbaseHandler = dlopen(ADSP_LIBHIDL_NAME, RTLD_NOW);
  if (libhidlbaseHandler == NULL)
  {
    ALOGE("libhidlbase dlopen failed");
    return 0;
  }
#endif

#ifdef __ANDROID_API__
  ALOGI("sscrpcd daemon starting for %s", argv[1]);
#endif
  while (1) {
    if(NULL != (adsphandler = dlopen(get_lib(), RTLD_NOW))) {
      if(NULL != (listener_start =
        (adsp_default_listener_start_t)dlsym(adsphandler, "adsp_default_listener_start"))) {
        nErr = request_fastrpc_wakelock(adsphandler);
        if(nErr){
#ifdef __ANDROID_API__
          ALOGE("request_fastrpc_wakelock failed with, %x", nErr);
#endif
        }
        listener_start(argc, argv);
#ifdef __ANDROID_API__
        ALOGE("listener_start exit");
#endif
      }
      if(0 != dlclose(adsphandler)) {
#ifdef __ANDROID_API__
        ALOGE("dlclose failed");
#endif
      }
    }else {
#ifdef __ANDROID_API__
      ALOGE("fail to open %s due to %s", get_lib(), dlerror());
#endif
    }
    usleep(25000);
  }
  if(0 != dlclose(libhidlbaseHandler)) {
#ifdef __ANDROID_API__
    ALOGE("libhidlbase dlclose failed");
#endif
  }
#ifdef __ANDROID_API__
  ALOGI("sscrpcd daemon exiting %x", nErr);
#endif

  return nErr;
}
