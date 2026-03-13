#pragma once
/*
* sns_ct7117x_hal.h
* Hardware-specific data types for driver
*
*/

#include <stdint.h>

#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#define TEMP_GET_BITSLICE(regvar, bitname)\
  ((regvar & bitname##__MSK) >> bitname##__POS)

#define TEMP_SET_BITSLICE(regvar, bitname, val)\
  ((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/* Constants */
#define TEMP_NULL                          (0)
#define TEMP_RETURN_FUNCTION_TYPE          s8
/* right shift definitions*/
#define TEMP_SHIFT_BIT_POSITION_BY_01_BIT         (1)
#define TEMP_SHIFT_BIT_POSITION_BY_02_BITS      (2)
#define TEMP_SHIFT_BIT_POSITION_BY_03_BITS      (3)
#define TEMP_SHIFT_BIT_POSITION_BY_04_BITS      (4)
#define TEMP_SHIFT_BIT_POSITION_BY_05_BITS      (5)
#define TEMP_SHIFT_BIT_POSITION_BY_08_BITS      (8)
#define TEMP_SHIFT_BIT_POSITION_BY_11_BITS      (11)
#define TEMP_SHIFT_BIT_POSITION_BY_12_BITS      (12)
#define TEMP_SHIFT_BIT_POSITION_BY_13_BITS      (13)
#define TEMP_SHIFT_BIT_POSITION_BY_14_BITS      (14)
#define TEMP_SHIFT_BIT_POSITION_BY_15_BITS      (15)
#define TEMP_SHIFT_BIT_POSITION_BY_16_BITS      (16)

/* numeric definitions */
#define  TEMP_TEMPERATURE_CALIB_DATA_LENGTH    (2)
#define  TEMP_GEN_READ_WRITE_DATA_LENGTH      (2)
#define  TEMP_REGISTER_READ_DELAY        (1)
#define  TEMP_TEMPERATURE_DATA_LENGTH        (2)
#define  TEMP_INIT_VALUE_MSB          (0x19)
#define  TEMP_INIT_VALUE_LSB          (0x00)
#define  TEMP_INIT_CONFIG_VALUE      (0xc2)
#define  TEMP_CHIP_ID_READ_SUCCESS        (0)
#define  TEMP_CHIP_ID_READ_FAIL        ((s8)-1)
#define  TEMP_INVALID_DATA          (0)

/************************************************/
/**\name  ERROR CODES      */
/************************************************/
#define  SUCCESS      ((u8)0)
#define E_TEMP_NULL_PTR         ((s8)-127)
#define E_TEMP_COMM_RES         ((s8)-1)
#define E_TEMP_OUT_OF_RANGE     ((s8)-2)
#define ERROR                     ((s8)-1)
/************************************************/
/************************************************/

/************************************************/
/**\name  POWER MODE DEFINITION       */
/***********************************************/
/* Sensor Specific constants */
#define TEMP_SLEEP_MODE                    (0x00)
#define TEMP_FORCED_MODE                   (0x01)
#define TEMP_NORMAL_MODE                   (0x02)

/************************************************/
/**\name  STANDBY TIME DEFINITION       */
/***********************************************/
#define TEMP_STANDBY_TIME_1_MS              (0x00)
#define TEMP_STANDBY_TIME_63_MS             (0x01)
#define TEMP_STANDBY_TIME_125_MS            (0x02)
#define TEMP_STANDBY_TIME_250_MS            (0x03)
#define TEMP_STANDBY_TIME_500_MS            (0x04)
#define TEMP_STANDBY_TIME_1000_MS           (0x05)
#define TEMP_STANDBY_TIME_2000_MS           (0x06)
#define TEMP_STANDBY_TIME_4000_MS           (0x07)
/************************************************/
/**\name  OVERSAMPLING DEFINITION       */
/***********************************************/
#define TEMP_OVERSAMP_SKIPPED          (0x00)
#define TEMP_OVERSAMP_1X               (0x01)
#define TEMP_OVERSAMP_2X               (0x02)
#define TEMP_OVERSAMP_4X               (0x03)
#define TEMP_OVERSAMP_8X               (0x04)
#define TEMP_OVERSAMP_16X              (0x05)
/************************************************/
/**\name  WORKING MODE DEFINITION       */
/***********************************************/
#define TEMP_LOW_POWER_MODE               (0x00)
#define TEMP_STANDARD_RESOLUTION_MODE      (0x01)
#define TEMP_HIGH_RESOLUTION_MODE          (0x02)

#define TEMP_ULTRALOWPOWER_OVERSAMP_TEMPERATURE       TEMP_OVERSAMP_1X

#define TEMP_LOWPOWER_OVERSAMP_TEMPERATURE           TEMP_OVERSAMP_1X

#define TEMP_STANDARDRESOLUTION_OVERSAMP_TEMPERATURE  TEMP_OVERSAMP_1X

#define TEMP_HIGHRESOLUTION_OVERSAMP_TEMPERATURE      TEMP_OVERSAMP_1X
/************************************************/
/**\name  DELAY TIME DEFINITION       */
/***********************************************/
#define T_INIT_MAX          (20)
/* 20/16 = 1.25 ms */
#define T_MEASURE_PER_OSRS_MAX        (37)
/* 37/16 = 2.3125 ms*/
/************************************************/
/**\name  CALIBRATION PARAMETERS DEFINITION       */
/***********************************************/
/*calibration parameters */
#define TEMP_TEMPERATURE_CALIB_DIG_T1_LSB_REG             (0x00)            //CT7117

/************************************************/
/**\name  REGISTER ADDRESS DEFINITION       */
/***********************************************/
#define TEMP_CHIP_ID_REG                   (0x07)  /*Chip ID Register */
#define TEMP_RST_REG                       (0x01) /*Softreset Register */
#define TEMP_STAT_REG                      (0x01)  /*Status Register */
#define TEMP_CTRL_MEAS_REG                 (0x01)
#define TEMP_CONFIG_REG                    (0x01)  /*Configuration Register */
#define TEMP_TEMPERATURE_REG               (0x00)  /*Temperature MSB Reg */

/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION      */
/***********************************************/
/* Status Register */
#define TEMP_STATUS_REG_MEASURING__POS           (3)
#define TEMP_STATUS_REG_MEASURING__MSK           (0x08)
#define TEMP_STATUS_REG_MEASURING__LEN           (1)
#define TEMP_STATUS_REG_MEASURING__REG           (TEMP_STAT_REG)

#define TEMP_STATUS_REG_IM_UPDATE__POS            (0)
#define TEMP_STATUS_REG_IM_UPDATE__MSK            (0x01)
#define TEMP_STATUS_REG_IM_UPDATE__LEN            (1)
#define TEMP_STATUS_REG_IM_UPDATE__REG           (TEMP_STAT_REG)
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR TEMPERATURE OVERSAMPLING */
/***********************************************/
/* Control Measurement Register */
#define TEMP_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__POS             (5)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__MSK             (0xE0)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__LEN             (3)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_TEMPERATURE__REG             \
(TEMP_CTRL_MEAS_REG)
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR PRESSURE OVERSAMPLING */
/***********************************************/
#define TEMP_CTRL_MEAS_REG_OVERSAMP_PRESSURE__POS             (2)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_PRESSURE__MSK             (0x1C)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_PRESSURE__LEN             (3)
#define TEMP_CTRL_MEAS_REG_OVERSAMP_PRESSURE__REG             \
(TEMP_CTRL_MEAS_REG)
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR POWER MODE */
/***********************************************/
#define TEMP_CTRL_MEAS_REG_POWER_MODE__POS              (0)
#define TEMP_CTRL_MEAS_REG_POWER_MODE__MSK              (0x03)
#define TEMP_CTRL_MEAS_REG_POWER_MODE__LEN              (2)
#define TEMP_CTRL_MEAS_REG_POWER_MODE__REG             (TEMP_CTRL_MEAS_REG)
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR STANDBY DURATION */
/***********************************************/
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR IIR FILTER */
/***********************************************/
#define TEMP_CONFIG_REG_FILTER__POS              (2)
#define TEMP_CONFIG_REG_FILTER__MSK              (0x1C)
#define TEMP_CONFIG_REG_FILTER__LEN              (3)
#define TEMP_CONFIG_REG_FILTER__REG              (TEMP_CONFIG_REG)

/* Configuration Register */
#define TEMP_CONFIG_REG_STANDBY_DURN__POS                 (5)
#define TEMP_CONFIG_REG_STANDBY_DURN__MSK                 (0xE0)
#define TEMP_CONFIG_REG_STANDBY_DURN__LEN                 (3)
#define TEMP_CONFIG_REG_STANDBY_DURN__REG                 (TEMP_CONFIG_REG)
/************************************************/
/**\name  BIT LENGTH,POSITION AND MASK DEFINITION
FOR PRESSURE AND TEMPERATURE DATA REGISTERS */
/***********************************************/
#define TEMP_TEMPERATURE_XLSB_REG_DATA__POS      (4)
#define TEMP_TEMPERATURE_XLSB_REG_DATA__MSK      (0xF0)
#define TEMP_TEMPERATURE_XLSB_REG_DATA__LEN      (4)
#define TEMP_TEMPERATURE_XLSB_REG_DATA__REG      (TEMP_TEMPERATURE_XLSB_REG)
/************************************************/
/**\name  BUS READ AND WRITE FUNCTION POINTERS */
/***********************************************/
#define TEMP_WR_FUNC_PTR  s8 (*bus_write)(u8, u8, u8 *, u8)

#define TEMP_RD_FUNC_PTR  s8 (*bus_read)(u8, u8, u8 *, u8)

#define TEMP_MDELAY_DATA_TYPE u32
/****************************************************/
/**\name  DEFINITIONS FOR ARRAY SIZE OF DATA   */
/***************************************************/
#define  TEMP_TEMPERATURE_DATA_SIZE    (2)
#define  TEMP_FUNCTION_DATA_SIZE    (2)
#define  TEMP_CALIB_DATA_SIZE      (25)

#define  TEMP_TEMPERATURE_MSB_DATA    (0)
#define  TEMP_TEMPERATURE_LSB_DATA    (1)
#define  TEMP_TEMPERATURE_XLSB_DATA    (2)

/****************************************************/
/**\name  ARRAY PARAMETER FOR CALIBRATION     */
/***************************************************/
#define  TEMP_TEMPERATURE_CALIB_DIG_T1_LSB  (1)
#define  TEMP_TEMPERATURE_CALIB_DIG_T1_MSB  (0)
/**
 *  Address registers
 */

/** Default values loaded in probe function */
#define TEMP_WHOAMI_VALUE              (0x59)  /** Who Am I default value */

/** Off to idle time */
#define TEMP_OFF_TO_IDLE_MS      250  //ms
//#define TEMP_OFF_TO_IDLE_MS      5  //ms
#define TEMP_NUM_AXES                            1

sns_rc ct7117x_reset_device(
  sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  ct7117x_sensor_type sensor);

void ct7117x_send_config_event(sns_sensor_instance * const instance);
sns_rc ct7117x_get_who_am_i(
  sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  uint8_t *buffer);

void ct7117x_start_sensor_temp_polling_timer(sns_sensor_instance *this);
void ct7117x_reconfig_hw(sns_sensor_instance *this, ct7117x_sensor_type sensor_type);
void ct7117x_convert_and_send_temp_sample(
  sns_sensor_instance *const instance,
  sns_time            timestamp,
  const uint8_t       data[2]);
sns_rc ct7117x_read_cal_params(sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  uint8_t *buffer);
void ct7117x_set_temperature_config(sns_sensor_instance *const this);
void ct7117x_set_pressure_polling_config(sns_sensor_instance *const this);
/**
   * Executes requested self-tests.
   *
   * @param instance   reference to the instace
   *
   * @return none
   */
void ct7117x_run_self_test(sns_sensor_instance *instance);
void ct7117x_handle_temperature_data_stream_timer_event(sns_sensor_instance * const instance);
TEMP_RETURN_FUNCTION_TYPE ct7117x_set_work_mode(ct7117x_instance_state *state,u8 v_work_mode_u8);
TEMP_RETURN_FUNCTION_TYPE ct7117x_get_calib_param(sns_sensor * const this);
