#pragma once
/*
*	sns_ct7117x_sensor_instance.h
*	Sensor instance data types forthe driver
*
*/

#include <stdint.h>
#include "sns_sensor_instance.h"
#include "sns_data_stream.h"
#include "sns_com_port_types.h"
#include "sns_time.h"
#include "sns_com_port_types.h"
#include "sns_sync_com_port_service.h"
#include "sns_sensor_uid.h"
#include "sns_async_com_port.pb.h"
#include "sns_math_util.h"
#include "sns_interrupt.pb.h"
#include "sns_std_sensor.pb.h"
#include "sns_physical_sensor_test.pb.h"
#include "sns_diag_service.h"

/** Forward Declaration of Instance API */
extern sns_sensor_instance_api ct7117x_sensor_instance_api;


#ifndef s8
#define s8                                  int8_t
#define u8                                  uint8_t
#define s16                                 int16_t
#define u16                                 uint16_t
#define s32                                 int32_t
#define u32                                 uint32_t
typedef int64_t                             s64;/**< used for signed 64bit */
typedef uint64_t                            u64;/**< used for signed 64bit */
#endif

/* physical COM port structure */
typedef struct ct7117x_com_port_info
{
  sns_com_port_config        com_config;
  sns_sync_com_port_handle  *port_handle;
} ct7117x_com_port_info;
/*sensor type*/
typedef enum
{
  TEMP_TEMPERATURE = 0x2,
  TEMP_SENSOR_INVALID = 0xFF
} ct7117x_sensor_type;
/*config state*/
typedef enum
{
  TEMP_CONFIG_IDLE,            		/** not configuring */
  TEMP_CONFIG_POWERING_DOWN,   		/** cleaning up when no clients left */
  TEMP_CONFIG_STOPPING_STREAM,		/** stream stop initiated, waiting for completion */
  TEMP_CONFIG_FLUSH_HW,     	/** FIFO flush initiated, waiting for completion */
  TEMP_CONFIG_UPDATING_HW      		/** updating sensor HW, when done goes back to IDLE */
} ct7117x_config_step;
/*self_test info*/
typedef struct temp_self_test_info
{
  sns_physical_sensor_test_type test_type;
  bool test_client_present;
} temp_self_test_info;


/*!
* low level operation mode
* */
typedef enum
{
  SLEEP_MODE,         /* sleep mode      : no measurements are performed*/
  FORCED_MODE,        /* forced mode     :single TPHG cycle is performed; sensor automatically returns to sleep mode afterwards*/
  SEQUENTIAL_MODE,    /* sequential mode :TPHG measurements are performed continuously until mode change; Between each cycle,
                                the sensor enters stand-by for a period of time according to the odr control register*/
  PARALLEL_MODE,      /* parallel mode   : TPHG measurements are performed continuously until mode change;
                                 no stand-by occurs between consecutive TPHG cycles*/
  MAX_NUM_OP_MODE,    /* the max number of operation mode */
  INVALID_WORK_MODE = MAX_NUM_OP_MODE, /* invalid mode */
} ct7117x_power_mode;

typedef struct ct7117x_sensor_deploy_info
{
  /** Determines which Sensor data to publish. Uses
   *  temp_sensor_type as bit mask. */
  uint8_t           publish_sensors;
  uint8_t           enable;
} ct7117x_sensor_deploy_info;


typedef struct ct7117x_sensor_cfg_info
{
  float             desired_odr;
  float             curr_odr;
  sns_sensor_uid    suid;
  uint64_t          trigger_num;
  sns_time          timeout_ticks; /* derived from the odr */
  sns_time          expection_timeout_ticks_derived_from_odr;
  bool              timer_is_active;
  uint32_t          report_timer_hz;
  float             report_rate_hz;
  float             sampling_rate_hz;
  sns_time          sampling_intvl;
  sns_time          expect_time;
  temp_self_test_info  test_info;
} ct7117x_sensor_cfg_info;

/* async port for the data stream which use the COM port handle */
typedef struct ct7117x_async_com_port_info {
  uint32_t port_handle;
} ct7117x_async_com_port_info;

struct ct7117x_calib_param_t{
	u16 dig_T1;/**<calibration T1 data*/ 
	s16 dig_T2;/**<calibration T2 data*/  
	s16 dig_T3;/**<calibration T3 data*/ 
	s32 t_fine;/**<calibration t_fine data*/
};

/** Private state. */
typedef struct ct7117x_instance_state
{
  /** -- sensor configuration details --*/
  /** temperature HW config details */
  ct7117x_sensor_cfg_info temperature_info;
  ct7117x_sensor_deploy_info   deploy_info;
  /** COM port info */
  ct7117x_com_port_info com_port_info;
  /**--------Async Com Port--------*/
  sns_async_com_port_config  ascp_config;
  ct7117x_config_step       config_step;
  sns_time                       interrupt_timestamp;
  /** Data streams from dependentcies. */
  sns_sensor_uid                 timer_suid;
  sns_sensor_uid                 irq_suid;
  uint32_t                       irq_num;
  sns_data_stream                *temperature_timer_data_stream;
  sns_data_stream                *interrupt_data_stream;
  sns_data_stream                *async_com_port_data_stream;  /* data streaming channel */
  /* request/configure stream */
  uint32_t                       client_req_id;
  sns_std_sensor_config          temp_req;   /* stream for the configure */
  size_t                         encoded_imu_event_len;
  sns_diag_service               *diag_service;  /* for diagnostic to print debug message */
  sns_sync_com_port_service      *scp_service;
  ct7117x_power_mode              op_mode;
  uint32_t                       interface;
  sns_rc (* com_read)(
     sns_sync_com_port_service *scp_service,
      sns_sync_com_port_handle *port_handle,
      uint32_t rega,
      uint8_t  *regv,
      uint32_t bytes,
      uint32_t *xfer_bytes);
  sns_rc (* com_write)(
      sns_sync_com_port_service *scp_service,
      sns_sync_com_port_handle *port_handle,
      uint32_t rega,
      uint8_t  *regv,
      uint32_t bytes,
      uint32_t *xfer_bytes,
      bool save_write_time);
  bool  instance_is_ready_to_configure;
  uint8_t              enabled_sensors;
  bool                 is_dri;
  bool new_self_test_request;
  struct ct7117x_calib_param_t calib_param;/**<calibration data*/  
  u8 oversamp_temperature;/**< temperature over sampling*/
  size_t           log_raw_encoded_size;
} ct7117x_instance_state;


typedef struct sns_temp_cfg_req {
  float               sample_rate;
  float               report_rate;
  ct7117x_sensor_type  sensor_type;
  ct7117x_power_mode      op_mode;
} sns_temp_cfg_req;

void ct7117x_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp);
sns_rc ct7117x_temp_inst_init(sns_sensor_instance *const this,
    sns_sensor_state const *sstate);
sns_rc ct7117x_temp_inst_deinit(sns_sensor_instance *const this);
sns_rc ct7117x_temp_inst_set_client_config(
     sns_sensor_instance * const this,
    sns_request const *client_request);
