/*
*	sns_ct7117x_hal_island.c
*	Hardware-specific functions for normal mode
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include "sns_rc.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_event_service.h"
#include "sns_mem_util.h"
#include "sns_math_util.h"
#include "sns_service_manager.h"
#include "sns_com_port_types.h"
#include "sns_sync_com_port_service.h"
#include "sns_types.h"

#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#include "sns_async_com_port.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"

#include "sns_std_sensor.pb.h"

#include "sns_diag_service.h"
#include "sns_std.pb.h"
#include "sns_diag.pb.h"



/*==============================================================================
  Type definitions
  ============================================================================*/
typedef struct log_sensor_state_raw_info
{
  /* Pointer to diag service */
  sns_diag_service *diag;
  /* Pointer to sensor instance */
  sns_sensor_instance *instance;
  /* Pointer to sensor UID*/
  struct sns_sensor_uid *sensor_uid;
  /* Size of a single encoded sample */
  size_t encoded_sample_size;
  /* Pointer to log*/
  void *log;
  /* Size of allocated space for log*/
  uint32_t log_size;
  /* Number of actual bytes written*/
  uint32_t bytes_written;
  /* Number of batch samples written*/
  /* A batch may be composed of several logs*/
  uint32_t batch_sample_cnt;
  /* Number of log samples written*/
  uint32_t log_sample_cnt;
} log_sensor_state_raw_info;

// Unencoded batch sample
typedef struct
{
  /* Batch Sample type as defined in sns_diag.pb.h */
  sns_diag_batch_sample_type sample_type;
  /* Timestamp of the sensor state data sample */
  sns_time timestamp;
  /*Raw sensor state data sample*/
  float sample[TEMP_NUM_AXES];
  /* Data status.*/
  sns_std_sensor_sample_status status;
} temp_batch_sample;


/*==============================================================================
  Function Definitions
  ============================================================================*/

/*!
 *  @brief This API is used to write
 *   the working mode of the sensor
 *  @param v_work_mode_u8 : The value of work mode
 *   value      |  mode
 * -------------|-------------
 *    0         | TEMP_LOW_POWER_MODE
 *    1         | TEMP_STANDARD_RESOLUTION_MODE
 *    2         | TEMP_HIGH_RESOLUTION_MODE
 *  @return results of bus communication function
 *  @retval 0 -> Success
 *  @retval -1 -> Error
 */
TEMP_RETURN_FUNCTION_TYPE ct7117x_set_work_mode(
  ct7117x_instance_state *state,
  u8 v_work_mode_u8)
{
  /* variable used to return communication result*/
  TEMP_RETURN_FUNCTION_TYPE com_rslt = ERROR;
  u8 a_data_u8r[TEMP_FUNCTION_DATA_SIZE] = {0x00,
			TEMP_INIT_CONFIG_VALUE};

  u32 xfer_bytes;
  /* check the p_TEMP structure pointer as NULL*/
  if (state == TEMP_NULL) 
  {
    com_rslt = E_TEMP_NULL_PTR;
  } 
  else 
  {
  	if (v_work_mode_u8 <= TEMP_HIGH_RESOLUTION_MODE) 
  	{
   		com_rslt = state->com_read(
        state->scp_service,
        state->com_port_info.port_handle,
        TEMP_CTRL_MEAS_REG, a_data_u8r,
        TEMP_GEN_READ_WRITE_DATA_LENGTH,
        &xfer_bytes);
    	if (com_rslt == SUCCESS) 
		{
      		switch (v_work_mode_u8) 
	 		{
      			/* write work mode*/
      			case TEMP_LOW_POWER_MODE:
        			state->oversamp_temperature =
          			TEMP_LOWPOWER_OVERSAMP_TEMPERATURE; 
					a_data_u8r[1]=0x12;
        		break;
      			case TEMP_STANDARD_RESOLUTION_MODE:
        			state->oversamp_temperature =
        			TEMP_STANDARDRESOLUTION_OVERSAMP_TEMPERATURE; 
					a_data_u8r[1]=0x42;
        		break;
      			case TEMP_HIGH_RESOLUTION_MODE:
        			state->oversamp_temperature =
        			TEMP_HIGHRESOLUTION_OVERSAMP_TEMPERATURE;
					a_data_u8r[1]=0xc2;
       	 		break;
	      	}
	      	com_rslt = state->com_write(
	        	state->scp_service,
	        	state->com_port_info.port_handle,
	        	TEMP_CTRL_MEAS_REG,
	        	a_data_u8r, TEMP_GEN_READ_WRITE_DATA_LENGTH,
	       	 	&xfer_bytes,
	        	false);
    	}
  	} 
	else 
	{
    com_rslt = E_TEMP_OUT_OF_RANGE;
 	}
  }
  return com_rslt;
}

sns_rc ct7117x_read_config_params(sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  uint8_t *buffer)
{
  sns_rc rv = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;

  rv = ct7117x_com_read_wrapper(scp_service,
                               port_handle,
                               TEMP_CONFIG_REG,
                               buffer,
                               TEMP_TEMPERATURE_CALIB_DATA_LENGTH,
                               &xfer_bytes);
  if (rv != SNS_RC_SUCCESS || xfer_bytes != TEMP_TEMPERATURE_CALIB_DATA_LENGTH) {
    rv = SNS_RC_FAILED;
  }
  return rv;
}

/*!
 *  @brief This API is used to read
 *  calibration parameters used for calculation in the registers
 */
TEMP_RETURN_FUNCTION_TYPE ct7117x_get_calib_param(sns_sensor * const this)
{
  ct7117x_state *state = (ct7117x_state *)this->state->state;

  /* variable used to return communication result*/
  TEMP_RETURN_FUNCTION_TYPE com_rslt = ERROR;
  u8 a_data_u8[TEMP_TEMPERATURE_DATA_SIZE] = {TEMP_INIT_VALUE_MSB,
      TEMP_INIT_VALUE_LSB};
  /* check the p_TEMP structure pointer as NULL*/
  if (state == TEMP_NULL)
  {
    com_rslt = E_TEMP_NULL_PTR;
  } 
  else 
  {
    ct7117x_read_config_params(state->scp_service, state->com_port_info.port_handle, a_data_u8);
    /* read calibration values*/
    state->calib_param.dig_T1 = (u16)((((u16)((u8)a_data_u8[
          TEMP_TEMPERATURE_CALIB_DIG_T1_MSB]))
          << TEMP_SHIFT_BIT_POSITION_BY_08_BITS)
          | a_data_u8[
          TEMP_TEMPERATURE_CALIB_DIG_T1_LSB]);
  }
  SNS_PRINTF(LOW, this, "calib_data T1 = 0x%x",state->calib_param.dig_T1);  
  return com_rslt;
}

sns_rc ct7117x_get_who_am_i(
  sns_sync_com_port_service * scp_service,
  sns_sync_com_port_handle *port_handle,
  uint8_t *buffer)
{
  sns_rc rv = SNS_RC_SUCCESS;
  uint32_t xfer_bytes;                             //device ID address is 0x07
  
  rv = ct7117x_com_read_wrapper(scp_service,
                        port_handle,
                          0x07,
                          buffer,
                          (uint32_t) 1,
                          &xfer_bytes);
  if (rv != SNS_RC_SUCCESS || xfer_bytes != 1) {
    rv = SNS_RC_FAILED;
  }
  return rv;
}


/**
 * Encode log sensor state raw packet
 *
 * @param[i] log Pointer to log packet information
 * @param[i] log_size Size of log packet information
 * @param[i] encoded_log_size Maximum permitted encoded size of
 *                            the log
 * @param[o] encoded_log Pointer to location where encoded
 *                       log should be generated
 * @param[o] bytes_written Pointer to actual bytes written
 *       during encode
 *
 * @return sns_rc
 * SNS_RC_SUCCESS if encoding was succesful
 * SNS_RC_FAILED otherwise
 */
sns_rc ct7117x_encode_log_sensor_state_raw(
  void *log, size_t log_size, size_t encoded_log_size, void *encoded_log,
  size_t *bytes_written)
{
  sns_rc rc = SNS_RC_SUCCESS;
  uint32_t i = 0;
  size_t encoded_sample_size = 0;
  size_t parsed_log_size = 0;
  sns_diag_batch_sample batch_sample = sns_diag_batch_sample_init_default;
  uint8_t arr_index = 0;
  float temp[TEMP_NUM_AXES];
  pb_float_arr_arg arg = {.arr = (float *)temp, .arr_len = TEMP_NUM_AXES,
    .arr_index = &arr_index};

  if(NULL == encoded_log || NULL == log || NULL == bytes_written)
  {
    return SNS_RC_FAILED;
  }

  batch_sample.sample.funcs.encode = &pb_encode_float_arr_cb;
  batch_sample.sample.arg = &arg;

  if(!pb_get_encoded_size(&encoded_sample_size, sns_diag_batch_sample_fields,
                          &batch_sample))
  {
    return SNS_RC_FAILED;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(encoded_log, encoded_log_size);
  temp_batch_sample *raw_sample = (temp_batch_sample *)log;

  while(parsed_log_size < log_size &&
        (stream.bytes_written + encoded_sample_size)<= encoded_log_size &&
        i < (uint32_t)(-1))
  {
    arr_index = 0;
    arg.arr = (float *)raw_sample[i].sample;

    batch_sample.sample_type = raw_sample[i].sample_type;
    batch_sample.status = raw_sample[i].status;
    batch_sample.timestamp = raw_sample[i].timestamp;

    if(!pb_encode_tag(&stream, PB_WT_STRING,
                      sns_diag_sensor_state_raw_sample_tag))
    {
      rc = SNS_RC_FAILED;
      break;
    }
    else if(!pb_encode_delimited(&stream, sns_diag_batch_sample_fields,
                                 &batch_sample))
    {
      rc = SNS_RC_FAILED;
      break;
    }

    parsed_log_size += sizeof(temp_batch_sample);
    i++;
  }

  if (SNS_RC_SUCCESS == rc)
  {
    *bytes_written = stream.bytes_written;
  }

  return rc;
}

/**
 * Allocate Sensor State Raw Log Packet
 *
 * @param[i] log_raw_info Sensor state log info
 * @param[i] log_size     Optional size of log packet to
 *    be allocated. If not provided by setting to 0, will
 *    default to using maximum supported log packet size
 */
void ct7117x_log_sensor_state_raw_alloc(
  log_sensor_state_raw_info *log_raw_info,
  uint32_t log_size)
{
  uint32_t max_log_size = 0;

  if(NULL == log_raw_info || NULL == log_raw_info->diag ||
     NULL == log_raw_info->instance || NULL == log_raw_info->sensor_uid)
  {
    return;
  }

  // allocate memory for sensor state - raw sensor log packet
  max_log_size = log_raw_info->diag->api->get_max_log_size(
       log_raw_info->diag);

  if(0 == log_size)
  {
    // log size not specified.
    // Use max supported log packet size
    log_raw_info->log_size = max_log_size;
  }
  else if(log_size <= max_log_size)
  {
    log_raw_info->log_size = log_size;
  }
  else
  {
    return;
  }

  log_raw_info->log = log_raw_info->diag->api->alloc_log(
    log_raw_info->diag,
    log_raw_info->instance,
    log_raw_info->sensor_uid,
    log_raw_info->log_size,
    SNS_DIAG_SENSOR_STATE_LOG_RAW);

  log_raw_info->log_sample_cnt = 0;
  log_raw_info->bytes_written = 0;
}

/**
 * Submit the Sensor State Raw Log Packet
 *
 * @param[i] log_raw_info   Pointer to logging information
 *       pertaining to the sensor
 * @param[i] batch_complete true if submit request is for end
 *       of batch
 *  */
void ct7117x_log_sensor_state_raw_submit(
  log_sensor_state_raw_info *log_raw_info,
  bool batch_complete)
{
  temp_batch_sample *sample = NULL;

  if(NULL == log_raw_info || NULL == log_raw_info->diag ||
     NULL == log_raw_info->instance || NULL == log_raw_info->sensor_uid ||
     NULL == log_raw_info->log)
  {
    return;
  }

  sample = (temp_batch_sample *)log_raw_info->log;

  if(batch_complete)
  {
    // overwriting previously sample_type for last sample
    if(1 == log_raw_info->batch_sample_cnt)
    {
      sample[0].sample_type =
        SNS_DIAG_BATCH_SAMPLE_TYPE_ONLY;
    }
    else if(1 < log_raw_info->batch_sample_cnt)
    {
      sample[log_raw_info->log_sample_cnt - 1].sample_type =
        SNS_DIAG_BATCH_SAMPLE_TYPE_LAST;
    }
  }

  log_raw_info->diag->api->submit_log(
        log_raw_info->diag,
        log_raw_info->instance,
        log_raw_info->sensor_uid,
        log_raw_info->bytes_written,
        log_raw_info->log,
        SNS_DIAG_SENSOR_STATE_LOG_RAW,
        log_raw_info->log_sample_cnt * log_raw_info->encoded_sample_size,
        ct7117x_encode_log_sensor_state_raw);

  log_raw_info->log = NULL;
}

/**
 *
 * Add raw uncalibrated sensor data to Sensor State Raw log
 * packet
 *
 * @param[i] log_raw_info Pointer to logging information
 *                        pertaining to the sensor
 * @param[i] raw_data     Uncalibrated sensor data to be logged
 * @param[i] timestamp    Timestamp of the sensor data
 * @param[i] status       Status of the sensor data
 *
 * * @return sns_rc,
 * SNS_RC_SUCCESS if encoding was succesful
 * SNS_RC_FAILED otherwise
 */
sns_rc ct7117x_log_sensor_state_raw_add(
  log_sensor_state_raw_info *log_raw_info,
  float *raw_data,
  sns_time timestamp,
  sns_std_sensor_sample_status status)
{
  sns_rc rc = SNS_RC_SUCCESS;

  if(NULL == log_raw_info || NULL == log_raw_info->diag ||
     NULL == log_raw_info->instance || NULL == log_raw_info->sensor_uid ||
     NULL == raw_data || NULL == log_raw_info->log)
  {
    return SNS_RC_FAILED;
  }

  if( (log_raw_info->bytes_written + sizeof(temp_batch_sample)) >
     log_raw_info->log_size)
  {
    ct7117x_log_sensor_state_raw_submit(log_raw_info, false);
    ct7117x_log_sensor_state_raw_alloc(log_raw_info, 0);
  }

  if(NULL == log_raw_info->log)
  {
    rc = SNS_RC_FAILED;
  }
  else
  {
    temp_batch_sample *sample =
        (temp_batch_sample *)log_raw_info->log;

    if(0 == log_raw_info->batch_sample_cnt)
    {
      sample[log_raw_info->log_sample_cnt].sample_type =
        SNS_DIAG_BATCH_SAMPLE_TYPE_FIRST;
    }
    else
    {
      sample[log_raw_info->log_sample_cnt].sample_type =
        SNS_DIAG_BATCH_SAMPLE_TYPE_INTERMEDIATE;
    }

    sample[log_raw_info->log_sample_cnt].timestamp = timestamp;

    sns_memscpy(sample[log_raw_info->log_sample_cnt].sample,
                sizeof(sample[log_raw_info->log_sample_cnt].sample),
                raw_data,
                sizeof(sample[log_raw_info->log_sample_cnt].sample));

    sample[log_raw_info->log_sample_cnt].status = status;

    log_raw_info->bytes_written += sizeof(temp_batch_sample);

    log_raw_info->log_sample_cnt++;
    log_raw_info->batch_sample_cnt++;
  }

  return rc;
}

/**
 * Read wrapper for Synch Com Port Service.
 *
 * @param[i] port_handle      port handle
 * @param[i] reg_addr         register address
 * @param[i] buffer           read buffer
 * @param[i] bytes            bytes to read
 * @param[o] xfer_bytes       bytes read
 *
 * @return sns_rc
 */
sns_rc ct7117x_com_read_wrapper(
  sns_sync_com_port_service *scp_service,
  sns_sync_com_port_handle *port_handle,
  uint32_t reg_addr,
  uint8_t  *buffer,
  uint32_t bytes,
  uint32_t *xfer_bytes)
{
  sns_port_vector port_vec;
  port_vec.buffer = buffer;
  port_vec.bytes = bytes;
  port_vec.is_write = false;
  port_vec.reg_addr = reg_addr;

  return scp_service->api->sns_scp_register_rw(port_handle,
                              &port_vec,
                              1,
                              false,
                              xfer_bytes);
}

/**
 * Write wrapper for Synch Com Port Service.
 *
 * @param[i] port_handle      port handle
 * @param[i] reg_addr         register address
 * @param[i] buffer           write buffer
 * @param[i] bytes            bytes to write
 * @param[o] xfer_bytes       bytes written
 * @param[i] save_write_time  true to save write transfer time.
 *
 * @return sns_rc
 */
sns_rc ct7117x_com_write_wrapper(
  sns_sync_com_port_service * scp_service,
  sns_sync_com_port_handle *port_handle,
  uint32_t reg_addr,
  uint8_t  *buffer,
  uint32_t bytes,
  uint32_t *xfer_bytes,
  bool     save_write_time)
{
  sns_port_vector port_vec;
  port_vec.buffer = buffer;
  port_vec.bytes = bytes;
  port_vec.is_write = true;
  port_vec.reg_addr = reg_addr;

  return  scp_service->api->sns_scp_register_rw(
      port_handle,
      &port_vec,
      1,
      save_write_time,
      xfer_bytes);
}
/*!
 *  @brief This API is used to write filter setting
 *  @param v_value_u8 : The value of filter coefficient
 *  @return results of bus communication function
 *  @retval 0 -> Success
 *  @retval -1 -> Error
 *
 *
 */
TEMP_RETURN_FUNCTION_TYPE ct7117x_set_filter(
ct7117x_instance_state *state, u8 v_value_u8)
{
  TEMP_RETURN_FUNCTION_TYPE com_rslt = SUCCESS;
  u8 v_data_u8=TEMP_INIT_CONFIG_VALUE;                           
  uint32_t xfer_bytes;
  /* check the p_TEMP structure pointer as NULL*/
  if (state == TEMP_NULL) {
    com_rslt = E_TEMP_NULL_PTR;
  } else {
    /* write filter*/
    com_rslt = state->com_read(
        state->scp_service,
        state->com_port_info.port_handle,
        TEMP_CONFIG_REG_FILTER__REG, &v_data_u8,
        TEMP_GEN_READ_WRITE_DATA_LENGTH,
        &xfer_bytes);
    if (com_rslt == SUCCESS) {
      v_data_u8 = TEMP_SET_BITSLICE(v_data_u8,
          TEMP_CONFIG_REG_FILTER, v_value_u8);
      com_rslt += state->com_write(
          state->scp_service,
          state->com_port_info.port_handle,
          TEMP_CONFIG_REG_FILTER__REG,
          &v_data_u8,
          TEMP_GEN_READ_WRITE_DATA_LENGTH,
          &xfer_bytes,
          false);
    }
  }
  return com_rslt;
}

/*!
 *  @brief This API is used to read uncompensated temperature
 *  in the registers 0x00
 *  @param v_uncomp_temperature_s16 : The uncompensated temperature.
 *  @return results of bus communication function
 *  @retval 0 -> Success
 *  @retval -1 -> Error
 */
TEMP_RETURN_FUNCTION_TYPE ct7117x_read_uncomp_temperature(
  ct7117x_instance_state *state,
  s16 *v_uncomp_temperature_s16)
{
  /* variable used to return communication result*/
  TEMP_RETURN_FUNCTION_TYPE com_rslt = ERROR;
  u8 a_data_u8r[TEMP_TEMPERATURE_DATA_SIZE] = {TEMP_INIT_VALUE_MSB,
      TEMP_INIT_VALUE_LSB};
  u32 xfer_bytes;
  /* check the p_temp structure pointer as NULL*/
  if (state == TEMP_NULL) {
    com_rslt = E_TEMP_NULL_PTR;
  } else {
    /* read temperature data */
    com_rslt = state->com_read(
        state->scp_service,
        state->com_port_info.port_handle,
        TEMP_TEMPERATURE_REG, a_data_u8r,
        TEMP_TEMPERATURE_DATA_LENGTH,
        &xfer_bytes);

    *v_uncomp_temperature_s16 = (s16)((((u16)(
        a_data_u8r[TEMP_TEMPERATURE_MSB_DATA]))
        << TEMP_SHIFT_BIT_POSITION_BY_08_BITS)
        | ((u16)(
        a_data_u8r[TEMP_TEMPERATURE_LSB_DATA])));
  }
  return com_rslt;
}

/*!
 * @brief This API used to read
 * actual temperature from uncompensated temperature
 * @note Returns the value in Degree centigrade
 * @note Output value of "51.23" equals 51.23 DegC.
 *  @param v_uncomp_temperature_s32 : value of uncompensated temperature
 *  @return
 *  Actual temperature in floating point
 *
 */
float ct7117x_compensate_temperature_double(
  s16 v_uncomp_temperature_s16)
{
  double temperature = TEMP_INIT_VALUE_MSB;
  temperature = ((double)v_uncomp_temperature_s16) / 128.0 ;
  return temperature;
}
 
/*!
 *  @brief This API used to set the
 *  Operational Mode from the sensor in the register 0xF4 bit 0 and 1
 *  @param v_power_mode_u8 : The value of power mode value
 *  value            |   Power mode
 * ------------------|------------------
 *  0x00             | temp_SLEEP_MODE
 *  0x01             | temp_FORCED_MODE
 *  0x03             | temp_NORMAL_MODE
 *  @return results of bus communication function
 *  @retval 0 -> Success
 *  @retval -1 -> Error
 */
TEMP_RETURN_FUNCTION_TYPE ct7117x_set_power_mode(
  ct7117x_instance_state *state,
  u8 v_power_mode_u8)
{
  TEMP_RETURN_FUNCTION_TYPE com_rslt = ERROR;
  
  u8 a_data_u8r[TEMP_TEMPERATURE_DATA_SIZE] = {0x00,
			TEMP_INIT_CONFIG_VALUE};
  u32 xfer_bytes;
  if (state == TEMP_NULL) {
    com_rslt = E_TEMP_NULL_PTR;
  } else {
    if (v_power_mode_u8 <= TEMP_NORMAL_MODE) {

          if(v_power_mode_u8==TEMP_SLEEP_MODE)
		  	a_data_u8r[0]|=0x01;
		  else if(v_power_mode_u8==TEMP_FORCED_MODE)
		  	{
		  	  a_data_u8r[0]|=0x01;
			  a_data_u8r[1]|=0x01;
		  	}	  	
          
      com_rslt = state->com_write(
          state->scp_service,
          state->com_port_info.port_handle,
          TEMP_CTRL_MEAS_REG_POWER_MODE__REG,
          a_data_u8r,
          TEMP_GEN_READ_WRITE_DATA_LENGTH,
          &xfer_bytes,
          false);
    } else {
      com_rslt = E_TEMP_OUT_OF_RANGE;
    }
  }
  return com_rslt;
}

void ct7117x_handle_temperature_data_sample(
    sns_sensor_instance *const instance,
    float sample_data,
    sns_time timestamp)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  sns_diag_service* diag       = state->diag_service;
  log_sensor_state_raw_info log_sensor_state_raw_info;
  sns_std_sensor_sample_status status = SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH;
  sns_sensor_uid suid;
  timestamp = sns_get_system_time();
  suid = state->temperature_info.suid;
  log_sensor_state_raw_info.encoded_sample_size = state->log_raw_encoded_size;
  log_sensor_state_raw_info.diag = diag;
  log_sensor_state_raw_info.instance = instance;
  log_sensor_state_raw_info.sensor_uid = &suid;
  ct7117x_log_sensor_state_raw_alloc(&log_sensor_state_raw_info, 0);
  pb_send_sensor_stream_event(instance,
      &suid,
      timestamp,
      SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT,
      SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH,
      &sample_data,
      1,
      state->encoded_imu_event_len);
  ct7117x_log_sensor_state_raw_add(
        &log_sensor_state_raw_info,
        &sample_data,
        timestamp,
        status);
  ct7117x_log_sensor_state_raw_submit(&log_sensor_state_raw_info, true);
}

void ct7117x_convert_and_send_temp_sample(
  sns_sensor_instance *const instance,
  sns_time            timestamp,
  const uint8_t       data[2])
{
  s16 v_uncomp_temperature_s16 = 0;
  float ct7117x_comp_data_buffer = 0;

  v_uncomp_temperature_s16 = (s16)((((u16)(
          data[0]))
          << TEMP_SHIFT_BIT_POSITION_BY_08_BITS)
          | ((u16)(data[1])));

  ct7117x_comp_data_buffer = ct7117x_compensate_temperature_double(v_uncomp_temperature_s16);
  ct7117x_handle_temperature_data_sample(instance, ct7117x_comp_data_buffer, timestamp);
}
 
void ct7117x_process_temperature_timer_stream_data_buffer(sns_sensor_instance *instance)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  s16 v_uncomp_temperature_s16 = 0;
  float ct7117x_comp_data_buffer = 0;

  ct7117x_read_uncomp_temperature(state, &v_uncomp_temperature_s16);
  ct7117x_comp_data_buffer = ct7117x_compensate_temperature_double(v_uncomp_temperature_s16);
  sns_time timestamp = state->interrupt_timestamp;
  ct7117x_handle_temperature_data_sample(instance, ct7117x_comp_data_buffer, timestamp);
}
 
void ct7117x_handle_temperature_data_stream_timer_event(sns_sensor_instance *const instance)
{
  ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
  int err = 0;
  SNS_INST_PRINTF(LOW, instance, "t timer_event");
  ct7117x_process_temperature_timer_stream_data_buffer(instance);
  err = ct7117x_set_power_mode(state,TEMP_FORCED_MODE);
  if (err)
     SNS_INST_PRINTF(ERROR, instance,"config power mode failed err = %d", err);
}

