/*============================================================================
  Copyright (c) 2017-2021, 2023-2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file USTACmdLineReceiver.java
  @brief
   USTACmdLineReceiver class implementation for command line services extends BroadcastReceiver
============================================================================*/

package com.qualcomm.qti.usta.core;

import android.os.Bundle;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import java.math.BigInteger;
import com.qualcomm.qti.usta.ui.SensorPayloadFragment;

import java.util.HashMap;
import java.util.Map;
import java.util.Vector;


public class USTACmdLineReceiver extends BroadcastReceiver {

  private final SensorContext sensorContext;

  private final String DELIMITERCOLON = ":";
  private final String DELIMITERSEMICOLON = ";";
  private final String RIGHTBRACE = "}";
  private final String LEFTBRACE = "{";
  private final String NAME = "Name";
  private final String DATATYPE = "DataType";
  private final String ENUMVALUES = "EnumVaules";
  private final String COMMA = ",";
  private final static int MAX_ADB_LOGCAT_LENGTH = 4000;
  private long[] sensorSuid;
  private int[] channelDb;
  private final int MAX_CHANNELS = 32;
  private static final int SNS_LOW_LAT_CHANNEL_TYPE_NOT_INIT = -1;
  private static final int SNS_LOW_LAT_CHANNEL_TYPE_ANDROID = 0x0;
  private static final int SNS_LOW_LAT_CHANNEL_TYPE_GENERIC = 0x1;
  private static final int SNS_LOW_LAT_FLAG_ANDROID_STYLE_CHANNEL = 0x2;
  private Map sensorReqHandleMapTable;
  private Map sensorClientConnectIDMapTable;
  private Map reqHandleTablePower;
  private Map clientIDTablePower;

  public USTACmdLineReceiver() {
    sensorReqHandleMapTable = new HashMap();
    sensorClientConnectIDMapTable = new HashMap();
    reqHandleTablePower = new HashMap();
    clientIDTablePower = new HashMap();
    sensorContext = USTACmdLineService.getSensorContext();
  }

  @Override
  public void onReceive(Context context, Intent intent) {

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.GETSENSORLIST")) {
      USTALog.i("GETSENSORLIST intent");
      dumpSensorList();
    }

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.DISABLELOGGING")) {
      USTALog.i("DISABLELOGGING intent");
      disableLogging(intent);
    }


    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.GETSENSORATTRIBUTES")) {

      USTALog.i("GETSENSORATTRIBUTES intent");
      dumpSensorAttribList(intent);
    }

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.GETREQUESTMSGS")) {

      USTALog.i("GETREQUESTMSGS intent");
      dumpReqMsgList(intent);
    }

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.ENABLE")) {

      USTALog.i("ENABLE intent");
      sendReqMsg(intent);
    }

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.DISABLE")) {

      USTALog.i("DISABLE intent");
      stopReqMsg(intent);
    }

    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.POWERAUTOMATION_ENABLE")) {
      USTALog.i("POWERAUTOMATION_ENABLE intent");
      powerAutomation(intent);
    }
    if (intent.getAction().contentEquals("com.qualcomm.qti.usta.core.intent.POWERAUTOMATION_DISABLE")) {
      USTALog.i("POWERAUTOMATION_DISABLE intent");
      powerAutomationDisable(intent);
    }
  }

  private void powerAutomationDisable(Intent intent){
    String scriptFileName = intent.getStringExtra("script_name");
    if(scriptFileName == null || scriptFileName.length() == 0){
      USTALog.e("Invalid Argument from command line");
      return;
    }
    USTALog.d("powerAutomationDisable , input string is " + scriptFileName);

    Vector<String> suid = sensorContext.getScriptUtil().getSUID(scriptFileName);
    if(suid == null || suid.size() == 0 ){
      USTALog.e("Error in getting suid info");
      return;
    }

    int sensorHandle = sensorContext.getSensorHandleFromSUID(suid.get(0), suid.get(1));
    if(sensorHandle == -1 ) {
      USTALog.e("Sensor handle not found. Please check the SUID info passed here");
      return;
    }

    int reqHandle = -1;
    if(reqHandleTablePower.containsKey(sensorHandle) == true ) {
      USTALog.i("reqHandle present for this sensor");
      reqHandle = (int) reqHandleTablePower.get(sensorHandle);
      if(clientIDTablePower.containsKey(sensorHandle) == true)
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(reqHandle).setClientConnectID((Integer) clientIDTablePower.get(sensorHandle));
      else {
        USTALog.i("clientConnectID NOT present for this sensor");
        return;
      }

    } else {
      USTALog.i("reqHandle NOT present for this sensor");
      return;
    }
    USTALog.d("stop request from command line for sensor " + Integer.toString(sensorHandle) + " with req handle " + reqHandle);

    int clientConnectID = sensorContext.sendRequest(sensorHandle, reqHandle, null, null, true);
    USTALog.i("power automation, stop done for sensorhandle " + sensorHandle + " with client ID as " + clientConnectID);
    if(clientConnectID != -1) {
      if(clientIDTablePower.containsKey(sensorHandle) == true) {
        clientIDTablePower.remove(sensorHandle);
      }
      if(reqHandle != -1)
        reqHandleTablePower.remove(sensorHandle);
      USTALog.i("power automation, Updating client connect id in the static fields as -1");
      sensorContext.getSensors().elementAt(sensorHandle).getSensorStdFields(reqHandle).setClientConnectID(-1);
    }
  }

  private void powerAutomation(Intent intent){
    String scriptFileName = intent.getStringExtra("script_name");
    if(scriptFileName == null || scriptFileName.length() == 0){
      USTALog.e("Invalid Argument from command line");
      return;
    }
    USTALog.d("powerAutomation , input string is " + scriptFileName);
    Vector<String> suid = sensorContext.getScriptUtil().getSUID(scriptFileName);
    if(suid == null || suid.size() == 0 ){
      USTALog.e("Error in getting suid info");
      return;
    }
    int sensorHandle = sensorContext.getSensorHandleFromSUID(suid.get(0), suid.get(1));
    if(sensorHandle == -1 ) {
      USTALog.e("Sensor handle not found. Please check the SUID info passed here");
      return;
    }

    String absoluteScriptPath =  "/data/vendor/sensors/scripts" + "/" + scriptFileName;
    Map scriptDataTable = sensorContext.getScriptUtil().parseScriptFile(absoluteScriptPath);

    parseScriptData(sensorHandle , scriptDataTable);
  }

  private void parseScriptData(int sensorHandle , Map scriptDataTable){
    if(scriptDataTable == null )
      return;
    SendRequestScriptBased(sensorHandle , scriptDataTable);
  }

  private int getOrdinalNumberFromEnumString(Vector<String> stringList , String currentString) {
    if(stringList == null || stringList.size() == 0) {
      return -1;
    }
    for (int index = 0 ; index < stringList.size() ; index++){
      if(stringList.elementAt(index).equals(currentString)) {
        return index;
      }
    }
    return -1;
  }

  private boolean SendRequestScriptBased(int sensorHandle , Map scriptDataTable) {

    Vector<String> req_MsgID = (Vector<String>) scriptDataTable.get("Req_Msgs_ID");
    if (req_MsgID == null || req_MsgID.elementAt(0) == null || req_MsgID.elementAt(0).length() == 0 || req_MsgID.elementAt(0).equals("null")) {
      USTALog.e("Error in parsing req_MsgID");
      return false;
    }
    int reqMsgID = Integer.parseInt(req_MsgID.elementAt(0));
    if (reqMsgID == -1) {
      USTALog.e("Invalid reqMsgID from the Script file");
      return false;
    }

    int clientproc;
    Vector<String> client_processor = (Vector<String>) scriptDataTable.get("Client_Processor");
    if (client_processor == null || client_processor.elementAt(0) == null || client_processor.elementAt(0).length() == 0 || client_processor.elementAt(0).equals("null")) {
      clientproc = 0; // Assign default value
    } else {
      clientproc = getOrdinalNumberFromEnumString(sensorContext.getClientProcessors() , client_processor.elementAt(0).toLowerCase());
      if(clientproc == -1)
        clientproc = 0;
    }

    int wakeupType;
    Vector<String> wakeup_delivery = (Vector<String>) scriptDataTable.get("wakeup_delivery");
    if (wakeup_delivery == null || wakeup_delivery.elementAt(0) == null || wakeup_delivery.elementAt(0).length() == 0 || wakeup_delivery.elementAt(0).equals("null")) {
      wakeupType = 0; // Assign default value
    } else {
      wakeupType = getOrdinalNumberFromEnumString(sensorContext.getWakeupDelivery() , wakeup_delivery.elementAt(0).toLowerCase());
      if(wakeupType == -1)
        wakeupType = 0;
    }

    int batchperiod;
    Vector<String> batch_period = (Vector<String>) scriptDataTable.get("batch_period");
    if (batch_period == null || batch_period.elementAt(0) == null || batch_period.elementAt(0).length() == 0 || batch_period.elementAt(0).equals("null")) {
      batchperiod = -1; // Assign default value
    } else {
      batchperiod = Integer.parseInt(batch_period.elementAt(0));
    }

    int flushperiod;
    Vector<String> flush_period = (Vector<String>) scriptDataTable.get("flush_period");
    if (flush_period == null || flush_period.elementAt(0) == null || flush_period.elementAt(0).length() == 0 || flush_period.elementAt(0).equals("null")) {
      flushperiod = -1; // Assign default value
    } else {
      flushperiod = Integer.parseInt(flush_period.elementAt(0));
    }

    boolean flushOnly;
    Vector<String> flush_only = (Vector<String>) scriptDataTable.get("flush_only");
    if (flush_only == null || flush_only.elementAt(0) == null || flush_only.elementAt(0).length() == 0 || flush_only.elementAt(0).equals("null")) {
      flushOnly = false; // Assign default value
    } else {
      flushOnly = true;
    }

    boolean maxBatch;
    Vector<String> max_batch = (Vector<String>) scriptDataTable.get("max_batch");
    if (max_batch == null || max_batch.elementAt(0) == null || max_batch.elementAt(0).length() == 0 || max_batch.elementAt(0).equals("null")) {
      maxBatch = false; // Assign default value
    } else {
      maxBatch = true;
    }

    boolean isPassive;
    Vector<String> is_passive = (Vector<String>) scriptDataTable.get("is_passive");
    if (is_passive == null || is_passive.elementAt(0) == null || is_passive.elementAt(0).length() == 0 || is_passive.elementAt(0).equals("null")) {
      isPassive = false; // Assign default value
    } else {
      isPassive = true;
    }

    Sensor sensorInstance = sensorContext.getSensors().get(sensorHandle);
    if (sensorInstance == null) {
      USTALog.e("Improper Sensor Handle passed. Please check the suid of the script!");
      return false;
    }

    boolean msgIDValid = false;
    int reqMsgHandle = -1;
    for (String msgID : sensorInstance.getSensorReqMsgIDs()) {
      reqMsgHandle++;
      if (msgID.compareTo(Integer.toString(reqMsgID)) == 0) {
        msgIDValid = true;
        break;
      }
    }
    if (!msgIDValid) {
      USTALog.e("Incorrect Request Msg ID passed. ");
      return false;
    }


    if (batchperiod != -1)
      sensorInstance.getSensorStdFields(reqMsgHandle).setBatchPeriod(Integer.toString(batchperiod));
    else
      sensorInstance.getSensorStdFields(reqMsgHandle).setBatchPeriod(null);


    sensorInstance.getSensorStdFields(reqMsgHandle).setFlushOnly(flushOnly);
    sensorInstance.getSensorStdFields(reqMsgHandle).setMaxBatch(maxBatch);
    sensorInstance.getSensorStdFields(reqMsgHandle).setPassive(isPassive);
    sensorInstance.getSensorStdFields(reqMsgHandle).setClientProcessor(clientproc);
    sensorInstance.getSensorStdFields(reqMsgHandle).setWakeupDevilvery(wakeupType);

    if (flushperiod != -1)
      sensorInstance.getSensorStdFields(reqMsgHandle).setFlushPeriod(Integer.toString(flushperiod));
    else
      sensorInstance.getSensorStdFields(reqMsgHandle).setFlushPeriod(null);

    updateSensorPayloadField(sensorInstance.getSensorPayloadField(reqMsgHandle), scriptDataTable);


    int clientConnectID = sensorContext.sendRequest(sensorHandle, reqMsgHandle, null, "", false);
    if(clientConnectID != -1) {
      if(reqHandleTablePower.containsKey(sensorHandle) == false)
        reqHandleTablePower.put(sensorHandle, reqMsgHandle);
      if (clientIDTablePower.containsKey(sensorHandle) == false)
        clientIDTablePower.put(sensorHandle, clientConnectID);
      USTALog.i("power automation, Updating client connect id in the static fields as " + clientConnectID);
      sensorContext.getSensors().elementAt(sensorHandle).getSensorStdFields(reqMsgHandle).setClientConnectID(clientConnectID);
    }

    return true;
  }

  private void disableLogging(Intent intent){
    int disable_logging = intent.getIntExtra("disable_logging", -1);
    if (disable_logging == 0) {
      USTALog.isLoggingDisabled = false;
      SensorContextJNI.updateLoggingFlagWrapper();
    }
    else if (disable_logging == 1) {
      USTALog.isLoggingDisabled = true;
      SensorContextJNI.updateLoggingFlagWrapper();
    }
    else{
      USTALog.e("Wrong value from command line instruction");
    }

  }

  private void updateSensorPayloadField(Vector<SensorPayloadField> payloadFieldVector, Intent intent) {

    for (SensorPayloadField payloadField : payloadFieldVector) {

      DataType dataType = payloadField.getDataType();

      switch (dataType) {
        case ENUM:
        case SIGNED_INTEGER32:
        case UNSIGNED_INTEGER32:
        case SIGNED_INTEGER64:
        case UNSIGNED_INTEGER64:
          String suidLow = "suid_low";
          String suidHigh = "suid_high";
          if (payloadField.getFieldName().equals(suidLow) || payloadField.getFieldName().equals(suidHigh)) {
            String suidPart = intent.getStringExtra(payloadField.getFieldName());
            payloadField.addValue(suidPart);
          } else {
            int intField = intent.getIntExtra(payloadField.getFieldName(), -1);
            if(intField != -1)
              payloadField.addValueToWidgetIDValueTable(Integer.toString(intField), 999);
          }
          break;

        case FLOAT:
          float floatField = intent.getFloatExtra(payloadField.getFieldName(), (float) -1.0);
          if(floatField != -1)
            payloadField.addValueToWidgetIDValueTable(Float.toString(floatField), 999);
          break;

        case BOOLEAN:
          boolean boolField = intent.getBooleanExtra(payloadField.getFieldName(), false);
          payloadField.addValueToWidgetIDValueTable(Boolean.toString(boolField), 999);
          break;

        case USER_DEFINED:
          for (int nestedMsgIndex = 0; nestedMsgIndex < payloadField.getNestedMsgSize(); nestedMsgIndex++) {
            Vector<SensorPayloadField> sensorPayloadFields;
            sensorPayloadFields = new Vector<>();
            for (int payLoadFieldIndex = 0; payLoadFieldIndex < (payloadField.getNestedMsgs())[nestedMsgIndex].getFieldCount(); payLoadFieldIndex++) {
              sensorPayloadFields.add((payloadField.getNestedMsgs())[nestedMsgIndex].getFiledAt(payLoadFieldIndex));
            }
            updateSensorPayloadField(sensorPayloadFields, intent);
          }
          break;

        case STRING:
          String stringField = intent.getStringExtra(payloadField.getFieldName());
          if(stringField != null && stringField.length() != 0)
            payloadField.addValue(stringField);
      }

    }
  }

  private void updateSensorPayloadField(Vector<SensorPayloadField> payloadFieldVector, Map scriptDataTable) {

    for (SensorPayloadField payloadField : payloadFieldVector) {

      DataType dataType = payloadField.getDataType();

      switch (dataType) {
        case ENUM:
        case SIGNED_INTEGER32:
        case UNSIGNED_INTEGER32:
        case SIGNED_INTEGER64:
        case UNSIGNED_INTEGER64:
          Vector<String> intFieldVector = (Vector<String>) scriptDataTable.get(payloadField.getFieldName());
          if (intFieldVector == null || intFieldVector.elementAt(0) == null || intFieldVector.elementAt(0).length() == 0 || intFieldVector.elementAt(0).equals("null")) {
          } else {
            payloadField.addValueToWidgetIDValueTable(intFieldVector.elementAt(0), 999);
          }

        case FLOAT:
          Vector<String> floatFieldVector = (Vector<String>) scriptDataTable.get(payloadField.getFieldName());
          if (floatFieldVector == null || floatFieldVector.elementAt(0) == null || floatFieldVector.elementAt(0).length() == 0 || floatFieldVector.elementAt(0).equals("null")) {
          } else {
            payloadField.addValueToWidgetIDValueTable(floatFieldVector.elementAt(0), 999);
          }
          break;

        case BOOLEAN:
          Vector<String> booleanFieldVector = (Vector<String>) scriptDataTable.get(payloadField.getFieldName());
          if (booleanFieldVector == null || booleanFieldVector.elementAt(0) == null || booleanFieldVector.elementAt(0).length() == 0 || booleanFieldVector.elementAt(0).equals("null")) {
          } else {
            payloadField.addValueToWidgetIDValueTable("true", 999);
          }
          break;
        case USER_DEFINED:
          for (int nestedMsgIndex = 0; nestedMsgIndex < payloadField.getNestedMsgSize(); nestedMsgIndex++) {
            Vector<SensorPayloadField> sensorPayloadFields;
            sensorPayloadFields = new Vector<>();
            for (int payLoadFieldIndex = 0; payLoadFieldIndex < (payloadField.getNestedMsgs())[nestedMsgIndex].getFieldCount(); payLoadFieldIndex++) {
              sensorPayloadFields.add((payloadField.getNestedMsgs())[nestedMsgIndex].getFiledAt(payLoadFieldIndex));
            }
            updateSensorPayloadField(sensorPayloadFields, scriptDataTable);
          }
          break;

        case STRING:

          Vector<String> stringFieldVector = (Vector<String>) scriptDataTable.get(payloadField.getFieldName());
          if (stringFieldVector == null || stringFieldVector.elementAt(0) == null || stringFieldVector.elementAt(0).length() == 0 || stringFieldVector.elementAt(0).equals("null")) {
          } else {
            payloadField.addValue(stringFieldVector.elementAt(0));
          }
          break;
      }
    }
  }
  private void sendReqMsg(Intent intent) {

    int sensorHandle = intent.getIntExtra("sensorhandle", -1);

    if (-1 == sensorHandle) {
      String sensorSUIDLow = intent.getStringExtra("suid_low");
      String sensorSUIDHigh = intent.getStringExtra("suid_high");
      sensorHandle = sensorContext.getSensorHandleFromSUID(sensorSUIDLow, sensorSUIDHigh);
    }
    if (sensorHandle == -1 || sensorHandle >= sensorContext.getSensors().size()) {
      USTALog.e("Improper Sensor SUID is passed. Please check and send valid SUID !");
      return;
    }

    String logFileName = intent.getStringExtra("logfile");

    if (logFileName == null) {
      USTALog.i("LogFileName was not entered by Users. So Taking the default logFileName ");
      logFileName = "";

    }
    int reqMsgID = intent.getIntExtra("msg_id", 0);
    Sensor sensorInstance = sensorContext.getSensors().get(sensorHandle);
    boolean msgIDValid = false;
    int reqMsgHandle = -1;

    if (sensorInstance == null) {

      USTALog.e("Improper Sensor Handle passed. Please check and send valid Handle!");
      return;
    }
    USTALog.d("User MSg ID " + reqMsgID);

    for (String msgID : sensorInstance.getSensorReqMsgIDs()) {

      reqMsgHandle++;
      USTALog.d("current MSg ID " + msgID);
      if (msgID.compareTo(Integer.toString(reqMsgID)) == 0) {
        msgIDValid = true;
        USTALog.d(Integer.toString(reqMsgHandle));
        break;
      }
    }
    if(10 == reqMsgID){
      msgIDValid = true;
      SensorPayloadFragment.isClientDisableRequestEnabled = true;
      int reqHandle = -1;
      if(sensorReqHandleMapTable.containsKey(sensorHandle) == true ) {
        reqHandle = (int) sensorReqHandleMapTable.get(sensorHandle);
      }
      if(reqHandle != -1) {
        if(sensorClientConnectIDMapTable.containsKey(sensorHandle) == true) {
          sensorContext.getSensors().elementAt(sensorHandle).getSensorStdFields(reqHandle).setClientConnectID((int)sensorClientConnectIDMapTable.get(sensorHandle));
        }
      }
      int clientConnectID = sensorContext.sendRequest(sensorHandle, reqHandle, null, null, true);
      if(sensorClientConnectIDMapTable.containsKey(sensorHandle) == true)
        sensorClientConnectIDMapTable.remove(sensorHandle);
      SensorPayloadFragment.isClientDisableRequestEnabled = false;
      if(reqHandle != -1)
        sensorReqHandleMapTable.remove(sensorHandle);
      return;
    }
    if (!msgIDValid) {

      USTALog.e("Incorrect Request Msg ID passed. Please check and send valid Request Msg ID!");

      return;
    }



      int clientproc = intent.getIntExtra("client", 0);
      int wakeupType = intent.getIntExtra("wakeup", 0);
      int batchperiod = intent.getIntExtra("batch_period", -1);
      int flushperiod = intent.getIntExtra("flush_period", -1);
      boolean flushOnly = intent.getBooleanExtra("flush_only", false);
      boolean maxBatch = intent.getBooleanExtra("max_batch", false);
      boolean isPassive = intent.getBooleanExtra("is_passive", false);
      boolean isResample = intent.getBooleanExtra("is_resample", false);
      int resampleType = intent.getIntExtra("resample_type", 0);
      boolean resampleFilter = intent.getBooleanExtra("resample_filter", false);
      boolean isThreshold = intent.getBooleanExtra("is_threshold", false);
      int thresholdType = intent.getIntExtra("threshold_type", 0);
      float thresholdValueX = intent.getFloatExtra("threshold_value_x", 0f);
      float thresholdValueY = intent.getFloatExtra("threshold_value_y", 0f);
      float thresholdValueZ = intent.getFloatExtra("threshold_value_z", 0f);
      int clientConnectId = intent.getIntExtra("clientConnectID", -1);

      if(batchperiod != -1)
        sensorInstance.getSensorStdFields(reqMsgHandle).setBatchPeriod(Integer.toString(batchperiod));
      else
        sensorInstance.getSensorStdFields(reqMsgHandle).setBatchPeriod(null);
      sensorInstance.getSensorStdFields(reqMsgHandle).setFlushOnly(flushOnly);
      sensorInstance.getSensorStdFields(reqMsgHandle).setMaxBatch(maxBatch);
      sensorInstance.getSensorStdFields(reqMsgHandle).setPassive(isPassive);
      sensorInstance.getSensorStdFields(reqMsgHandle).setClientProcessor(clientproc);
      sensorInstance.getSensorStdFields(reqMsgHandle).setWakeupDevilvery(wakeupType);
      if(flushperiod != -1)
        sensorInstance.getSensorStdFields(reqMsgHandle).setFlushPeriod(Integer.toString(flushperiod));
      else
        sensorInstance.getSensorStdFields(reqMsgHandle).setFlushPeriod(null);
      if(isResample == true) {
        sensorInstance.getSensorStdFields(reqMsgHandle).setResampled(true);
        sensorInstance.getSensorStdFields(reqMsgHandle).setResamplerType(resampleType);
        sensorInstance.getSensorStdFields(reqMsgHandle).setResamplerFilter(resampleFilter);
      } else {
        sensorInstance.getSensorStdFields(reqMsgHandle).setResampled(false);
      }
      if(isThreshold == true) {
        sensorInstance.getSensorStdFields(reqMsgHandle).setThresholded(true);
        sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdType(thresholdType);
        if(thresholdValueX != 0)
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueX(Float.toString(thresholdValueX));
        else
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueX(null);
        if(thresholdValueY != 0)
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueY(Float.toString(thresholdValueY));
        else
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueY(null);
        if(thresholdValueZ != 0)
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueZ(Float.toString(thresholdValueZ));
        else
          sensorInstance.getSensorStdFields(reqMsgHandle).setThresholdValueZ(null);
      } else {
        sensorInstance.getSensorStdFields(reqMsgHandle).setThresholded(false);
      }

      if(clientConnectId != -1) {
        sensorInstance.getSensorStdFields(reqMsgHandle).setClientConnectID(clientConnectId);
      }
    String registryName = "registry";
    if(!sensorContext.getSensorNames().get(sensorHandle).contains(registryName)) {
      updateSensorPayloadField(sensorInstance.getSensorPayloadField(reqMsgHandle), intent);
      int clientConnectID = sensorContext.sendRequest(sensorHandle, reqMsgHandle, null, logFileName, false);
      if(sensorReqHandleMapTable.containsKey(sensorHandle) == false)
        sensorReqHandleMapTable.put(sensorHandle, reqMsgHandle);
      if(sensorClientConnectIDMapTable.containsKey(sensorHandle) == false)
        sensorClientConnectIDMapTable.put(sensorHandle, clientConnectID);
    }
    else if(reqMsgID == 512){
      updateSensorPayloadField(sensorInstance.getSensorPayloadField(reqMsgHandle), intent);
      sensorContext.sendRequest(sensorHandle, reqMsgHandle, null, logFileName, false);
    }
    else if(reqMsgID == 513) {
      USTALog.i("registry write case ");
      String subMessageName = intent.getStringExtra("sub_message_name");
      if(subMessageName == null || subMessageName.length() == 0){
        USTALog.e("Incorrect subMessageName is not passed. Please check again and try");
        return;
      }

      String dataName = intent.getStringExtra("data_name");
      if(dataName == null || dataName.length() == 0){
        USTALog.e("Incorrect dataName is not passed. Please check again and try");
        return;
      }

      int version = intent.getIntExtra("version", -1);
      if(version == -1){
        USTALog.e("version number is not supplied. Please supply it to proceed further ");
        return;
      }

      String oneOfFiledName = intent.getStringExtra("oneof_name");
      if(oneOfFiledName == null || oneOfFiledName.length() == 0){
        USTALog.e("oneOfFiledName is not passed. Please check again and try");
        return;
      }
      int position = -1;
      if(oneOfFiledName.equals("sint")){
        position = 2 ;
      }
      else if(oneOfFiledName.equals("flt")){
        position = 1 ;
      }
      else if(oneOfFiledName.equals("str")){
        position = 0 ;
      }

      String oneOfValue = intent.getStringExtra("one_of_value");
      if(subMessageName == null || subMessageName.length() == 0){
        USTALog.e("Incorrect subMessageName is not passed. Please check again and try");
        return;
      }
      sensorContext.RegistryWriteSendRequest(ModeType.USTA_MODE_TYPE_COMMAND_LINE, sensorHandle, subMessageName, dataName, Integer.toString(version), position,oneOfValue, null);
    }
  }

  private void stopReqMsg(Intent intent) {

    int sensorHandle = intent.getIntExtra("sensorhandle", -1);

    if (-1 == sensorHandle) {
      String sensorSUIDLow = intent.getStringExtra("suid_low");
      String sensorSUIDHigh = intent.getStringExtra("suid_high");
      sensorHandle = sensorContext.getSensorHandleFromSUID(sensorSUIDLow, sensorSUIDHigh);
    }

    if (sensorHandle == -1) {
      USTALog.e("Improper Sensor SUID is passed. Please check and send valid SUID !");

      return;
    }

    USTALog.d("stop request from command line for sensor " + Integer.toString(sensorHandle));
    Sensor sensorInstance = sensorContext.getSensors().get(sensorHandle);
    if (sensorInstance == null) {
      USTALog.e("Improper Sensor Handle passed. Please check and send valid Handle!");
      return;
    }
    int reqHandle = -1;
    if(sensorReqHandleMapTable.containsKey(sensorHandle) == true ) {
      USTALog.i("reqHandle present for this sensor");
      reqHandle = (int) sensorReqHandleMapTable.get(sensorHandle);
      if(sensorClientConnectIDMapTable.containsKey(sensorHandle) == true)
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(reqHandle).setClientConnectID((Integer) sensorClientConnectIDMapTable.get(sensorHandle));
      else {
        USTALog.i("clientConnectID NOT present for this sensor");
        return;
      }
    } else {
      USTALog.i("reqHandle NOT present for this sensor");
      return;
    }
    USTALog.d("stop request from command line for sensor " + Integer.toString(sensorHandle) + " with req handle " + reqHandle);
    sensorContext.sendRequest(sensorHandle, reqHandle, null, null, true);
    if(sensorClientConnectIDMapTable.containsKey(sensorHandle) == true)
      sensorClientConnectIDMapTable.remove(sensorHandle);
    if(reqHandle != -1) {
      sensorContext.getSensors().elementAt(sensorHandle).getSensorStdFields(reqHandle).setClientConnectID(-1);
      sensorReqHandleMapTable.remove(sensorHandle);
    }
    USTALog.d("after stop request for the sensorhandle " + Integer.toString(sensorHandle));
  }

  private void dumpSensorList() {

    Vector<String> sensorNames = sensorContext.getSensorNames();

    for (int index = 0; index < sensorNames.size(); index++) {

      USTALog.d("Sensor Handle" + DELIMITERCOLON + index +
              " Sensor Name" + DELIMITERCOLON + sensorNames.get(index) +
              " Sensor SUID Low" + DELIMITERCOLON + sensorContext.getSensors().get(index).getSensorSUIDLow() +
              " Sensor SUID High" + DELIMITERCOLON + sensorContext.getSensors().get(index).getSensorSUIDHigh());
    }
  }

  private void dumpSensorAttribList(Intent intent) {

    int sensorHandle = intent.getIntExtra("sensorhandle", -1);

    if (-1 == sensorHandle) {

      String sensorSUIDLow = intent.getStringExtra("suid_low");
      String sensorSUIDHigh = intent.getStringExtra("suid_high");

      sensorHandle = sensorContext.getSensorHandleFromSUID(sensorSUIDLow, sensorSUIDHigh);
    }
    if (-1 == sensorHandle) {
      USTALog.e("Improper Sensor SUID is passed. Please check and send valid SUID !");
    }

    if(SensorContextJNI.getSensorAttributesNativeWrapper(sensorContext.getUSTAModeType(), sensorHandle).length() <= MAX_ADB_LOGCAT_LENGTH) {
      USTALog.d(SensorContextJNI.getSensorAttributesNativeWrapper(sensorContext.getUSTAModeType(), sensorHandle));
    }
    else {
      int maxLoopCount = SensorContextJNI.getSensorAttributesNativeWrapper(sensorContext.getUSTAModeType(), sensorHandle).length() / MAX_ADB_LOGCAT_LENGTH + 1;
      String remainingLogBuffer = SensorContextJNI.getSensorAttributesNativeWrapper(sensorContext.getUSTAModeType(), sensorHandle);
      int loopCount = 0;
      while(loopCount < maxLoopCount) {
        int startIndex = 0;
        int endIndex = (remainingLogBuffer.length() > MAX_ADB_LOGCAT_LENGTH )?MAX_ADB_LOGCAT_LENGTH:remainingLogBuffer.length();
        String subString = remainingLogBuffer.substring(startIndex, endIndex);
        USTALog.d(subString);
        loopCount = loopCount + 1 ;
        remainingLogBuffer = remainingLogBuffer.substring(endIndex );
      }
    }

  }

  private void dumpReqMsgList(Intent intent) {

    int sensorHandle = intent.getIntExtra("sensorhandle", -1);

    if (-1 == sensorHandle) {

      String sensorSUIDLow = intent.getStringExtra("suid_low");
      String sensorSUIDHigh = intent.getStringExtra("suid_high");

      sensorHandle = sensorContext.getSensorHandleFromSUID(sensorSUIDLow, sensorSUIDHigh);
    }

    if (sensorHandle == -1) {

      USTALog.e( "Improper Sensor SUID is passed. Please check and send valid SUID !");
      return;
    }

    Sensor sensorInstance = sensorContext.getSensors().get(sensorHandle);

    if (sensorInstance == null) {

      USTALog.e("Improper Sensor Handle passed. Please check and send valid Handle!");
    } else {

      for (int reqMsgIndex = 0; reqMsgIndex < sensorInstance.getSensorReqMsgsNames().size(); reqMsgIndex++) {

        String reqMsgName = sensorInstance.getSensorReqMsgsNames().get(reqMsgIndex);
        String reqMsgID = sensorInstance.getSensorReqMsgIDs().get(reqMsgIndex);
        String payloadMsgFields;

        Vector<String> clientProcessors = sensorContext.getClientProcessors();
        String clientProcessorString = "";

        for (int clientProcIndex = 0; clientProcIndex < clientProcessors.size(); clientProcIndex++) {
          clientProcessorString = clientProcessorString + clientProcessors.get(clientProcIndex);
          if (clientProcIndex < clientProcessors.size() - 1) {
            clientProcessorString = clientProcessorString + COMMA;
          }
        }

        Vector<String> wakeupDelivery = sensorContext.getWakeupDelivery();
        String wakeupDeliveryString = "";

        for (int wakeupDeliveryIndex = 0; wakeupDeliveryIndex < wakeupDelivery.size(); wakeupDeliveryIndex++) {
          wakeupDeliveryString = wakeupDeliveryString + wakeupDelivery.get(wakeupDeliveryIndex);
          if (wakeupDeliveryIndex < wakeupDelivery.size() - 1) {
            wakeupDeliveryString = wakeupDeliveryString + COMMA;
          }
        }

        // Req Msg Name and Msg ID
        USTALog.d(reqMsgName + DELIMITERCOLON + reqMsgID);

        // Req Msg Std Fields
        USTALog.d("{Name:clientprocessor;DataType:enum;EnumValues:" + clientProcessorString + ";}");
        USTALog.d("{Name:wakeupDelivery;DataType:enum;EnumValues:" + wakeupDeliveryString + ";}");
        USTALog.d("{Name:batch_period;DataType:uint32;}");
        USTALog.d("{Name:flush_period;DataType:uint32;}");

        for (int payloadIndex = 0; payloadIndex < sensorInstance.getSensorPayloadField(reqMsgIndex).size(); payloadIndex++) {

          SensorPayloadField payloadField = sensorInstance.getSensorPayloadField(reqMsgIndex).get(payloadIndex);

          payloadMsgFields = "";
          payloadMsgFields = payloadMsgFields + LEFTBRACE + NAME + DELIMITERCOLON + payloadField.getFieldName() + DELIMITERSEMICOLON +
                  DATATYPE + DELIMITERCOLON + payloadField.getDataType().toString().toLowerCase() + DELIMITERSEMICOLON;

          if (payloadField.getDataType() == DataType.ENUM) {

            payloadMsgFields = payloadMsgFields + ENUMVALUES + DELIMITERCOLON;

            for (String enumVal : payloadField.getEnumValues()) {
              payloadMsgFields = payloadMsgFields + enumVal + COMMA;
            }

          }

          payloadMsgFields = payloadMsgFields + RIGHTBRACE;

          USTALog.d( payloadMsgFields);
        }
      }
    }
  }

  public IntentFilter getIntentFilter() {

    IntentFilter intentFilter = new IntentFilter();

    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.GETSENSORLIST");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.GETSENSORATTRIBUTES");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.GETREQUESTMSGS");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.ENABLE");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.DISABLE");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.DISABLELOGGING");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.POWERAUTOMATION_ENABLE");
    intentFilter.addAction("com.qualcomm.qti.usta.core.intent.POWERAUTOMATION_DISABLE");
    return intentFilter;
  }
}
