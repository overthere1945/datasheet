/*============================================================================
  Copyright (c) 2017-2018 , 2021-2023 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file sensor_attributes.h
  @brief
   sensor_attributes class header.
============================================================================*/
#pragma once

#include "sensor_common.h"
/**
 * Following attributes are currently queried by USTA for a given
 * sensor. These strings must bear the same name as the enum name
 * defined in sns_std_sensor.proto. This list will be updated on
 * demand is any attribute query is required
 */

  // OPTIONAL
  // [Float]
  // Supported sample rates in Hz
char const sns_std_sensor_attrid_rates[] ="SNS_STD_SENSOR_ATTRID_RATES";

  // REQUIRED
  // String
  // Human-readable vendor name
char const sns_std_sensor_attrid_vendor[] ="SNS_STD_SENSOR_ATTRID_VENDOR";

  // REQUIRED
  // String
  // Data Type used by this Sensor
char const sns_std_sensor_attrid_type[]   ="SNS_STD_SENSOR_ATTRID_TYPE";

  // REQUIRED
  // [String]
  // .proto files specifying the incoming request and outgoing event messages
char const sns_std_sensor_attrid_api[]    ="SNS_STD_SENSOR_ATTRID_API";

#define MAX_ATTRIBUTE_COUNT_LIMIT 100

typedef struct sensors_attribute
{
  int32  attribute_id;
  std::string encoded_attribute;     // encoded attribute message for each attribute
}sensors_attribute;

typedef struct sensors_attrib_list
{
  int num_attrib;
  sensors_attribute attributes[MAX_ATTRIBUTE_COUNT_LIMIT];
}sensors_attrib_list;

/*
 * sensor_attribute class is used by each sensor instance, to store its
 *      attributes
 *
 **/
class sensor_attribute
{
public :
  sensor_attribute();
  ~sensor_attribute();
  /*
   * @brief used to store the sensors attributes in to member variable "attribute_list"
   *
   * @param[i] attribute event information in pb encoded form, mapping to sns_std_attr_event
   **/
  usta_rc store(std::string& payload);

  /* return the number of attributes for a particular sensor*/
  inline size_t get_count();
  /*
   * @brief check whether particular attribute is available or not
   *
   * @param[i] attribute name in string format
   *
   * @param[o] return true if it is available.
   **/
  bool is_available(std::string attrib);
  /*
   * returns the data type of the sensor
   **/
  std::string get_dataType();

  /*
   * returns vendor name of the sensor
   **/
  std::string get_vendor();

  /*
   * Returns list of proto files for this sensor.
   *
   *  It is mapped to SNS_STD_SENSOR_ATTRID_API
   *
   **/
  std::vector<std::string> get_API();

  /*
   * Returns max supported sample rate.
   *
   *  It is mapped to SNS_STD_SENSOR_ATTRID_RATES
   *
   **/
  float get_max_sample_rate();

  /*
  *Returns the max sample rate calculated from all attributes
  */
  static float get_max_combined_sample_rate() {return max_combined_sample_rate;}

private:
  size_t attribute_count;
  static float max_combined_sample_rate;

  // all the sensors attribute information is stored  in below variable
  sensors_attrib_list attribute_list;

  // it will delete internal memory used for attributes information
  void delete_memory();

  // creates the memory required for storing attributes information.
  usta_rc create_memory();
};
