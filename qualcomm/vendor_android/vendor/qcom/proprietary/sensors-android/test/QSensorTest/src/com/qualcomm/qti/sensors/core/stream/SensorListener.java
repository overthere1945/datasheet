/*============================================================================
@file SensorListener.java

@brief
SensorEvent listener.  Updates the associated QSensor upon event receipt.

Copyright (c) 2013-2016, 2022 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
============================================================================*/
package com.qualcomm.qti.sensors.core.stream;

import android.annotation.SuppressLint;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.TriggerEvent;
import android.hardware.SensorEventListener2;
import android.widget.Toast;
import android.util.Log;

import com.qualcomm.qti.sensors.core.qsensortest.QSensorContext;
import com.qualcomm.qti.sensors.core.qsensortest.QSensorEventListener;

@SuppressLint("NewApi")
public class SensorListener extends QSensorEventListener implements SensorEventListener, SensorEventListener2 {
  protected SensorAdapter sensorAdapter;

  public SensorListener(SensorAdapter sensorAdapter) {
    this.sensorAdapter = sensorAdapter;
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy) {
    if(!QSensorContext.optimizePower)
      this.sensorAdapter.accuracyIs(accuracy);
  }

  @Override
  public void onSensorChanged(SensorEvent event) {
    if(!QSensorContext.optimizePower)
      this.sensorAdapter.eventIs(new SensorSample(event, this.sensorAdapter));
  }

  @Override
  public void onTrigger(TriggerEvent event) {
      this.sensorAdapter.eventIs(new SensorSample(event));
  }

  @Override
  public void onFlushCompleted(Sensor sensor) {
    Toast.makeText(QSensorContext.getContext(), "Flush complete for:\n" + this.sensorAdapter.sensor(
).getName(),
    Toast.LENGTH_SHORT).show();
    Log.d(QSensorContext.TAG, "Flush complete event received for: " + this.sensorAdapter.sensor().getName());
  }
}
