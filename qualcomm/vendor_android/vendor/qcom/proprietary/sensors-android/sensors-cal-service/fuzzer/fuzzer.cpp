/*!
 *
 * Copyright (c) 2024 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */
#include <fuzzbinder/libbinder_ndk_driver.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <log/log.h>

#include "SensorsCalibrate.h"

using aidl::vendor::qti::hardware::sensorscalibrate::SensorsCalibrate;

std::shared_ptr<SensorsCalibrate> service;

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    service = ndk::SharedRefBase::make<SensorsCalibrate>();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    android::fuzzService(service->asBinder().get(), FuzzedDataProvider(data, size));
    return 0;
}

