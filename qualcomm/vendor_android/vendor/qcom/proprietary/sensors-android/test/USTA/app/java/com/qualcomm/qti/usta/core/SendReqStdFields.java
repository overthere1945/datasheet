/*============================================================================
  Copyright (c) 2017-2018,2021  Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file SendReqStdFields.java
  @brief
   SendReqStdFields is mapped to native data structure.
============================================================================*/
package com.qualcomm.qti.usta.core;

public class SendReqStdFields {
  String batchPeriod;
  String flushPeriod;
  String clientType;
  String wakeupType;
  String flushOnly;
  String maxBatch;
  String isPassive;
  String resamplerType;
  String resamplerFilter;
  String thresholdType;
  String thresholdValueX;
  String thresholdValueY;
  String thresholdValueZ;

  public void setMaxBatch(String maxBatch) {
    this.maxBatch = maxBatch;
  }

  public void setFlushOnly(String flushOnly) {
    this.flushOnly = flushOnly;
  }

  public void setBatchPeriod(String batchPeriod) {
    this.batchPeriod = batchPeriod;
  }

  public void setFlushPeriod(String flushPeriod) {
    this.flushPeriod = flushPeriod;
  }

  public void setClientType(String clientType) {
    this.clientType = clientType;
  }

  public void setWakeupType(String wakeupType) {
    this.wakeupType = wakeupType;
  }
  public String getWakeupType() {
    return this.wakeupType;
  }
  public void setPassive(String isPassive) {
    this.isPassive = isPassive;
  }

  public String getResamplerType() {
    return resamplerType;
  }

  public void setResamplerType(String resamplerType) {
    this.resamplerType = resamplerType;
  }

  public String getResamplerFilter() {
    return resamplerFilter;
  }

  public void setResamplerFilter(String resamplerFilter) {
    this.resamplerFilter = resamplerFilter;
  }

  public String getThresholdType() {
    return thresholdType;
  }

  public void setThresholdType(String thresholdType) {
    this.thresholdType = thresholdType;
  }

  public String getThresholdValueX() {
    return thresholdValueX;
  }

  public void setThresholdValueX(String thresholdValueX) {
    this.thresholdValueX = thresholdValueX;
  }

  public String getThresholdValueY() {
    return thresholdValueY;
  }

  public void setThresholdValueY(String thresholdValueY) {
    this.thresholdValueY = thresholdValueY;
  }

  public String getThresholdValueZ() {
    return thresholdValueZ;
  }

  public void setThresholdValueZ(String thresholdValueZ) {
    this.thresholdValueZ = thresholdValueZ;
  }

}
