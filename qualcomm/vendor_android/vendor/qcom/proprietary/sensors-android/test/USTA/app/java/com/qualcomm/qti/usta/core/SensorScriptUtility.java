/*============================================================================
  Copyright (c) 2018,2020-2021, 2023 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file SensorScriptUtility.java
  @brief
  SensorScriptUtility class is an util function for script generation and reading.
============================================================================*/

package com.qualcomm.qti.usta.core;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Vector;

public class SensorScriptUtility {
  static final String SCRIPT_DIR_PATH = "/data/vendor/sensors/scripts";
  private int total_script_count = 0;
  private Vector<String> filteredList;

  private final String COLON = ":";
  private final String SEMICOLON = ";";
  private final String COMMA = ",";
  private final String REQMSG_ID = "Req_Msgs_ID";
  private final String CLIENT_PROCESSOR = "Client_Processor";
  private final String WAKEUP_DELIVERY = "wakeup_delivery";
  private final String BATCH_PERIOD = "batch_period";
  private final String FLUSH_PERIOD = "flush_period";
  private final String FLUSH_ONLY = "flush_only";
  private final String MAX_BATCH = "max_batch";
  private final String IS_PASSIVE = "is_passive";
  private final String RESAMPLER_CONFIG = "is_resample";
  private final String THRESHOLD_CONFIG = "is_threshold";
  private final String RESAMPLER_TYPE = "resampler_type";
  private final String RESAMPLER_FILTER = "resampler_filter";
  private final String THRESHOLD_TYPE = "threshold_type";
  private final String THRESHOLD_VALUE_X = "threshold_value_x_axis";
  private final String THRESHOLD_VALUE_Y = "threshold_value_y_axis";
  private final String THRESHOLD_VALUE_Z = "threshold_value_z_axis";
  private final String TRUE = "true";

  private Map fieldValueTable;

  public void SensorScriptUtility(){
    fieldValueTable = new HashMap();
  }

  public Vector<String> getScriptList(String filterString){
    filteredList = new Vector<>();
    filteredList.add("Nothing is selected");

    File[] filesList = new File(SCRIPT_DIR_PATH).listFiles();
    if(filesList == null)
      return filteredList;

    for (int i = 0; i < filesList.length; i++)
    {
      USTALog.d("File is at " + filesList[i].getPath());
      if(filesList[i].getPath().contains(filterString) == true)
        filteredList.add(filesList[i].getPath());
    }
    total_script_count = filteredList.size() - 1;
    USTALog.d("Number of scripts for sensor " + filterString + " " + total_script_count);
    return filteredList;
  }

  /*Creates an XML file and stores the data here*/
  public void createScriptFile(String fileName, String sensorDataType, String SUIDLow , String SUIDHigh, ReqMsgPayload reqMsg,SendReqStdFields sendReqStdFieldObj, boolean isResampled, boolean isThresholded){
    String filePath = "";
    String[] sensorDataTypeWithoutVendor = sensorDataType.split("-");
    if(fileName != null && fileName.length() != 0)
      filePath =  fileName + "_" + sensorDataTypeWithoutVendor[0] + "_" + SUIDLow + "_" + SUIDHigh + ".txt" ;
    else
      filePath = total_script_count + "_" + sensorDataTypeWithoutVendor[0] + "_" + SUIDLow + "_" + SUIDHigh + ".txt" ;
    USTALog.d("file Path " + filePath);
    try {
      File finalFile = new File(SCRIPT_DIR_PATH + "/" + filePath);
      if(finalFile.exists() == false){
        USTALog.d(finalFile + " doesn't exist. So creating new one");
        finalFile.createNewFile();
      }
      FileOutputStream fileInstance = new FileOutputStream(SCRIPT_DIR_PATH + "/" + filePath);
      USTALog.d("fileInstance is created ");
      String scriptContent = "";
      scriptContent = updateWithStandardFields(scriptContent , reqMsg.getMsgID(),sendReqStdFieldObj, isResampled, isThresholded);
      for (int fieldIndex = 0; fieldIndex < reqMsg.getFieldCount(); fieldIndex++) {
        SensorPayloadField payLoadField = reqMsg.getFieldAt(fieldIndex);
        scriptContent = updatePayLoadFields(scriptContent, payLoadField);
      }
      USTALog.d("fileInstance content " + scriptContent);
      fileInstance.write(scriptContent.getBytes());
      fileInstance.close();

      if(fileName == null || fileName.length() == 0)
        total_script_count++;

      USTALog.d("fileInstance is closed ");
    }catch (IOException ex) {
      USTALog.d("Exception occured while fileInstance update ");
      ex.printStackTrace();
    }
  }

  public Map getScriptDataTable(){
    return fieldValueTable;
  }

  public Map parseScriptFile(int ScriptFileHandle) {

    return parseScriptFile(filteredList.elementAt(ScriptFileHandle));
  }

  public Map parseScriptFile(String ScriptFilePath) {
    if(fieldValueTable != null)
      fieldValueTable.clear();
    if(fieldValueTable == null)
      fieldValueTable = new HashMap();

    String ScriptData = readStringFromFile(ScriptFilePath);
    Vector<String> fieldLists = new Vector<String>(Arrays.asList(ScriptData.split(SEMICOLON)));
    if(fieldLists != null && fieldLists.size() > 0){
      for(int fieldIndex = 0 ; fieldIndex < fieldLists.size() ; fieldIndex++){
        if(fieldLists.elementAt(fieldIndex) != null && fieldLists.elementAt(fieldIndex).length() != 0) {
          String[] fieldValueSeperator = fieldLists.elementAt(fieldIndex).split(COLON);
          String fieldName = fieldValueSeperator[0];
          if (fieldValueSeperator.length == 2) {
            Vector<String> fieldValues = new Vector<String>(Arrays.asList(fieldValueSeperator[1].split(COMMA)));
            fieldValueTable.put(fieldName, fieldValues);
          }
          else if(fieldValueSeperator.length == 1){
            fieldValueTable.put(fieldName, null);
          }
        }
      }
    }
    return fieldValueTable;
  }

  public Vector<String> getSUID(String scriptFileName){

    Vector<String> suidInfo = new Vector<>();

    String[] fileNameWithoutExtn = scriptFileName.split("\\.");
    if(fileNameWithoutExtn == null || fileNameWithoutExtn[0].length() ==0 ) {
      USTALog.e("Error in ScriptName format. Please provide proper one generated by USTA");
      return suidInfo; // Empty Vector<String>
    }

    String fileNameContents[] = fileNameWithoutExtn[0].split("_");
    if(fileNameContents == null || fileNameContents.length < 3 ) {
      USTALog.e("Error in ScriptName format. Please provide proper one generated by USTA");
      return suidInfo; // Empty Vector<String>
    }

    String suidLow = fileNameContents[fileNameContents.length-2];
    String suidHigh = fileNameContents[fileNameContents.length-1];
    if(suidLow == null || suidLow.length() == 0 || suidHigh == null || suidHigh.length() == 0){
      USTALog.e("Error in ScriptName format. Please provide proper one generated by USTA");
      return suidInfo; // Empty Vector<String>
    }

    suidInfo.add(suidLow);
    suidInfo.add(suidHigh);
    return  suidInfo;
  }

  private String readStringFromFile(String filePath) {
    String scriptContent = "";
    try {
      FileInputStream fileInstance = new FileInputStream(filePath);
      while (true) {
        int currentByte = fileInstance.read();
        if (currentByte == -1)
          break;
        else {
          scriptContent = scriptContent + Character.toString((char) currentByte);
        }
      }
    } catch (IOException e) {
      USTALog.e("Script file read exception" + e.toString());
    }
    USTALog.i("scriptContent is " + scriptContent);
    return scriptContent;
  }

  private String updatePayLoadFields(String scriptContent , SensorPayloadField payLoadField){

      if (payLoadField.isUserDefined()) {
        USTALog.d("User defined field");
      } else {
        scriptContent = scriptContent + payLoadField.getFieldName();
        scriptContent = scriptContent + COLON;
        if (payLoadField.getAccessModifier() == AccessModifier.REPEATED) {
          String[] repeatedValues = payLoadField.getValuesForNative();
          for (int repeatCount = 0; repeatCount < repeatedValues.length; repeatCount++) {
            scriptContent = scriptContent + repeatedValues[repeatCount];
            if(repeatCount != repeatedValues.length - 1) {
              scriptContent = scriptContent + COMMA;
            }
          }
        } else {
          if(payLoadField.getValue().size() != 0) {
            scriptContent = scriptContent + payLoadField.getValue().elementAt(0);
          }
        }
        scriptContent = scriptContent + SEMICOLON;
      }

    return scriptContent;
  }

  private String updateWithStandardFields(String scriptContent , String reqMsgID ,SendReqStdFields sendReqStdFieldObj,  boolean isResampled, boolean isThresholded){

    scriptContent = scriptContent + REQMSG_ID;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + reqMsgID;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + CLIENT_PROCESSOR;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.clientType;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + WAKEUP_DELIVERY;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.wakeupType;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + BATCH_PERIOD;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.batchPeriod;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + FLUSH_PERIOD;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.flushPeriod;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + FLUSH_ONLY;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.flushOnly;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + MAX_BATCH;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.maxBatch;
    scriptContent = scriptContent + SEMICOLON;

    scriptContent = scriptContent + IS_PASSIVE;
    scriptContent = scriptContent + COLON;
    scriptContent = scriptContent + sendReqStdFieldObj.isPassive;
    scriptContent = scriptContent + SEMICOLON;

    if(isResampled == true ) {
      scriptContent = scriptContent + RESAMPLER_CONFIG;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + "true";
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + RESAMPLER_TYPE;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.resamplerType;
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + RESAMPLER_FILTER;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.resamplerFilter;
      scriptContent = scriptContent + SEMICOLON;
    }

    if(isThresholded == true) {
      scriptContent = scriptContent + THRESHOLD_CONFIG;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + "true";
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + THRESHOLD_TYPE;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.thresholdType;
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + THRESHOLD_VALUE_X;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.thresholdValueX;
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + THRESHOLD_VALUE_Y;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.thresholdValueY;
      scriptContent = scriptContent + SEMICOLON;

      scriptContent = scriptContent + THRESHOLD_VALUE_Z;
      scriptContent = scriptContent + COLON;
      scriptContent = scriptContent + sendReqStdFieldObj.thresholdValueZ;
      scriptContent = scriptContent + SEMICOLON;
    }

    return scriptContent;
  }
}
