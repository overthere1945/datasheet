/*============================================================================
@file CommandReceiver.java

@brief
Receives and processes broadcast commands.

Copyright (c) 2012-2021, 2022 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
============================================================================*/
package com.qualcomm.qti.sensors.core.qsensortest;

import java.util.List;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.util.Log;
import android.widget.Toast;

import com.qualcomm.qti.sensors.core.stream.SensorAdapter;

public class CommandReceiver extends BroadcastReceiver {
  public CommandReceiver() {}

  private static final String TAG = QSensorContext.TAG;

  @Override
  public void onReceive(Context context, Intent intent) {
    if(intent.getAction().contentEquals("com.qualcomm.qti.sensors.qsensortest.intent.STREAM")) {
      String sensorName = intent.getStringExtra("sensorname");
      int sensorType = intent.getIntExtra("sensortype", 0);
      int rate = intent.getIntExtra("rate", 60);
      int batchRate = intent.getIntExtra("batch", -1);
      boolean isSensorFound = false;

      if( (rate>=-1 && rate<=3) ){
        rate = rate;
      }else{
        /* Convert rate from millisec to microsec */
        rate = rate * 1000;
      }
      Log.i(TAG, "received stream intent with parameters: (sensorname: " + sensorName + ", sensortype: " + sensorType + ", rate: " + rate + " batch: " + batchRate + ")");

      List<SensorAdapter> sensorAdapters = QSensorContext.sensorAdapterList();
      for(SensorAdapter adapter : sensorAdapters) {
        Sensor sensor = adapter.sensor();
        if(sensor.getName().replace(" ","_").contentEquals(sensorName) &&
           sensor.getType() == sensorType &&
           SettingsDatabase.getSettings().getSensorSetting(sensor).getEnableStreaming()) {
          isSensorFound = true;
          Log.d(QSensorContext.TAG, "CR setting stream rate for " + sensor.getName() + ": " + rate + ", batch:" + batchRate);
          adapter.streamRateIs(rate, batchRate, true);
          break;
        }
      }
      if (isSensorFound == false) {
          Log.e(QSensorContext.TAG, "improper command for STREAM, Mismatch of sensorname or sensortype , please check name and type in available sensor list ");
      }
    }
    if(intent.getAction().contentEquals("com.qualcomm.qti.sensors.qsensortest.intent.STREAM_ALL")) {
      int rate = intent.getIntExtra("rate", SensorManager.SENSOR_DELAY_NORMAL);
      int batchRate = intent.getIntExtra("batch", -1);
      /* prevent the case where unsupported rate is set by command line */
      if(rate != -1) {
        if(rate < SensorManager.SENSOR_DELAY_FASTEST || rate > SensorManager.SENSOR_DELAY_NORMAL) {
          Log.i(TAG, "rate: " + rate  + " is not expected value set it to NORMAL rate");
          rate = SensorManager.SENSOR_DELAY_NORMAL;
        }
      }
      if (rate < SensorManager.SENSOR_DELAY_GAME) {
          Log.i(TAG, "adjust rate from " + rate + " to GAME");
          rate = SensorManager.SENSOR_DELAY_GAME;
      }
      Log.i(TAG, "received stream all intent with parameters: , rate: " + rate + " batch: " + batchRate + ")");

      List<SensorAdapter> sensorAdapters = QSensorContext.sensorAdapterList();
      for(SensorAdapter adapter : sensorAdapters) {
          Sensor sensor = adapter.sensor();
          int sensorType = sensor.getType();
          boolean isWakeUpSensor = sensor.isWakeUpSensor();
          if (sensorType >= 268369920 || isWakeUpSensor == false) {
              Log.i(TAG, "skip sensorType: " + sensorType + " WakeUp: " + isWakeUpSensor);
          } else {
              Log.i(TAG, "set rate: " + rate + " for sensorType: " + sensorType + " sensorStringType: " + sensor.getStringType());
              adapter.streamRateIs(rate, batchRate, true);
          }
      }
    }
    if (intent.getAction().contentEquals("com.qualcomm.qti.sensors.qsensortest.intent.SENSORLIST")) {
        List<SensorAdapter> sensorAdapters = QSensorContext.sensorAdapterList();
        for (SensorAdapter adapter : sensorAdapters) {
            Sensor sensor = adapter.sensor();
            Log.d(QSensorContext.TAG,"< "+sensor.getStringType()+" > " +" Sensor type:" + "\"" +sensor.getType() + "\"" + " SensorName:" + "\""+sensor.getName()+"\"" );
        }
    }
  }

  public IntentFilter getIntentFilter() {
    IntentFilter intentFilter = new IntentFilter();
    intentFilter.addAction("com.qualcomm.qti.sensors.qsensortest.intent.STREAM");
    Log.d(QSensorContext.TAG, "Available: com.qualcomm.qti.sensors.qsensortest.intent.STREAM");
    intentFilter.addAction("com.qualcomm.qti.sensors.qsensortest.intent.STREAM_ALL");
    Log.d(QSensorContext.TAG, "Available: com.qualcomm.qti.sensors.qsensortest.intent.STREAM_ALL");
    intentFilter.addAction("com.qualcomm.qti.sensors.qsensortest.intent.SENSORLIST");
    Log.d(QSensorContext.TAG,"Available: com.qualcomm.qti.sensors.qsensortest.intent.SENSORLIST");
    intentFilter.addCategory("com.qualcomm.qti.sensors.qsensortest.intent.category.DEFAULT");
    return intentFilter;
  }
}
