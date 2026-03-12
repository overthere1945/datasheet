/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <jni.h>
#include "sensor_cxt.h"
#include <mutex>

/*Includes for Device Mode feature start */
#include "AEEStdErr.h"
#include "rpcmem.h"
#include "remote.h"
#include "sns_device_mode.h"
#include "sns_device_mode.pb.h"
#include "sensors_timeutil.h"
#include <cutils/properties.h>
#include <sensors_target.h>
#define USTA_GLINK_TRANSPORT_NAME "ssc_usta"

using namespace std;

std::atomic<bool> is_device_mode_handle_opened = false;
static remote_handle64 device_mode_remote_handle = 0;
/*Includes for Device Mode feature End*/

vector<string> samplesBufferPool;
vector<string> registrySamplesBufferPool;
std::mutex cv_mutex;
std::condition_variable cv_sample_ready;
std::mutex g_registry_cv_mutex;
std::condition_variable g_registry_cv_sample_ready;
string suid_registry = "0x5e2541b4701e2275";

extern vector<int> gDirectChannelFDArray ;
extern vector<char*> gDirectChannelMempointer ;
/*10secs worth of data with 8K sample rate - each sample size of 104 Bytes*/
#define DIRECT_CHANNEL_SHARED_MEMORY_SIZE (10*8000*104)   // 16Byte multiple
bool is_property_read = false;
bool is_direct_channel_logging_enabled = false;

/*Below Enum Should be in sync with Java Ewnum */
typedef enum DataType {
  STRING,
  SIGNED_INTEGER32,
  SIGNED_INTEGER64,
  UNSIGNED_INTEGER32,
  UNSIGNED_INTEGER64,
  FLOAT,
  BOOLEAN,
  ENUM,
  USER_DEFINED
}DataType;

typedef enum AccessModifier {
  REQUIRED,
  OPTIONAL,
  REPEATED
}AccessModifier;

typedef enum ModeType {
  USTA_MODE_TYPE_UI,
  USTA_MODE_TYPE_COMMAND_LINE,
  USTA_MODE_TYPE_DIRECT_CHANNEL_UI,
  /*Please add all other modes before this line. USTA_MODE_TYPE_COUNT will be the last line to have count */
  USTA_MODE_TYPE_COUNT
}ModeType;

static int current_sensor_handle = -1;
SensorCxt* gSensorContextInstance[USTA_MODE_TYPE_COUNT] = {NULL, NULL, NULL};

jobject setFieldInfo(JNIEnv *env, jobject obj, field_info &in_filed_info);
jobject setAccessModifier(JNIEnv *env, jobject obj, string label);
jobject setDataType(JNIEnv *env, jobject obj, string data_type);
jobject setNestedMessageInfo(JNIEnv *env, jobject obj, nested_msg_info &nested_msg);
void getFieldInfo(JNIEnv *env, jobject obj, jobject jfieldobject, send_req_field_info &each_field);


void streamEventsDisplayCallback(string displaySamples , bool is_registry_sensor)
{
  if(is_registry_sensor == false)
  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    samplesBufferPool.insert(samplesBufferPool.begin(), displaySamples);
    cv_sample_ready.notify_one();
  } else if(is_registry_sensor == true) {
    std::unique_lock<std::mutex> lock(g_registry_cv_mutex);
    registrySamplesBufferPool.insert(registrySamplesBufferPool.begin(), displaySamples);
    g_registry_cv_sample_ready.notify_one();
  }
}

void sharedConnEventCallback(string sensor_event, unsigned int sensor_handle, unsigned int client_connect_id)
{
  UNUSED_VAR(client_connect_id);
  if(current_sensor_handle ==  sensor_handle){
     //Checking if the sensor event is registry sensor event using suid.
     if(sensor_event.find(suid_registry)!= string::npos)
        streamEventsDisplayCallback(sensor_event, true);
     else
        streamEventsDisplayCallback(sensor_event, false);
  }
}

string getModeType(JNIEnv *env, jobject obj, jobject modeType)
{
  UNUSED_VAR(obj);
  /*Get the object */
  jclass modeType_class = env->GetObjectClass(modeType);
  if(modeType_class == NULL)
  {
    sns_loge("modeType_class is not found");
    return NULL;
  }

  jmethodID GetEnumvalueMethod = env->GetMethodID(modeType_class, "getEnumValue", "()I");
  jint value = env->CallIntMethod(modeType, GetEnumvalueMethod);

  string modetype;
  if(value == USTA_MODE_TYPE_UI)
  {
    modetype = "UI";
  }
  else if(value == USTA_MODE_TYPE_COMMAND_LINE)
  {
    modetype = "COMMAND_LINE";
  }
  else if(value == USTA_MODE_TYPE_DIRECT_CHANNEL_UI)
  {
    modetype = "DIRECT_CHANNEL_UI";
  }

  return modetype;

}

jobjectArray getSensorList(JNIEnv *env, jobject obj, jobject inModeType)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);
  sns_logi(" getSensorList Start ");
  vector<sensor_info> sensorListInfo;
  string modeType = getModeType(env, obj, inModeType);
  char property[PROPERTY_VALUE_MAX];

  if(modeType.compare("UI") == 0)
  {
    sns_logi("getSensorList UI Mode it is ");
    gSensorContextInstance[USTA_MODE_TYPE_UI] = SensorCxt::getInstance();
    gSensorContextInstance[USTA_MODE_TYPE_UI]->get_sensor_list(sensorListInfo);
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    sns_logi("getSensorList COMMAND_LINE Mode it is ");
      gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE] = SensorCxt::getInstance();
    gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_sensor_list(sensorListInfo);
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    sns_logi("getSensorList DIRECT_CHANNEL_UI Mode it is ");
    gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI] = SensorCxt::getInstance();
    gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_sensor_list(sensorListInfo);
  }

  jobjectArray javaSensorListArray = NULL;
  javaSensorListArray = (jobjectArray)env->NewObjectArray(sensorListInfo.size(),env->FindClass("com/qualcomm/qti/usta/core/SensorListInfo"),NULL);
  if(javaSensorListArray == NULL)
    return NULL;

  for(unsigned int sensor_index = 0; sensor_index < sensorListInfo.size() ; sensor_index++)
  {

    jclass sensor_info_class = env->FindClass("com/qualcomm/qti/usta/core/SensorListInfo");
    if(sensor_info_class == NULL)
    {
      sns_loge("sensor_info_class is not found");
      return NULL;
    }

    jmethodID sensor_info_methodID = env->GetMethodID(sensor_info_class, "<init>", "()V");
    if(sensor_info_methodID == NULL)
    {
      sns_loge("sensor_info_methodID is not created");
      return NULL;
    }

    jobject sensor_instance = env->NewObject(sensor_info_class, sensor_info_methodID);

    /*dataType*/
    jfieldID fid = env->GetFieldID(sensor_info_class, "dataType", "Ljava/lang/String;");
    if(fid == 0)
      return NULL;
    jstring str = env->NewStringUTF(sensorListInfo[sensor_index].data_type.c_str());
    env->SetObjectField(sensor_instance, fid, str);

    /*vendor*/
    fid = env->GetFieldID(sensor_info_class, "vendor", "Ljava/lang/String;");
    if (fid == 0)
      return NULL;
    str = env->NewStringUTF(sensorListInfo[sensor_index].vendor.c_str());
    env->SetObjectField(sensor_instance, fid, str);


    /*suidLow*/
    fid = env->GetFieldID(sensor_info_class, "suidLow", "Ljava/lang/String;");
    if (fid == 0)
      return NULL;
    str = env->NewStringUTF(sensorListInfo[sensor_index].suid_low.c_str());
    env->SetObjectField(sensor_instance, fid, str);


    /*suidHigh*/
    fid = env->GetFieldID(sensor_info_class, "suidHigh", "Ljava/lang/String;");
    if (fid == 0)
      return NULL;
    str = env->NewStringUTF(sensorListInfo[sensor_index].suid_high.c_str());
    env->SetObjectField(sensor_instance, fid, str);


    /*Pushing everything to the vector */
    env->SetObjectArrayElement(javaSensorListArray,sensor_index, sensor_instance);

  }


  sns_logi(" getSensorList End ");

  return javaSensorListArray;

}

jobjectArray getClientProcessors(JNIEnv *env , jobject jOb, jobject inModeType)
{
  UNUSED_VAR(jOb);
  vector<string> client_processor;
  string modeType = getModeType(env, jOb, inModeType);
  if(modeType.compare("UI") == 0)
  {
    client_processor = gSensorContextInstance[USTA_MODE_TYPE_UI]->get_client_processor_list();
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    client_processor = gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_client_processor_list();
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    client_processor = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_client_processor_list();
  }
  jobjectArray javaStringArray;
  javaStringArray= (jobjectArray)env->NewObjectArray(client_processor.size(),env->FindClass("java/lang/String"),env->NewStringUTF(""));

  for(unsigned int index = 0; index < client_processor.size(); index++)
  {
    env->SetObjectArrayElement(javaStringArray,index, env->NewStringUTF(client_processor[index].c_str()));
  }

  return javaStringArray;
}

jobjectArray getWakeupDeliveryType(JNIEnv *env , jobject jOb, jobject inModeType)
{
  UNUSED_VAR(jOb);
  vector<string> wakeup_delivery_type;
  string modeType = getModeType(env, jOb, inModeType);
  if(modeType.compare("UI") == 0)
  {
    wakeup_delivery_type = gSensorContextInstance[USTA_MODE_TYPE_UI]->get_wakeup_delivery_list();
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    wakeup_delivery_type = gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_wakeup_delivery_list();
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    wakeup_delivery_type = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_wakeup_delivery_list();
  }
  jobjectArray javaStringArray;
  javaStringArray= (jobjectArray)env->NewObjectArray(wakeup_delivery_type.size(),env->FindClass("java/lang/String"),env->NewStringUTF(""));
  for(unsigned int index = 0; index < wakeup_delivery_type.size(); index++)
  {
    env->SetObjectArrayElement(javaStringArray,index, env->NewStringUTF(wakeup_delivery_type[index].c_str()));
  }

  return javaStringArray;
}

jobjectArray getResamplerType(JNIEnv *env , jobject jOb, jobject inModeType)
{
  UNUSED_VAR(jOb);
  vector<string> resampler_type;
  string modeType = getModeType(env, jOb, inModeType);
    if(modeType.compare("UI") == 0)
  {
    resampler_type = gSensorContextInstance[USTA_MODE_TYPE_UI]->get_resampler_type_list();
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    resampler_type = gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_resampler_type_list();
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    resampler_type = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_resampler_type_list();
  }
  jobjectArray javaStringArray;
  javaStringArray= (jobjectArray)env->NewObjectArray(resampler_type.size(),env->FindClass("java/lang/String"),env->NewStringUTF(""));

  for(unsigned int index = 0; index < resampler_type.size(); index++)
  {
    env->SetObjectArrayElement(javaStringArray,index, env->NewStringUTF(resampler_type[index].c_str()));
  }

  return javaStringArray;
}

jobjectArray getThresholdType(JNIEnv *env , jobject jOb, jobject inModeType)
{
  UNUSED_VAR(jOb);
  vector<string> threshold_type;
  string modeType = getModeType(env, jOb, inModeType);
    if(modeType.compare("UI") == 0)
  {
    threshold_type = gSensorContextInstance[USTA_MODE_TYPE_UI]->get_threshold_type_list();
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    threshold_type = gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_threshold_type_list();
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    threshold_type = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_threshold_type_list();
  }
  jobjectArray javaStringArray;
  javaStringArray= (jobjectArray)env->NewObjectArray(threshold_type.size(),env->FindClass("java/lang/String"),env->NewStringUTF(""));

  for(unsigned int index = 0; index < threshold_type.size(); index++)
  {
    env->SetObjectArrayElement(javaStringArray,index, env->NewStringUTF(threshold_type[index].c_str()));
  }

  return javaStringArray;
}

void removeSensors(JNIEnv *env , jobject jOb, jobject inModeType , jintArray inSensorHandleArray)
{
  UNUSED_VAR(jOb);

  string modeType = getModeType(env, jOb, inModeType);

  int sensorHandleArrayLength = env->GetArrayLength(inSensorHandleArray);

  vector<int> sensorsToBeRemoved;

  for(int index = 0 ; index < sensorHandleArrayLength ; index++)
  {
    int currentSensorHandle = -1;
    env->GetIntArrayRegion(inSensorHandleArray, index, 1, &currentSensorHandle);
    if(currentSensorHandle == -1 )
    {
      sns_loge("Error while getting currentSensorHandle in removeSensors JNI Call");
      continue;
    }

    sensorsToBeRemoved.push_back(currentSensorHandle);
  }
  if(modeType.compare("UI") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_UI]->remove_sensors(sensorsToBeRemoved);
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->remove_sensors(sensorsToBeRemoved);
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->remove_sensors(sensorsToBeRemoved);
  }

}

jobject setAccessModifier(JNIEnv *env, jobject obj, string label)
{
  UNUSED_VAR(obj);
  jclass accessModifier_class = env->FindClass("com/qualcomm/qti/usta/core/AccessModifier");
  if(accessModifier_class == NULL)
  {
    sns_loge("setAccessModifier is not found");
    return NULL;
  }

  jfieldID fid = 0;
  if(label.compare("required") == 0)
  {
    fid = env->GetStaticFieldID(accessModifier_class, "REQUIRED" , "Lcom/qualcomm/qti/usta/core/AccessModifier;");
  }
  else if(label.compare("optional") == 0)
  {
    fid = env->GetStaticFieldID(accessModifier_class, "OPTIONAL" , "Lcom/qualcomm/qti/usta/core/AccessModifier;");
  }
  else if(label.compare("repeated") == 0)
  {
    fid = env->GetStaticFieldID(accessModifier_class, "REPEATED" , "Lcom/qualcomm/qti/usta/core/AccessModifier;");
  }
  if(fid == 0)
    return NULL;

  jobject accessModifier_instance = env->GetStaticObjectField(accessModifier_class, fid);

  return accessModifier_instance;

}

string getAccessModifier(JNIEnv *env, jobject obj, jobject accessModifer)
{
  UNUSED_VAR(obj);
  string label;


  /*Get the object */
  jclass acessModifier_class = env->GetObjectClass(accessModifer);
  if(acessModifier_class == NULL)
  {
    sns_loge("acessModifier_class is not found");
    return NULL;
  }

  jmethodID GetAcessModiferValueMethod = env->GetMethodID(acessModifier_class, "getAccessModifierValue", "()I");
  jint value = env->CallIntMethod(accessModifer, GetAcessModiferValueMethod);

  if(value == REQUIRED)
  {
    label = "required";
  }
  else if(value == OPTIONAL)
  {
    label = "optional";
  }
  else if(value == REPEATED)
  {
    label = "repeated";
  }

  return label;

}

jobject setDataType(JNIEnv *env, jobject obj, string data_type)
{
  UNUSED_VAR(obj);
  jclass datatype_class = env->FindClass("com/qualcomm/qti/usta/core/DataType");
  if(datatype_class == NULL)
  {
    sns_loge("DataType is not found");
    return NULL;
  }

  jfieldID fid ;
  if(data_type.compare("enum") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "ENUM" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("int32") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "SIGNED_INTEGER32" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("int64") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "SIGNED_INTEGER64" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("uint32") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "UNSIGNED_INTEGER32" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("uint64") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "UNSIGNED_INTEGER64" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("float") == 0 || data_type.compare("double") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "FLOAT" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("bool") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "BOOLEAN" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else if(data_type.compare("string") == 0)
  {
    fid = env->GetStaticFieldID(datatype_class, "STRING" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  else
  {
    fid = env->GetStaticFieldID(datatype_class, "USER_DEFINED" , "Lcom/qualcomm/qti/usta/core/DataType;");
  }
  if(fid == 0)
    return NULL;

  jobject datatype_instance = env->GetStaticObjectField(datatype_class, fid);


  return datatype_instance;

}
string getDataType(JNIEnv *env, jobject obj, jobject dataType)
{
  UNUSED_VAR(obj);
  /*Get the object */
  jclass dataType_class = env->GetObjectClass(dataType);
  if(dataType_class == NULL)
  {
    sns_loge("dataType_class is not found");
    return NULL;
  }

  jmethodID GetEnumvalueMethod = env->GetMethodID(dataType_class, "getEnumValue", "()I");
  jint value = env->CallIntMethod(dataType, GetEnumvalueMethod);

  string datatype;
  if(value == ENUM)
  {
    datatype = "enum";
  }
  else if(value == SIGNED_INTEGER32)
  {
    datatype = "int32";
  }
  else if(value == SIGNED_INTEGER64)
  {
    datatype = "int64";
  }
  else if(value == UNSIGNED_INTEGER32)
  {
    datatype = "uint32";
  }
  else if(value == UNSIGNED_INTEGER64)
  {
    datatype = "uint64";
  }
  else if(value == FLOAT)
  {
    datatype = "float";
  }
  else if(value == BOOLEAN)
  {
    datatype = "bool";
  }
  else if(value == STRING)
  {
    datatype = "string";
  }

  return datatype;

}


jobject setNestedMessageInfo(JNIEnv *env, jobject obj, nested_msg_info &nested_msg)
{
  UNUSED_VAR(obj);
  jclass nested_msg_class = env->FindClass("com/qualcomm/qti/usta/core/NestedMsgPayload");
  if(nested_msg_class == NULL)
  {
    sns_loge("NestedMsgPayload is not found");
    return NULL;
  }

  jmethodID nested_msg_methodID = env->GetMethodID(nested_msg_class, "<init>", "()V");
  if(nested_msg_methodID == NULL)
  {
    sns_loge("NestedMsgPayload is not created");
    return NULL;
  }

  jobject nested_msg_instance = env->NewObject(nested_msg_class, nested_msg_methodID);

  /*nested msg name*/
  jfieldID fid = env->GetFieldID(nested_msg_class, "nestedMsgName", "Ljava/lang/String;");
  if (fid == 0)
    return NULL;
  jstring str = env->NewStringUTF(nested_msg.msg_name.c_str());
  env->SetObjectField(nested_msg_instance, fid, str);

  /*Vector of fields */
  jobjectArray javaSensorPayLoadFieldsArray;
  javaSensorPayLoadFieldsArray= (jobjectArray)env->NewObjectArray(nested_msg.fields.size(),env->FindClass("com/qualcomm/qti/usta/core/SensorPayloadField"),NULL);

  for(unsigned int field_index = 0 ; field_index < nested_msg.fields.size() ; field_index++)
  {
    jobject field_instance = setFieldInfo(env, obj, nested_msg.fields[field_index]);
    env->SetObjectArrayElement(javaSensorPayLoadFieldsArray,field_index, field_instance);
  }
  fid = env->GetFieldID(nested_msg_class, "fields", "[Lcom/qualcomm/qti/usta/core/SensorPayloadField;");
  if (fid == 0)
    return NULL;
  env->SetObjectField(nested_msg_instance, fid, javaSensorPayLoadFieldsArray);


  return nested_msg_instance;


}


void getNestedMessageInfo(JNIEnv *env, jobject obj, jobject jNestedReqMsg, send_req_nested_msg_info &nested_msg)
{
  UNUSED_VAR(obj);
  /*Get the object */
  jclass req_msg_class = env->GetObjectClass(jNestedReqMsg);
  if(req_msg_class == NULL)
  {
    sns_loge("req_msg_class is not found");
    return;
  }

  /*Message Name*/
  jmethodID GetMessageNameMethod = env->GetMethodID(req_msg_class, "getNestedMsgName", "()Ljava/lang/String;");
  jstring jMsgName = (jstring) env->CallObjectMethod(jNestedReqMsg, GetMessageNameMethod);

  const char *msgNamePtr = env->GetStringUTFChars(jMsgName, NULL);
  string message_name(msgNamePtr);
  nested_msg.msg_name = message_name;

  /*SensorPayloadField[]*/

  jfieldID fid = env->GetFieldID(req_msg_class, "fields", "[Lcom/qualcomm/qti/usta/core/SensorPayloadField;");
  if (fid == 0)
    return;
  jobjectArray javaSensorPayLoadFieldsArray = (jobjectArray) env->GetObjectField(jNestedReqMsg, fid);
  int fieldCount = env->GetArrayLength(javaSensorPayLoadFieldsArray);

  for(int field_index = 0 ; field_index < fieldCount; field_index++)
  {
    jobject jfieldobject = env->GetObjectArrayElement(javaSensorPayLoadFieldsArray, field_index);
    send_req_field_info each_field;
    getFieldInfo(env, obj, jfieldobject, each_field);
    nested_msg.fields.push_back(each_field);
  }


}


jobject setFieldInfo(JNIEnv *env, jobject obj, field_info &in_filed_info)
{
  UNUSED_VAR(obj);
  jclass filed_info_class = env->FindClass("com/qualcomm/qti/usta/core/SensorPayloadField");
  if(filed_info_class == NULL)
  {
    sns_loge("SensorPayloadField is not found");
    return NULL;
  }

  jmethodID field_info_methodID = env->GetMethodID(filed_info_class, "<init>", "()V");
  if(field_info_methodID == NULL)
  {
    sns_loge("SensorPayloadField is not created");
    return NULL;
  }

  jobject field_info_instance = env->NewObject(filed_info_class, field_info_methodID);

  /*Name*/
  jfieldID fid = env->GetFieldID(filed_info_class, "fieldName", "Ljava/lang/String;");
  if (fid == 0)
    return NULL;
  jstring str = env->NewStringUTF(in_filed_info.name.c_str());
  env->SetObjectField(field_info_instance, fid, str);


  /*Data type*/
  fid = env->GetFieldID(filed_info_class, "dataType", "Lcom/qualcomm/qti/usta/core/DataType;");
  if (fid == 0)
    return NULL;
  jobject jdatatype = setDataType(env, obj, in_filed_info.data_type);
  env->SetObjectField(field_info_instance, fid, jdatatype);

  /*Lable*/
  fid = env->GetFieldID(filed_info_class, "accessModifier", "Lcom/qualcomm/qti/usta/core/AccessModifier;");
  if (fid == 0)
    return NULL;
  jobject jAccessModifier = setAccessModifier(env, obj,in_filed_info.label);
  env->SetObjectField(field_info_instance, fid, jAccessModifier);

  /*is_user_defined_msg */
  fid = env->GetFieldID(filed_info_class, "isUserDefined", "Z");
  if (fid == 0)
    return NULL;
  env->SetBooleanField(field_info_instance, fid, in_filed_info.is_user_defined_msg);



  /*Nested Messages*/
  if(in_filed_info.is_user_defined_msg == true)
  {

    /*Vector of nested messages*/
    jobjectArray javaNestedMsgArray;
    javaNestedMsgArray= (jobjectArray)env->NewObjectArray(in_filed_info.nested_msgs.size(),env->FindClass("com/qualcomm/qti/usta/core/NestedMsgPayload"),NULL);

    for(unsigned int nested_msg_index = 0 ; nested_msg_index < in_filed_info.nested_msgs.size() ; nested_msg_index++)
    {
      jobject nested_msg_instance = setNestedMessageInfo(env, obj, in_filed_info.nested_msgs[nested_msg_index]);
      env->SetObjectArrayElement(javaNestedMsgArray,nested_msg_index, nested_msg_instance);
    }
    fid = env->GetFieldID(filed_info_class, "nestedMsgs", "[Lcom/qualcomm/qti/usta/core/NestedMsgPayload;");
    if (fid == 0)
      return NULL;
    env->SetObjectField(field_info_instance, fid, javaNestedMsgArray);

  }
  else
  {
    /*Has default*/
    fid = env->GetFieldID(filed_info_class, "hasDefaultValue", "Z");
    if (fid == 0)
      return NULL;
    env->SetBooleanField(field_info_instance, fid, in_filed_info.has_default);

    /*is enum*/
    fid = env->GetFieldID(filed_info_class, "isEnumerated", "Z");
    if (fid == 0)
      return NULL;
    env->SetBooleanField(field_info_instance, fid, in_filed_info.is_enum);


    if(in_filed_info.has_default == true)
    {
      /*default value*/
      fid = env->GetFieldID(filed_info_class, "defaultValue", "Ljava/lang/String;");
      if (fid == 0)
        return NULL;
      str = env->NewStringUTF(in_filed_info.default_val.c_str());
      env->SetObjectField(field_info_instance, fid, str);
    }

    if(in_filed_info.is_enum == true)
    {
      /*enum_list*/

      jobjectArray javaEnumListArray;
      javaEnumListArray= (jobjectArray)env->NewObjectArray(in_filed_info.enum_list.size(),env->FindClass("java/lang/String"),env->NewStringUTF(""));

      for(unsigned int enum_index = 0 ; enum_index < in_filed_info.enum_list.size(); enum_index ++)
      {
        env->SetObjectArrayElement(javaEnumListArray,enum_index, env->NewStringUTF(in_filed_info.enum_list[enum_index].name.c_str()));
      }
      fid = env->GetFieldID(filed_info_class, "enumValues", "[Ljava/lang/String;");
      if (fid == 0)
        return NULL;
      env->SetObjectField(field_info_instance, fid, javaEnumListArray);
    }

  }

  return field_info_instance;

}
void getFieldInfo(JNIEnv *env, jobject obj, jobject jfieldobject, send_req_field_info &each_field)
{
  UNUSED_VAR(obj);

  /*Get the object */
  jclass field_class = env->GetObjectClass(jfieldobject);
  if(field_class == NULL)
  {
    sns_loge("field_class is not found");
    return;
  }

  /*fieldName*/
  jfieldID fid = env->GetFieldID(field_class, "fieldName", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jFieldName = (jstring) env->GetObjectField(jfieldobject, fid);
  const char *fieldNamePtr = env->GetStringUTFChars(jFieldName, NULL);
  string field_name(fieldNamePtr);
  each_field.name = field_name;

  /*is user defined */
  fid = env->GetFieldID(field_class, "isUserDefined", "Z");
  if (fid == 0)
    return;
  jboolean jisUserDefined = env->GetBooleanField(jfieldobject, fid);
  bool isUserDefined = jisUserDefined;


  if(isUserDefined == true)
  {
    /*NestedMsgPayload[] */
    fid = env->GetFieldID(field_class, "nestedMsgs", "[Lcom/qualcomm/qti/usta/core/NestedMsgPayload;");
    if (fid == 0)
      return;

    jobjectArray javaNestedMessagesArray = (jobjectArray) env->GetObjectField(jfieldobject, fid);
    int nestedMsgCount = env->GetArrayLength(javaNestedMessagesArray);

    for(int index = 0 ; index < nestedMsgCount; index++)
    {
      jobject jnestedMessageobject = env->GetObjectArrayElement(javaNestedMessagesArray, index);
      send_req_nested_msg_info nestedMsgInfo ;
      getNestedMessageInfo(env, obj, jnestedMessageobject, nestedMsgInfo);
      each_field.nested_msgs.push_back(nestedMsgInfo);
    }

  }
  else
  {
    /*Data type*/
    fid = env->GetFieldID(field_class, "dataType", "Lcom/qualcomm/qti/usta/core/DataType;");
    if (fid == 0)
      return;
    jobject jdatatype = env->GetObjectField(jfieldobject, fid);
    string dataType = getDataType(env, obj, jdatatype);
    //    /*Lable*/
    fid = env->GetFieldID(field_class, "accessModifier", "Lcom/qualcomm/qti/usta/core/AccessModifier;");
    if (fid == 0)
      return;
    jobject jAccessModifier = env->GetObjectField(jfieldobject, fid);
    string accessModifier = getAccessModifier(env, obj,jAccessModifier);

    /*value of the field */
    fid = env->GetFieldID(field_class, "valuesForNative", "[Ljava/lang/String;");
    if (fid == 0)
    {
      sns_loge("valuesForNative field not found ");
      return;
    }
    jobjectArray javaValueArray = (jobjectArray) env->GetObjectField(jfieldobject, fid);
    int valueCount = env->GetArrayLength(javaValueArray);
    if(valueCount ==0 )
    {
      if(accessModifier.compare("required") == 0 && dataType.compare("bool") ==0 )
      {
        each_field.value.push_back("false");
      }
    }

    for(int value_index = 0 ; value_index < valueCount; value_index++)
    {
      jstring jValue = (jstring) env->GetObjectArrayElement(javaValueArray, value_index);
      if(jValue == NULL)
      {
        return;
      }
      const char *valuePtr = env->GetStringUTFChars(jValue, NULL);
      string value(valuePtr);
      if(each_field.name.compare("suid_low") == 0 ||
          each_field.name.compare("suid_high") == 0)
      {
        google::protobuf::uint64 suid_part = 0;
        std::stringstream stream;
        stream << value;
        stream >> std::hex >> suid_part;
        each_field.value.push_back(to_string(suid_part));
      }
      else
      {
        if(value.empty())
        {
          if(accessModifier.compare("required") == 0 && dataType.compare("bool") ==0 )
          {
            each_field.value.push_back("false");
          }
        }
        else
        {
          each_field.value.push_back(value);
        }
      }
    }
  }
}


jobjectArray getRequestMessages(JNIEnv *env, jobject obj, jobject inModeType, jint jSensorHandle)
{
  UNUSED_VAR(obj);
  vector<msg_info> request_msgs;
  int sensorHandle = jSensorHandle;
  string modeType = getModeType(env, obj, inModeType);
  if(modeType.compare("UI") == 0)
  {
    sns_logi("getRequestMessages UI Mode it is ");
    gSensorContextInstance[USTA_MODE_TYPE_UI]->get_request_list(sensorHandle, request_msgs);
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    sns_logi("getRequestMessages COMMAND_LINE Mode it is ");
    gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_request_list(sensorHandle, request_msgs);
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    sns_logi("getRequestMessages DIRECT_CHANNEL_UI Mode it is ");
    gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_request_list(sensorHandle, request_msgs);
  }

  jobjectArray javaReqMsgArray;
  javaReqMsgArray= (jobjectArray)env->NewObjectArray(request_msgs.size(),env->FindClass("com/qualcomm/qti/usta/core/ReqMsgPayload"),NULL);

  for(unsigned int req_msg_index = 0; req_msg_index < request_msgs.size() ; req_msg_index++)
  {
    jclass req_msg_class = env->FindClass("com/qualcomm/qti/usta/core/ReqMsgPayload");
    if(req_msg_class == NULL)
    {
      sns_loge("ReqMsgPayload is not found");
      return NULL;
    }

    jmethodID reg_msg_methodID = env->GetMethodID(req_msg_class, "<init>", "()V");
    if(reg_msg_methodID == NULL)
    {
      sns_loge("ReqMsgPayload is not created");
      return NULL;
    }

    jobject reg_msg_instance = env->NewObject(req_msg_class, reg_msg_methodID);

    /*Message Name*/
    jfieldID fid = env->GetFieldID(req_msg_class, "msgName", "Ljava/lang/String;");
    if (fid == 0)
      return NULL;
    jstring str = env->NewStringUTF(request_msgs[req_msg_index].msg_name.c_str());
    env->SetObjectField(reg_msg_instance, fid, str);


    /*Message ID*/
    fid = env->GetFieldID(req_msg_class, "msgID", "Ljava/lang/String;");
    if (fid == 0)
      return NULL;
    str = env->NewStringUTF(request_msgs[req_msg_index].msgid.c_str());
    env->SetObjectField(reg_msg_instance, fid, str);


    /*Vector of fields*/
    jobjectArray javaSensorPayLoadFieldsArray;
    javaSensorPayLoadFieldsArray= (jobjectArray)env->NewObjectArray(request_msgs[req_msg_index].fields.size(),env->FindClass("com/qualcomm/qti/usta/core/SensorPayloadField"),NULL);

    for(unsigned int field_index = 0 ; field_index < request_msgs[req_msg_index].fields.size() ; field_index++)
    {
      jobject field_instance = setFieldInfo(env, obj, request_msgs[req_msg_index].fields[field_index]);
      env->SetObjectArrayElement(javaSensorPayLoadFieldsArray,field_index, field_instance);
    }
    fid = env->GetFieldID(req_msg_class, "fields", "[Lcom/qualcomm/qti/usta/core/SensorPayloadField;");
    if (fid == 0)
      return NULL;
    env->SetObjectField(reg_msg_instance, fid, javaSensorPayLoadFieldsArray);


    /*Pushing everything to the vector */
    env->SetObjectArrayElement(javaReqMsgArray,req_msg_index, reg_msg_instance);

  }
  return javaReqMsgArray;
}

jstring getSensorAttributes(JNIEnv *env, jobject obj, jobject inModeType, jint jSensorHandle)
{
  UNUSED_VAR(obj);
  string sensorAttributes;
  int sensorHandle = jSensorHandle;
  string modeType = getModeType(env, obj, inModeType);
  if(modeType.compare("UI") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_UI]->get_attributes(sensorHandle, sensorAttributes);
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->get_attributes(sensorHandle, sensorAttributes);
  }
  else if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->get_attributes(sensorHandle, sensorAttributes);
  }
  return env->NewStringUTF(sensorAttributes.c_str());
}

void getSendRequestMsgInfo(JNIEnv *env, jobject obj, jobject jReqMsg, send_req_msg_info &out_send_reg)
{
  UNUSED_VAR(obj);

  sns_logi(" getSendRequestMsgInfo Start ");

  /*Get the object */
  jclass req_msg_class = env->GetObjectClass(jReqMsg);
  if(req_msg_class == NULL)
  {
    sns_loge("ReqMsgPayload is not found");
    return;
  }

  /*Message Name*/
  jfieldID fid = env->GetFieldID(req_msg_class, "msgName", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jMsgName = (jstring) env->GetObjectField(jReqMsg, fid);
  const char *msgNamePtr = env->GetStringUTFChars(jMsgName, NULL);
  if(nullptr != msgNamePtr) {
    string message_name(msgNamePtr);
    out_send_reg.msg_name = message_name;
  }
  sns_loge("getSendRequestMsgInfo msgNamePtr Done ");
  /*Message ID */
  fid = env->GetFieldID(req_msg_class, "msgID", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jMsgID = (jstring) env->GetObjectField(jReqMsg, fid);
  const char *msgIDPtr = env->GetStringUTFChars(jMsgID, NULL);
  string message_ID(msgIDPtr);
  out_send_reg.msgid = message_ID;


  /*SensorPayloadField[] */

  fid = env->GetFieldID(req_msg_class, "fields", "[Lcom/qualcomm/qti/usta/core/SensorPayloadField;");
  if (fid == 0) {
    sns_loge("getSendRequestMsgInfo:fields not found");
    return;
  }
  jobjectArray javaSensorPayLoadFieldsArray = reinterpret_cast<jobjectArray>(env->GetObjectField(jReqMsg, fid));
  int fieldCount = env->GetArrayLength(javaSensorPayLoadFieldsArray);


  for(int field_index = 0 ; field_index < fieldCount; field_index++)
  {

    jobject jfieldobject = reinterpret_cast<jobject>(env->GetObjectArrayElement(javaSensorPayLoadFieldsArray, field_index));
    send_req_field_info each_field;
    getFieldInfo(env, obj, jfieldobject, each_field);
    out_send_reg.fields.push_back(each_field);
  }

}

void getStdFieldsInfo(JNIEnv *env, jobject obj, jobject jSendReqStdFields, client_req_msg_fileds &std_field_info)
{
  UNUSED_VAR(obj);

  /*Get the object */
  jclass send_req_std_fields = env->GetObjectClass(jSendReqStdFields);
  if(send_req_std_fields == NULL)
  {
    sns_loge("send_req_std_fields is not found");
    return;
  }

  jfieldID fid = env->GetFieldID(send_req_std_fields, "batchPeriod", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jbatchPeriod = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jbatchPeriod != NULL)
  {
    const char *batchPeriodPtr = env->GetStringUTFChars(jbatchPeriod, NULL);
    string batch_period(batchPeriodPtr);
    std_field_info.batch_period = batch_period;
  }


  fid = env->GetFieldID(send_req_std_fields, "flushPeriod", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jFlushPeriod = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jFlushPeriod != NULL)
  {
    const char *flushPeriodPtr = env->GetStringUTFChars(jFlushPeriod, NULL);
    string flush_period(flushPeriodPtr);
    std_field_info.flush_period= flush_period;
  }

  fid = env->GetFieldID(send_req_std_fields, "clientType", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jclientType = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jclientType != NULL)
  {
    const char *clientTypePtr = env->GetStringUTFChars(jclientType, NULL);
    string client_Type(clientTypePtr);
    std_field_info.client_type= client_Type;
  }

  fid = env->GetFieldID(send_req_std_fields, "wakeupType", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jwakeupType = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jwakeupType != NULL)
  {
    const char *wakeupTypePtr = env->GetStringUTFChars(jwakeupType, NULL);
    string wakeup_type(wakeupTypePtr);
    std_field_info.wakeup_type= wakeup_type;
  }

  fid = env->GetFieldID(send_req_std_fields, "flushOnly", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jFlushOnly = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jFlushOnly != NULL)
  {
    const char *flushOnlyPtr = env->GetStringUTFChars(jFlushOnly, NULL);
    string flush_only(flushOnlyPtr);
    std_field_info.flush_only= flush_only;
  }

  fid = env->GetFieldID(send_req_std_fields, "maxBatch", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jmaxBatch = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jmaxBatch != NULL)
  {
    const char *maxBatchPtr = env->GetStringUTFChars(jmaxBatch, NULL);
    string max_batch(maxBatchPtr);
    std_field_info.max_batch= max_batch;
  }

  fid = env->GetFieldID(send_req_std_fields, "isPassive", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jisPassive = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jisPassive != NULL)
  {
    const char *isPassivePtr = env->GetStringUTFChars(jisPassive, NULL);
    string is_passive(isPassivePtr);
    std_field_info.is_passive= is_passive;
  }

  fid = env->GetFieldID(send_req_std_fields, "resamplerType", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jResamplerType = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jResamplerType != NULL)
  {
    const char *resamplerTypeptr = env->GetStringUTFChars(jResamplerType, NULL);
    string resampler_type(resamplerTypeptr);
    std_field_info.resampler_type= resampler_type;
  }

  fid = env->GetFieldID(send_req_std_fields, "resamplerFilter", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jResamplerFilter = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jResamplerFilter != NULL)
  {
    const char *resamplerFilterPtr = env->GetStringUTFChars(jResamplerFilter, NULL);
    string resampler_filter(resamplerFilterPtr);
    std_field_info.resampler_filter= resampler_filter;
  }


  fid = env->GetFieldID(send_req_std_fields, "thresholdType", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jThresholdType = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jThresholdType != NULL)
  {
    const char *thresholdTypePtr = env->GetStringUTFChars(jThresholdType, NULL);
    string threshold_type(thresholdTypePtr);
    std_field_info.threshold_type= threshold_type;
  }

  fid = env->GetFieldID(send_req_std_fields, "thresholdValueX", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jThresholdValueX = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jThresholdValueX != NULL)
  {
    const char *thresholdValueXPtr = env->GetStringUTFChars(jThresholdValueX, NULL);
    string threshold_value_x(thresholdValueXPtr);
    std_field_info.threshold_value.push_back(threshold_value_x);
  }

  fid = env->GetFieldID(send_req_std_fields, "thresholdValueY", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jThresholdValueY = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jThresholdValueY != NULL)
  {
    const char *thresholdValueYPtr = env->GetStringUTFChars(jThresholdValueY, NULL);
    string threshold_value_y(thresholdValueYPtr);
    std_field_info.threshold_value.push_back(threshold_value_y);
  }

  fid = env->GetFieldID(send_req_std_fields, "thresholdValueZ", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jThresholdValueZ = (jstring) env->GetObjectField(jSendReqStdFields, fid);
  if(jThresholdValueZ != NULL)
  {
    const char *thresholdValueZPtr = env->GetStringUTFChars(jThresholdValueZ, NULL);
    string threshold_value_z(thresholdValueZPtr);
    std_field_info.threshold_value.push_back(threshold_value_z);
  }

}
jobject setErrorCode(JNIEnv *env, jobject obj, usta_rc NativeErrorCode)
{
  UNUSED_VAR(obj);
  jclass errorCode_class = env->FindClass("com/qualcomm/qti/usta/core/NativeErrorCodes");
  if(errorCode_class == NULL)
  {
    sns_loge("errorCode_class is not found");
    return NULL;
  }

  jfieldID fid = 0;
  if(USTA_RC_NO_MEMORY == NativeErrorCode)
  {
    fid = env->GetStaticFieldID(errorCode_class, "USTA_MEMORY_ERROR" , "Lcom/qualcomm/qti/usta/core/NativeErrorCodes;");
  }
  else if(USTA_RC_NO_PROTO == NativeErrorCode)
  {
    fid = env->GetStaticFieldID(errorCode_class, "USTA_PROTOFILES_NOT_FOUND" , "Lcom/qualcomm/qti/usta/core/NativeErrorCodes;");
  }
  else if(USTA_RC_REQUIIRED_FILEDS_NOT_PRESENT == NativeErrorCode)
  {
    fid = env->GetStaticFieldID(errorCode_class, "USTA_REQUIRED_FIELDS_MISSING" , "Lcom/qualcomm/qti/usta/core/NativeErrorCodes;");
  }
  else if(USTA_RC_SUCCESS == NativeErrorCode)
  {
    fid = env->GetStaticFieldID(errorCode_class, "USTA_NO_ERROR" , "Lcom/qualcomm/qti/usta/core/NativeErrorCodes;");
  }
  if(fid == 0)
    return NULL;

  jobject errorCode_instance = env->GetStaticObjectField(errorCode_class, fid);


  return errorCode_instance;

}

jint sendRequest(JNIEnv *env, jobject obj, jobject inModeType, jint jSensorHandle, jobject jReqMsg , jobject jSendReqStdFields, jstring jlogFileName , jint jClientConnectId)
{
  UNUSED_VAR(obj);
  int clinet_connect_id = -1;
  sns_logi(" sendRequest Start ");
  string modeType = getModeType(env, obj, inModeType);

  int sensorHandle = jSensorHandle;
  int ClientConnectID = jClientConnectId;
  string recievedFileName ;
  if(jlogFileName != NULL)
  {
    const char * logFileName = env->GetStringUTFChars(jlogFileName, NULL);
    recievedFileName = logFileName;
  }
  else
  {
    recievedFileName = "";
  }

  client_req_msg_fileds std_field_info;
  getStdFieldsInfo(env, obj, jSendReqStdFields, std_field_info);

  send_req_msg_info reqMsg;
  getSendRequestMsgInfo(env, obj, jReqMsg, reqMsg);

  client_request request_info;
  if(-1 == ClientConnectID) {
    request_info.req_type = CREATE_CLIENT;
    if(modeType.compare("UI") == 0)
      request_info.cb_ptr.event_cb = sharedConnEventCallback;
    else
      request_info.cb_ptr.event_cb = nullptr;
  } else {
    request_info.req_type = USE_CLIENT_CONNECT_ID;
    request_info.client_connect_id = (int)jClientConnectId;
  }

  usta_rc NativeErrorCode = USTA_RC_SUCCESS;
  string usta_mode ;
  if(modeType.compare("UI") == 0)
  {
    sns_logi("sendRequest UI Mode it is ");
    usta_mode = "UI";
    clinet_connect_id = gSensorContextInstance[USTA_MODE_TYPE_UI]->send_request(request_info, sensorHandle, reqMsg, std_field_info, recievedFileName);
    sns_logi("sendRequest UI Mode it is with return %d",clinet_connect_id);
  }
  else if(modeType.compare("COMMAND_LINE") == 0)
  {
    sns_logi("sendRequest COMMAND_LINE Mode it is ");
    usta_mode = "command_line";
    clinet_connect_id = gSensorContextInstance[USTA_MODE_TYPE_COMMAND_LINE]->send_request(request_info, sensorHandle, reqMsg, std_field_info,recievedFileName);
    sns_logi("sendRequest COMMAND_LINE Mode it is with return %d", clinet_connect_id);
  } else {
    sns_loge("sendRequest Wrong mode it is");
  }

  sns_logi(" sendRequest End ");

  return clinet_connect_id;
}

void updateLoggingFlag(JNIEnv *env, jobject obj , jboolean is_logging_disabled)
{
  gSensorContextInstance[USTA_MODE_TYPE_UI]->update_logging_flag(is_logging_disabled);
}

jstring getSamplesFromNative(JNIEnv *env, jobject obj , jboolean jregistry_sensor)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);
  bool is_registry_sensor = (bool)jregistry_sensor;
  if(is_registry_sensor == true)
  {
    std::unique_lock<std::mutex> lock(g_registry_cv_mutex);
    if(registrySamplesBufferPool.empty()) {
      /* Should be conditional wait - Actual wait for sample to be ready */
      g_registry_cv_sample_ready.wait(lock );
    }
    /* Double check if unblocked due to UI change or not */
    else {
      jstring current_sample;
      string currentSample = registrySamplesBufferPool.back();
      registrySamplesBufferPool.pop_back();
      current_sample = env->NewStringUTF(currentSample.c_str());
      return current_sample;
    }
  }
  else if(is_registry_sensor == false)
  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    if(samplesBufferPool.empty()) {
      /* Should be conditional wait - Actual wait for sample to be ready */
      cv_sample_ready.wait(lock );
    }
    /* Double check if unblocked due to UI change or not */
    else {
      jstring current_sample;
      string currentSample = samplesBufferPool.back();
      samplesBufferPool.pop_back();
      current_sample = env->NewStringUTF(currentSample.c_str());
      return current_sample;
    }
  }
  return NULL;
}

void enableStreamingStatus(JNIEnv *env, jobject obj , jint sensorHandle)
{
  sns_logi(" enableStreamingStatus Start ");
  if(sensorHandle == -1)
    return;
  current_sensor_handle = (int)sensorHandle;
  sns_logi(" enableStreamingStatus End ");
}

void disableStreamingStatus(JNIEnv *env, jobject obj , jint sensorHandle)
{
  sns_logi(" disableStreamingStatus Start ");
  cv_sample_ready.notify_one();
  g_registry_cv_sample_ready.notify_one();
  if(sensorHandle == -1)
      return;

  current_sensor_handle = -1;
  samplesBufferPool.clear();
  registrySamplesBufferPool.clear();
  sns_logi(" disableStreamingStatus End ");
}

/*Implementations for Device mode feature start */

bool get_encoded_data(int mode_number, int mode_state, string &encoded_request)
{
  sns_logi("Current Device Mode number is %d " ,mode_number);
  sns_logi("Current Device Mode state is %d " ,mode_state);
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

void sns_device_mode_init()
{
  sns_logd("sns_device_mode_init Start");

  int nErr = AEE_SUCCESS;
  string uri = "file:///libsns_device_mode_skel.so?sns_device_mode_skel_handle_invoke&_modver=1.0";
  remote_handle64 remote_handle_fd = 0;
  remote_handle64 handle_l;
/*check for slpi or adsp*/
  if(sensors_target::is_slpi()) {
    uri +="&_dom=sdsp";
    if (remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=sdsp", &remote_handle_fd)) {
        sns_loge("failed to open remote handle for sensorspd - sdsp");
    }
  }
  else {
    uri +="&_dom=adsp";
    if (remote_handle64_open(ITRANSPORT_PREFIX "createstaticpd:sensorspd&_dom=adsp", &remote_handle_fd)) {
        sns_loge("failed to open remote handle for sensorspd - adsp\n");
    }
  }

  if (AEE_SUCCESS == (nErr = sns_device_mode_open(uri.c_str(), &handle_l))) {
    sns_logd("sns_device_mode_open success for sensorspd - handle_l is %ud\n" , (unsigned int)handle_l);
    device_mode_remote_handle = handle_l;
    is_device_mode_handle_opened = true;
  } else {
    device_mode_remote_handle = 0;
  }
  sns_logd("sns_device_mode_init End");
}

/*Return -1 on ereror case and 0 on success case*/
jint sendDeviceModeIndication(JNIEnv *env, jobject obj, jint device_mode_number , jint device_mode_state)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);

  sns_logd("sendDeviceModeIndication Start");
  uint32 result = 0;
  string encoded_request = "" ;
  if(false == get_encoded_data((int)device_mode_number,(int)device_mode_state, encoded_request)){
    sns_loge("sendDeviceModeIndication Error while encoding data ");
    return -1;
  }

  sns_device_mode_init();
  if(false == is_device_mode_handle_opened) {
    sns_loge("sendDeviceModeIndication Error  sns_device_mode_init failed");
    return -1;
  }

  sns_device_mode_set(device_mode_remote_handle, (const char*)encoded_request.c_str(), encoded_request.length(), &result);
  if(true == is_device_mode_handle_opened) {
    sns_device_mode_close(device_mode_remote_handle);
    is_device_mode_handle_opened = false;
  }
  sns_logd("sendDeviceModeIndication End");
  return 0;
}
/*Implementations for Device mode feature End */

jlong createDirectChannel(JNIEnv *env, jobject obj, jobject inModeType, jint channelType, jint clientType)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);
  sns_logi("createDirectChannel Start");

  create_channel_info create_info;
  unsigned long channel_handle = 0;
  usta_rc ret = USTA_RC_SUCCESS;

  string modeType = getModeType(env, obj, inModeType);

  char* shared_mem_buffer_ptr = NULL;
  shared_mem_buffer_ptr = (char*)rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM,RPCMEM_DEFAULT_FLAGS,DIRECT_CHANNEL_SHARED_MEMORY_SIZE);
  int fd = -1;
  fd = rpcmem_to_fd((void *)shared_mem_buffer_ptr);
  create_info.shared_memory_ptr = fd;
  create_info.shared_memory_size = DIRECT_CHANNEL_SHARED_MEMORY_SIZE;
  create_info.channel_type_value = (sensor_channel_type)channelType;
  create_info.client_type_value = (sensor_client_type)clientType;

  if(is_property_read == false) {
    char stats_debug[PROPERTY_VALUE_MAX];
    property_get("persist.vendor.sensors.debug.directChannelLogging", stats_debug, "false");
    sns_logi("is_direct_channel_logging: %s",stats_debug);
    if (!strncmp("true", stats_debug,4)) {
        sns_logi("is_direct_channel_logging : %s",stats_debug);
        is_direct_channel_logging_enabled = true;
    } else {
      is_direct_channel_logging_enabled = false;
    }
    is_property_read = false;
  }
  sns_logd("is_direct_channel_logging_enabled True before if condition");
  if(is_direct_channel_logging_enabled == true) {
    sns_logd("is_direct_channel_logging_enabled True");
    gDirectChannelFDArray.push_back(fd);
    gDirectChannelMempointer.push_back(shared_mem_buffer_ptr);
  }

  sns_logd("createDirectChannel IMapper memoery Created and fd is %d",fd );

  if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    ret = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->direct_channel_open(create_info, channel_handle);
  }

  if(USTA_RC_SUCCESS != ret) {
    sns_loge("createDirectChannel failed");
    return -1;
  }

  return channel_handle;

}

jint deleteDirectChannel(JNIEnv *env, jobject obj, jobject inModeType, jlong channelNumber)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);

  usta_rc ret = USTA_RC_SUCCESS;
  unsigned long channel_handle = channelNumber;


  string modeType = getModeType(env, obj, inModeType);

  if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    ret = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->direct_channel_close(channel_handle);
  }

  if(USTA_RC_SUCCESS != ret) {
    sns_loge("deleteDirectChannel failed");
    return -1;
  }

  return 0;

}

int64_t get_timestamp_offset()
{
  int64_t ts_offset;
  sensors_timeutil& timeutil = sensors_timeutil::get_instance();
  timeutil.recalculate_offset(true);
  ts_offset = timeutil.getElapsedRealtimeNanoOffset();
  return ts_offset;
}

jint timeStampOffsetUpdate(JNIEnv *env, jobject obj, jobject inModeType, jlong channelNumber)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);

  usta_rc ret = USTA_RC_SUCCESS;
  unsigned long channel_handle = channelNumber;
  string modeType = getModeType(env, obj, inModeType);

  int64_t ts_offset = get_timestamp_offset();

  if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    ret = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->direct_channel_update_offset_ts(channel_handle, ts_offset);
  }

  if(USTA_RC_SUCCESS != ret) {
    sns_loge("timeStampOffsetUpdate failed");
    return -1;
  }
  return 0;

}

jint sendRemoveConfigRequest(JNIEnv *env, jobject obj, jobject inModeType, jlong channelNumber, jint sensorHandle, jboolean iscalibrated, jboolean isResampled )
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);

  usta_rc ret = USTA_RC_SUCCESS;
  unsigned long channel_handle = channelNumber;
  unsigned long sensor_handle = sensorHandle;

  string modeType = getModeType(env, obj, inModeType);

  direct_channel_delete_req_info delete_req_info;
  delete_req_info.sensor_handle = sensor_handle;
  if((bool)iscalibrated == true ) {
    delete_req_info.is_calibrated = "true";
  } else if ((bool)iscalibrated == false) {
    delete_req_info.is_calibrated = "false";
  } else {
    delete_req_info.is_calibrated = "false";
  }

  if((bool)isResampled == true ) {
    delete_req_info.is_fixed_rate = "true";
  } else if ((bool)isResampled == false) {
    delete_req_info.is_fixed_rate = "false";
  } else {
    delete_req_info.is_fixed_rate = "false";
  }

  if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    ret = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->direct_channel_delete_request(channel_handle, delete_req_info);
  }

  if(USTA_RC_SUCCESS != ret) {
    sns_loge("deleteDirectChannel failed");
    return -1;
  }
  return 0;
}

void getDirectChannelStandReqInfo(JNIEnv *env, jobject obj, jobject stdFields, direct_channel_std_req_info &direct_channel_std_req_fields_info)
{
  UNUSED_VAR(env);

  /*Get the object */
  jclass direct_channel_std_req_fields = env->GetObjectClass(stdFields);
  if(direct_channel_std_req_fields == NULL)
  {
    sns_loge("direct_channel_std_req_fields is not found");
    return;
  }

  jfieldID fid = env->GetFieldID(direct_channel_std_req_fields, "SensorHandle", "I");
  if (fid == 0)
    return;
  jint jSensorHandle = env->GetIntField(stdFields, fid);
  direct_channel_std_req_fields_info.sensor_handle = (int)jSensorHandle;
  sns_logd("getDirectChannelStandReqInfo  sensorHandle %d", direct_channel_std_req_fields_info.sensor_handle);


  fid = env->GetFieldID(direct_channel_std_req_fields, "isCalibratead", "Z");
  if (fid == 0)
    return;
  jboolean jIsCalibrated = env->GetBooleanField(stdFields, fid);
  if((bool)jIsCalibrated == true ) {
    direct_channel_std_req_fields_info.is_calibrated = "true";
  } else if ((bool)jIsCalibrated == false) {
    direct_channel_std_req_fields_info.is_calibrated = "false";
  }

  fid = env->GetFieldID(direct_channel_std_req_fields, "isResampled", "Z");
  if (fid == 0)
    return;
  jboolean jIsResampled = env->GetBooleanField(stdFields, fid);
  if((bool)jIsResampled == true ) {
    direct_channel_std_req_fields_info.is_fixed_rate = "true";
  } else if ((bool)jIsResampled == false) {
    direct_channel_std_req_fields_info.is_fixed_rate = "false";
  }

  fid = env->GetFieldID(direct_channel_std_req_fields, "batchPeriod", "Ljava/lang/String;");
  if (fid == 0)
    return;
  jstring jbatchPeriod = (jstring) env->GetObjectField(stdFields, fid);
  if(jbatchPeriod != NULL)
  {
    const char *batchPeriodPtr = env->GetStringUTFChars(jbatchPeriod, NULL);
    string batch_period(batchPeriodPtr);
    direct_channel_std_req_fields_info.batch_period = batch_period;
  }
}

jint sendConfigRequest(JNIEnv *env, jobject obj, jobject inModeType, jlong channelNumber, jobject stdFields, jobject reqMsg)
{
  UNUSED_VAR(env);
  UNUSED_VAR(obj);
  unsigned long channel_number = channelNumber;
  string modeType = getModeType(env, obj, inModeType);
  direct_channel_std_req_info std_req_info;
  send_req_msg_info sensor_payload_field;
  usta_rc ret = USTA_RC_SUCCESS;
  getDirectChannelStandReqInfo(env, obj, stdFields, std_req_info);
  getSendRequestMsgInfo(env, obj, reqMsg, sensor_payload_field);

  if(modeType.compare("DIRECT_CHANNEL_UI") == 0)
  {
    ret = gSensorContextInstance[USTA_MODE_TYPE_DIRECT_CHANNEL_UI]->direct_channel_send_request(channel_number, std_req_info, sensor_payload_field);
  }

  if(USTA_RC_SUCCESS != ret) {
    sns_loge("sendDirectChannel failed");
    return -1;
  }

  return 0;
}

/*
 * Class and package name
 * */
static const char *classPathName = "com/qualcomm/qti/usta/core/SensorContextJNI";

/*
 * List of native methods
 *
 * */
static JNINativeMethod methods[] = {
    {"getSensorList" , "(Lcom/qualcomm/qti/usta/core/ModeType;)[Lcom/qualcomm/qti/usta/core/SensorListInfo;", (void *)getSensorList},
    {"getClientProcessors" , "(Lcom/qualcomm/qti/usta/core/ModeType;)[Ljava/lang/String;", (void *)getClientProcessors},
    {"getWakeupDeliveryType" , "(Lcom/qualcomm/qti/usta/core/ModeType;)[Ljava/lang/String;", (void *)getWakeupDeliveryType},
    {"getResamplerType" , "(Lcom/qualcomm/qti/usta/core/ModeType;)[Ljava/lang/String;", (void *)getResamplerType},
    {"getThresholdType" , "(Lcom/qualcomm/qti/usta/core/ModeType;)[Ljava/lang/String;", (void *)getThresholdType},
    {"removeSensors" , "(Lcom/qualcomm/qti/usta/core/ModeType;[I)V", (void *)removeSensors},
    {"getRequestMessages" , "(Lcom/qualcomm/qti/usta/core/ModeType;I)[Lcom/qualcomm/qti/usta/core/ReqMsgPayload;", (void *)getRequestMessages},
    {"getSensorAttributes" , "(Lcom/qualcomm/qti/usta/core/ModeType;I)Ljava/lang/String;", (void *)getSensorAttributes},
    {"sendRequest" , "(Lcom/qualcomm/qti/usta/core/ModeType;ILcom/qualcomm/qti/usta/core/ReqMsgPayload;Lcom/qualcomm/qti/usta/core/SendReqStdFields;Ljava/lang/String;I)I", (void *)sendRequest},
    {"updateLoggingFlag" , "(Z)V", (void *)updateLoggingFlag},
    {"enableStreamingStatus" , "(I)V", (void *)enableStreamingStatus},
    {"disableStreamingStatus" , "(I)V", (void *)disableStreamingStatus},
    {"getSamplesFromNative" , "(Z)Ljava/lang/String;", (void *)getSamplesFromNative},
    {"sendDeviceModeIndication" , "(II)I", (void *)sendDeviceModeIndication},
    {"createDirectChannel", "(Lcom/qualcomm/qti/usta/core/ModeType;II)J", (void *)createDirectChannel},
    {"deleteDirectChannel", "(Lcom/qualcomm/qti/usta/core/ModeType;J)I", (void *)deleteDirectChannel},
    {"timeStampOffsetUpdate", "(Lcom/qualcomm/qti/usta/core/ModeType;J)I", (void *)timeStampOffsetUpdate},
    {"sendRemoveConfigRequest", "(Lcom/qualcomm/qti/usta/core/ModeType;JIZZ)I", (void *)sendRemoveConfigRequest},
    {"sendConfigRequest", "(Lcom/qualcomm/qti/usta/core/ModeType;JLcom/qualcomm/qti/usta/core/DirectChannelStdFields;Lcom/qualcomm/qti/usta/core/ReqMsgPayload;)I", (void *)sendConfigRequest},
};


/*
 * Register several native methods for one class.
 *
 *
 * */
static int registerUSTANativeMethods(JNIEnv* envVar, const char* inClassName, JNINativeMethod* inMethodsList, int inNumMethods)
{
  jclass javaClazz = envVar->FindClass(inClassName);
  if (javaClazz == NULL)
  {
    return JNI_FALSE;
  }
  if (envVar->RegisterNatives(javaClazz, inMethodsList, inNumMethods) < 0)
  {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 *
 * Returns JNI_TRUE on success
 *
 * */
static int registerNatives(JNIEnv* env)
{
  if (!registerUSTANativeMethods(env, classPathName, methods, sizeof(methods) / sizeof(methods[0])))
  {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * This is called by the VM when the shared library is first loaded.
 * */
typedef union
{
  JNIEnv* env;
  void* venv;
}UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  UNUSED_VAR(reserved);
  UnionJNIEnvToVoid uenv;
  uenv.venv = NULL;
  JNIEnv* env = NULL;
  sns_logi("JNI_OnLoad Start");

  if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
  {
    sns_loge("ERROR: GetEnv failed");
    return -1;
  }
  env = uenv.env;

  if (registerNatives(env) != JNI_TRUE)
  {
    sns_loge("ERROR: registerNatives failed");
    return -1;
  }
  sns_logi("JNI_OnLoad End");

  return JNI_VERSION_1_4;
}
