#pragma once
/*
*	sns_ct7117x_sensor.h
*	Common sensor data types for combo driver
*
*/

#include "sns_data_stream.h"
#include "sns_diag_service.h"
#include "sns_printf.h"
#include "sns_pwr_rail_service.h"
#include "sns_registry_util.h"
#include "sns_sensor.h"
#include "sns_suid_util.h"
#include "sns_sync_com_port_service.h"
#include "sns_timer.pb.h"

#include "sns_ct7117x_config.h"
#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor_instance.h"

#define TEMPERATURE_SUID_0 \
{  \
    .sensor_uid =  \
    {  \
        0x53, 0x45, 0x4e, 0x53, 0x59, 0x4c, 0x49, 0x4e, \
        0x4b, 0x43, 0x54, 0x37, 0x31, 0x31, 0x37, 0x41  \
    }  \
}

#define TEMPERATURE_TYPE_0 "ambient_temperature"
#define TEMPERATURE_PROTO_0 "sns_ambient_temperature.proto"

#ifdef CT7117X_ENABLE_DUAL_SENSOR
#define TEMPERATURE_SUID_1 \
{  \
        .sensor_uid =  \
    {  \
            0xf5, 0x49, 0x0c, 0x1d, 0x25, 0x84, 0x4b, 0xc8, \
            0x8a, 0xb1, 0x9c, 0x8e, 0x8b, 0xff, 0x18, 0x9a  \
    }  \
}

#define TEMPERATURE_TYPE_1 "skin_temperature"
#define TEMPERATURE_PROTO_1 "sns_skin_temperature.proto"
#endif

/*sensor attribute*/
#define CT7117X_TEMPERATURE_RESOLUTION  (0.0078125f)
#define CT7117X_TEMPERATURE_RANGE_MIN  (-40.0f)
#define CT7117X_TEMPERATURE_RANGE_MAX  (125.0f)
#define CT7117X_TEMPERATURE_LOW_POWER_CURRENT  (1)
#define CT7117X_TEMPERATURE_NORMAL_POWER_CURRENT  (3)
#define CT7117X_TEMPERATURE_SLEEP_CURRENT  (0)

/* Forward Declaration of temperature Sensor API */
extern sns_sensor_api ct7117x_temperature_sensor_api;

/* ODR definitions*/
#define TEMP_ODR_0                 (0.0f)
#define TEMP_ODR_1                 (1.0f)
#define TEMP_ODR_5                 (5.0f)
#define TEMP_ODR_10                (10.0f)
#define TEMP_ODR_25                (25.0f)
#define SUID_NUM 3
/*op mode*/
#define TEMP_LPM              "LPM"
#define TEMP_HIGH_PERF        "HIGH_PERF"
#define TEMP_NORMAL           "NORMAL"

/* Power rail timeout States for the Sensors.*/
typedef enum
{
  POWER_RAIL_PENDING_NONE,
  POWER_RAIL_PENDING_INIT,
  POWER_RAIL_PENDING_SET_CLIENT_REQ,
  POWER_RAIL_PENDING_CREATE_DEPENDENCY,
} temp_power_rail_pending_state;

/** Interrupt Sensor State. */
typedef struct temp_timer_package
{
    sns_timer_sensor_config   req_payload;
    ct7117x_sensor_type        sensor_type;
} temp_timer_package;

/*sensor state*/
typedef struct ct7117x_state
{
  /* data stream */
  sns_data_stream              *reg_data_stream;
  sns_data_stream              *fw_stream;
  sns_data_stream              *timer_stream;
  /* sensor suid */
  sns_sensor_uid               reg_suid;
  sns_sensor_uid               irq_suid;
  sns_sensor_uid               timer_suid;
  sns_sensor_uid               acp_suid; // Asynchronous COM Port for the stream data fetching
  sns_sensor_uid               my_suid;
  SNS_SUID_LOOKUP_DATA(SUID_NUM) suid_lookup_data;
  ct7117x_sensor_type           sensor;   /* used to identify the sensor state involved with */
  /* COM port information */
  ct7117x_com_port_info         com_port_info;
  /*  Power rail */
  sns_pwr_rail_service         *pwr_rail_service;
  sns_rail_config              rail_config;
  sns_power_rail_state         registry_rail_on_state;
  bool                         hw_is_present;
  bool                         sensor_client_present;
  /* state machine flag */
  temp_power_rail_pending_state power_rail_pend_state;
  // sensor configuration
  bool is_dri;
  int64_t hardware_id;
  bool supports_sync_stream;
  uint8_t resolution_idx;
  // registry sensor config
  bool registry_cfg_received;
  sns_registry_phy_sensor_cfg registry_cfg;
  // registry sensor platform config
  bool registry_pf_cfg_received;
  sns_registry_phy_sensor_pf_cfg registry_pf_cfg;
  uint16_t                    	who_am_i;
  sns_diag_service             	*diag_service;
  sns_sync_com_port_service    	*scp_service;
  size_t                       	encoded_event_len;
  struct ct7117x_calib_param_t  calib_param;
} ct7117x_state;

/** Functions shared by all temp Sensors */
/**
 * This function parses the client_request list per Sensor and
 * determines final config for the Sensor Instance.
 *
 * @param[i] this          Sensor reference
 * @param[i] instance      Sensor Instance to config
 * @param[i] sensor_type   Sensor type
 *
 * @return none
 */
void ct7117x_reval_instance_config(sns_sensor *this,
        sns_sensor_instance *instance, ct7117x_sensor_type sensor_type);

/**
 * Sends a request to the SUID Sensor to get SUID of a dependent
 * Sensor.
 *
 * @param[i] this          Sensor reference
 * @param[i] data_type     data_type of dependent Sensor
 * @param[i] data_type_len Length of the data_type string
 */
void ct7117x_send_suid_req(sns_sensor *this, char *const data_type, uint32_t data_type_len);
void ct7117x_sensor_process_suid_events(sns_sensor * const this);
sns_rc ct7117x_com_read_wrapper(sns_sync_com_port_service *scp_service,sns_sync_com_port_handle *port_handle,
        uint32_t reg_addr, uint8_t *buffer, uint32_t bytes,
        uint32_t *xfer_bytes);
sns_rc ct7117x_com_write_wrapper(sns_sync_com_port_service *scp_service,sns_sync_com_port_handle *port_handle,
        uint32_t reg_addr, uint8_t *buffer, uint32_t bytes,
        uint32_t *xfer_bytes, bool save_write_time);
s8 ct7117x_set_power_mode(ct7117x_instance_state *state, u8 v_power_mode_u8);
sns_sensor_instance* ct7117x_sensor_set_client_request(sns_sensor * const this,
        struct sns_request const *exist_request,
        struct sns_request const *new_request,
        bool remove);
sns_rc ct7117x_sensor_notify_event(sns_sensor * const this);
sns_rc ct7117x_temperature_init(sns_sensor * const this);
sns_rc ct7117x_temperature_deinit(sns_sensor * const this);

