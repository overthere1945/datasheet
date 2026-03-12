/*
*	sns_ct7117x_sensor.c
*	Common normal mode sensor functions for driver
* 센서의 일반 모드 공통 함수 정의 (생명주기, 등록, 탐색 등 관리)
*/

/*==============================================================================
  Include Files
  ============================================================================*/
#include <string.h>
#include <stdio.h>
#include "sns_mem_util.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_service.h"
#include "sns_sensor_util.h"
#include "sns_types.h"
#include "sns_attribute_util.h"

#include "sns_ct7117x_sensor.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_suid.pb.h"
#include "sns_suid_util.h"

/*==============================================================================
  Macro Definitions
  ============================================================================*/
#define CT7117X_NAME                    "temp"
#define CT7117X_PLATFORM_CONFIG         "_platform.config"
#define CT7117X_CONFIG_TEMP             ".temp.config"

// 스트리밍 타입 정의 (연속 데이터)
static const sns_std_sensor_stream_type stream_type =
    SNS_STD_SENSOR_STREAM_TYPE_STREAMING;
// 에러 없는 인스턴스 구조체 선언
extern struct sns_sensor_instance sns_instance_no_error;

/*==============================================================================
  Function Definitions
  ============================================================================*/

// OEM 클라이언트 설정 (사용 안 함)
void ct7117x_inst_set_oem_client_config(sns_sensor_instance *const this)
{
  UNUSED_VAR(this);
}

// OEM 클라이언트 요청 설정 확인 (항상 true)
bool ct7117x_set_oem_client_request(sns_sensor *const this,
    struct sns_request const *exist_request)
{
  UNUSED_VAR(this);
  UNUSED_VAR(exist_request);
  return true;
}

// OEM 클라이언트 요청 메시지 ID 확인
bool ct7117x_set_oem_client_request_msgid(sns_sensor *const this,
    struct sns_request const *new_request)
{
  UNUSED_VAR(this);
  // Self Test 설정 요청이면 false 반환
  if(new_request != NULL &&
     (new_request->message_id == SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG))
  {
    return false;
  }
  return true;
}

/*
*	set client new config include report_rate sample_rate 
* 센서 인스턴스에 새로운 설정(ODR 등) 적용 요청
*/
static void ct7117x_set_sensor_inst_config(sns_sensor *this,
    sns_sensor_instance *instance, float chosen_report_rate,
    float chosen_sample_rate,
    ct7117x_sensor_type  sensor_type)
{
  UNUSED_VAR(instance);
  sns_temp_cfg_req new_client_config;
  sns_request config;

  // 설정 요청 구조체 값 채우기
  new_client_config.report_rate = chosen_report_rate;
  new_client_config.sample_rate = chosen_sample_rate;
  new_client_config.sensor_type = sensor_type;
  new_client_config.op_mode = FORCED_MODE;

  // 메시지 ID 및 길이 설정
  config.message_id  = SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG;
  config.request_len = sizeof(sns_temp_cfg_req);
  config.request     = &new_client_config;
  // 인스턴스 API를 통해 설정 적용
  this->instance_api->set_client_config(instance, &config);
}

/*
*	decoded request 
* 요청 메시지 디코딩 함수
*/
static bool ct7117x_get_decoded_sensor_request(
  sns_sensor const *this,
  sns_request const *in_request,
  sns_std_request *decoded_request,
  sns_std_sensor_config *decoded_payload)
{
  pb_istream_t stream;
  /* decode argument */
  pb_simple_cb_arg arg = {
      .decoded_struct = decoded_payload,
      .fields = sns_std_sensor_config_event_fields
  };
  /* decode functions.decode */
  decoded_request->payload = (struct pb_callback_s ) {
      .funcs.decode = &pb_decode_simple_cb,
      .arg = &arg
  };
  stream = pb_istream_from_buffer(in_request->request, in_request->request_len);
  if (!pb_decode(&stream, sns_std_request_fields, decoded_request))
  {
       SNS_PRINTF(ERROR, this, "LSM decode error");
       return false;
  }
  return true;
}

/* *	get the configuration item for tempture sensor 
* 모든 클라이언트 요청을 순회하며 최대 샘플링 레이트 계산
*/
static void ct7117x_get_sensor_tempture_config(sns_sensor *this,
  sns_sensor_instance *instance,
  ct7117x_sensor_type sensor_type,
  float *chosen_sample_rate,
  float *chosen_report_rate,
  bool *sensor_tempture_client_present)
{
  UNUSED_VAR(sensor_type);
  ct7117x_state *state = (ct7117x_state *)this->state->state;
  ct7117x_instance_state *inst_state =
     (ct7117x_instance_state*)instance->state->state;
  sns_request const *request;

  *chosen_report_rate = 0;
  *chosen_sample_rate = 0;
  *sensor_tempture_client_present = false;

  /** Parse through existing requests and get fastest sample
   * rate and report rate requests. */
  // 존재하는 모든 요청을 확인하여 가장 빠른 속도 결정
  for(request = instance->cb->get_client_request(instance,
                                                  &state->my_suid, true);
    NULL != request;
    request = instance->cb->get_client_request(instance,
                                                  &state->my_suid, false))
  {
    sns_std_request decoded_request;
    sns_std_sensor_config decoded_payload = {0};
    SNS_PRINTF(LOW, this, "temperature message id %d", request->message_id);
    if(request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG)
    {
    	if(ct7117x_get_decoded_sensor_request(this, request, &decoded_request, &decoded_payload))
    	{
      		float report_rate;
      		*chosen_sample_rate = SNS_MAX(*chosen_sample_rate,
                     decoded_payload.sample_rate);
      		if(decoded_request.has_batching
       			&&
       			decoded_request.batching.batch_period > 0)
      		{
      			report_rate = (1000000.0 / (float)decoded_request.batching.batch_period);
      		}
      		else
      		{
      			report_rate = *chosen_sample_rate;
      		}
      		*chosen_report_rate = SNS_MAX(*chosen_report_rate, report_rate);
      		*sensor_tempture_client_present = true;
    	}
    }
  }
  inst_state->temperature_info.report_timer_hz  = *chosen_report_rate;
  inst_state->temperature_info.sampling_rate_hz= *chosen_sample_rate;
  SNS_PRINTF(LOW, this, "temperature sample rete %d temperature present %d",
  (uint8_t)inst_state->temperature_info.sampling_rate_hz,
  *sensor_tempture_client_present);
}

/* *	mark corresponding sensor
* 센서 활성화 상태 마킹
*/
static void  ct7117x_mark_sensor_enable_state (
    sns_sensor_instance *this,
    ct7117x_sensor_type sensor_type,
    bool enable)
{
  ct7117x_instance_state *inst_state = (ct7117x_instance_state *) this->state->state;
    /* mark the corresponding sensor as fifo info field *now only the sw fifo* */
	if(sensor_type == TEMP_TEMPERATURE)
	{
    	if (enable) 
		{
      		inst_state->deploy_info.publish_sensors |= sensor_type;
      		inst_state->deploy_info.enable |= sensor_type;
   		} 
		else 
		{
      		inst_state->deploy_info.publish_sensors &= ~sensor_type;
      		inst_state->deploy_info.enable &= ~sensor_type;
    	}
	}
}

/*
*	re-evaluate the instance configuration
* 인스턴스 설정 재평가 및 적용
*/
void temp_reval_instance_config(sns_sensor *this,
    sns_sensor_instance *instance,
    ct7117x_sensor_type sensor_type)
{
  ct7117x_instance_state *inst_state = (ct7117x_instance_state*)instance->state->state;
  ct7117x_state *sensor_state = (ct7117x_state*)this->state->state;

  /**
   * 1. Get best temperature Config.
   * 2. Decide best Instance Config based on above outputs.
   */
  float t_sample_rate = 0;
  float t_report_rate = 0;

  float chosen_sample_rate;
  float chosen_report_rate;

  bool t_sensor_client_present = false;

  SNS_PRINTF(LOW, this, "sensor type: %d %d", sensor_state->sensor, sensor_type);

  if(sensor_type == TEMP_TEMPERATURE) 
  {
    ct7117x_get_sensor_tempture_config(this, instance, sensor_state->sensor,
    &t_sample_rate, &t_report_rate, &t_sensor_client_present);
    chosen_sample_rate = t_sample_rate;
    chosen_report_rate = t_report_rate;
  }

  if (sensor_type == TEMP_TEMPERATURE) 
  { 
      ct7117x_mark_sensor_enable_state(
      instance, TEMP_TEMPERATURE, t_sensor_client_present);
	  
    /* set the sensor instance configuration*/
    ct7117x_set_sensor_inst_config(this, instance, chosen_report_rate, chosen_sample_rate, sensor_type);
  }
  if (!inst_state->deploy_info.enable) 
  {
    sensor_state->rail_config.rail_vote = SNS_RAIL_OFF;
    sensor_state->pwr_rail_service->api->sns_vote_power_rail_update(
        sensor_state->pwr_rail_service, this, &sensor_state->rail_config, NULL);
  }
  // if(!inst_state->enabled_sensors)
 // {
 //   inst_state->instance_is_ready_to_configure = false;
 //   SNS_PRINTF(HIGH, this,"debug inst_state->instance_is_ready_to_configure = false");
//  }
}

/*
*	process suid events save suid based on incoming data type name
* SUID 이벤트 처리 및 저장
*/
void ct7117x_sensor_process_suid_events(sns_sensor *const this)
{
  ct7117x_state *state = (ct7117x_state*)this->state->state;

  for(;
      0 != state->fw_stream->api->get_input_cnt(state->fw_stream);
      state->fw_stream->api->get_next_input(state->fw_stream))
  {
    sns_sensor_event *event =
      state->fw_stream->api->peek_input(state->fw_stream);

    if(SNS_SUID_MSGID_SNS_SUID_EVENT == event->message_id)
    {
      pb_istream_t stream = pb_istream_from_buffer((void*)event->event, event->event_len);
      sns_suid_event suid_event = sns_suid_event_init_default;
      pb_buffer_arg data_type_arg = { .buf = NULL, .buf_len = 0 };
      sns_sensor_uid uid_list;
      sns_suid_search suid_search;
      suid_search.suid = &uid_list;
      suid_search.num_of_suids = 0;

      suid_event.data_type.funcs.decode = &pb_decode_string_cb;
      suid_event.data_type.arg = &data_type_arg;
      suid_event.suid.funcs.decode = &pb_decode_suid_event;
      suid_event.suid.arg = &suid_search;

      if(!pb_decode(&stream, sns_suid_event_fields, &suid_event)) 
	  {
         continue;
      }

      /* if no suids found, ignore the event */
      if(suid_search.num_of_suids == 0)
      {
        continue;
      }

      /* save suid based on incoming data type name */
      if(0 == strncmp(data_type_arg.buf, "interrupt", data_type_arg.buf_len))
      {
        state->irq_suid = uid_list;
      }
      else if(0 == strncmp(data_type_arg.buf, "timer", data_type_arg.buf_len))
      {
        state->timer_suid = uid_list;
      }
      else if (0 == strncmp(data_type_arg.buf, "async_com_port",
                            data_type_arg.buf_len))
      {
        state->acp_suid = uid_list;
      }
      else if (0 == strncmp(data_type_arg.buf, "registry", data_type_arg.buf_len))
      {
        state->reg_suid = uid_list;
      }
      else
      {
        SNS_PRINTF(ERROR, this, "invalid datatype_name");
      }
    }
  }
}

/*
*	Publish attributes read from registry
* 레지스트리에서 읽은 속성 발행
*/
static void ct7117x_publish_registry_attributes(sns_sensor *const this)
{
#if CT7117X_ATTRIBUTE_DISABLED
  UNUSED_VAR(this);
#else
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = state->registry_cfg.is_dri;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_DRI, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = state->registry_cfg.sync_stream;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_STREAM_SYNC, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = state->registry_cfg.hw_id;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_HW_ID, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = state->registry_pf_cfg.rigid_body_type;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_RIGID_BODY, &value, 1, false);
  }
#endif /* CT7117X_ATTRIBUTE_DISABLED */
}

/*
*	encode timer sensor config request and send timer request  
* 파워 레일 타이머 설정 및 요청 전송
*/
static void ct7117x_sensor_start_power_rail_timer(sns_sensor *const this,
    sns_time timeout_ticks,
    temp_power_rail_pending_state pwr_rail_pend_state)
{
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  
  SNS_PRINTF(LOW, this, "start power rail timer");
  if(NULL == state->timer_stream)
  {
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_svc = (sns_stream_service*)
      service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
    stream_svc->api->create_sensor_stream(stream_svc, this, state->timer_suid,
                                          &state->timer_stream);
  }

  if(NULL != state->timer_stream)
  {
    sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
    size_t req_len;
    uint8_t buffer[20];
    sns_memset(buffer, 0, sizeof(buffer));
    req_payload.is_periodic = false;
    req_payload.start_time = sns_get_system_time();
    req_payload.timeout_period = timeout_ticks;

    req_len = pb_encode_request(buffer, sizeof(buffer), &req_payload,
                                sns_timer_sensor_config_fields, NULL);
    if(req_len > 0)
    {
      sns_request timer_req =
        {  .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
           .request = buffer, .request_len = req_len};
      state->timer_stream->api->send_request(state->timer_stream, &timer_req);
      state->power_rail_pend_state = pwr_rail_pend_state;
    }
    else
    {
      SNS_PRINTF(ERROR, this, "LSM timer req encode error");
    }
  }
  else
  {
    SNS_PRINTF(ERROR, this, "create timer stream err");
  }
}

// COM 포트 등록 함수
static sns_rc ct7117x_register_com_port(sns_sensor *const this)
{
    sns_rc rv = SNS_RC_SUCCESS;
    ct7117x_state *state = (ct7117x_state*) this->state->state;
    if (NULL == state->com_port_info.port_handle)
    {
        rv = state->scp_service->api->sns_scp_register_com_port(
            &state->com_port_info.com_config,
            &state->com_port_info.port_handle);
        SNS_PRINTF(LOW, this, "register com port success? rv=%d", rv);
        /* open the COM port*/
        if(rv == SNS_RC_SUCCESS)
        {
            rv = state->scp_service->api->sns_scp_open(
                state->com_port_info.port_handle);
            SNS_PRINTF(LOW, this,
                       "open com port success> rv=%d, handle=%p", rv,
                       state->com_port_info.port_handle);
        }
        else
        {
            SNS_PRINTF(ERROR, this,
                       "sns_scp_register_com_port fail rc:%u",rv);
        }
    }
    return rv;
}

#if !CT7117X_DISABLE_REGISTRY

// 레지스트리 문자열 생성 함수
static void ct7117x_sensor_create_registry_str(int hw_id, char* type,
                                               char *str, uint16_t str_len)
{
  snprintf(str, str_len, "%s_%d%s", CT7117X_NAME, hw_id, type);
}
/*
*	process registry event load .json data
* 레지스트리 이벤트 처리 (.json 데이터 로드)
*/
static void ct7117x_sensor_process_registry_event(sns_sensor *const this,
                                                  sns_sensor_event *event)
{
  bool rv = true;
  char buffer[60] = {0};
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  pb_istream_t stream = pb_istream_from_buffer((void*)event->event,
      event->event_len);
  
  SNS_PRINTF(LOW, this, ">>ct7117x_sensor_process_registry_event = %d<<", event->message_id);              //514

  if(SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_EVENT == event->message_id)
  {
    sns_registry_read_event read_event = sns_registry_read_event_init_default;
    pb_buffer_arg group_name = {0,0};
    read_event.name.arg = &group_name;
    read_event.name.funcs.decode = pb_decode_string_cb;

    if(!pb_decode(&stream, sns_registry_read_event_fields, &read_event))
    {
      SNS_PRINTF(ERROR, this, "Error decoding registry event");
    }
    else
    {
      stream = pb_istream_from_buffer((void*)event->event, event->event_len);

      ct7117x_sensor_create_registry_str(state->hardware_id,
                                         CT7117X_CONFIG_TEMP,
                                         buffer, sizeof(buffer));
      bool temp_cfg = (0 == strncmp((char*)group_name.buf, buffer,
                                                  group_name.buf_len));

      ct7117x_sensor_create_registry_str(state->hardware_id,
                                         CT7117X_PLATFORM_CONFIG,
                                         buffer, sizeof(buffer));
      bool pf_cfg = (0 == strncmp((char*)group_name.buf, buffer,
                                  group_name.buf_len));
      if(temp_cfg)
      {
        {
          sns_registry_decode_arg arg = {
            .item_group_name = &group_name,
            .parse_info_len = 1,
            .parse_info[0] = {
              .group_name = "config",
              .parse_func = sns_registry_parse_phy_sensor_cfg,
              .parsed_buffer = &state->registry_cfg
            }
          };

          read_event.data.items.funcs.decode = &sns_registry_item_decode_cb;
          read_event.data.items.arg = &arg;

          rv = pb_decode(&stream, sns_registry_read_event_fields, &read_event);
        }

        if(rv)
        {
          state->registry_cfg_received = true;
          state->is_dri = state->registry_cfg.is_dri;
          state->hardware_id = state->registry_cfg.hw_id;
          state->resolution_idx = state->registry_cfg.res_idx;
          state->supports_sync_stream = state->registry_cfg.sync_stream;

          SNS_PRINTF(LOW, this, "Registry read event for sensor:%d, received is_dri:%d ",
                                   state->sensor,
                                   state->is_dri);                                 
		  SNS_PRINTF(LOW, this,"hardware_id:%lld ",state->hardware_id);
          SNS_PRINTF(LOW, this, "resolution_idx:%d, supports_sync_stream:%d ",
                                   state->resolution_idx,
                                   state->supports_sync_stream);
        }
      }
      else if(pf_cfg)
      {
        {
          sns_registry_decode_arg arg = {
            .item_group_name = &group_name,
            .parse_info_len = 1,
            .parse_info[0] = {
              .group_name = "config",
              .parse_func = sns_registry_parse_phy_sensor_pf_cfg,
              .parsed_buffer = &state->registry_pf_cfg
            }
          };

          read_event.data.items.funcs.decode = &sns_registry_item_decode_cb;
          read_event.data.items.arg = &arg;

          rv = pb_decode(&stream, sns_registry_read_event_fields, &read_event);
        }

        if(rv)
        {
          state->registry_pf_cfg_received = true;

          state->com_port_info.com_config.bus_type = SNS_BUS_SPI;// state->registry_pf_cfg.bus_type;
          state->com_port_info.com_config.bus_instance = 5;//state->registry_pf_cfg.bus_instance;
          state->com_port_info.com_config.slave_control = 0;//state->registry_pf_cfg.slave_config;
          state->com_port_info.com_config.min_bus_speed_KHz = 15000;//state->registry_pf_cfg.min_bus_speed_khz;
          state->com_port_info.com_config.max_bus_speed_KHz = 15000;//state->registry_pf_cfg.max_bus_speed_khz;
          state->com_port_info.com_config.reg_addr_type = 0x0;//state->registry_pf_cfg.reg_addr_type;
 
          state->rail_config.num_of_rails = state->registry_pf_cfg.num_rail;
          state->registry_rail_on_state = state->registry_pf_cfg.rail_on_state;
          sns_strlcpy(state->rail_config.rails[0].name,
                      state->registry_pf_cfg.vddio_rail,
                      sizeof(state->rail_config.rails[0].name));
          sns_strlcpy(state->rail_config.rails[1].name,
                      state->registry_pf_cfg.vdd_rail,
                      sizeof(state->rail_config.rails[1].name)); 
        }
      }
      else
      {
        SNS_PRINTF(ERROR, this, "no find the group_name");
        rv = false;
      } 
    }
  }
  else
  {
    SNS_PRINTF(ERROR, this, "Received unsupported registry event msg id %u",
                             event->message_id);
  }
}

/*
*	send registry read request
* 레지스트리 읽기 요청 전송
*/
static void ct7117x_sensor_send_registry_request(sns_sensor *const this,
                                                 char *reg_group_name)
{
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  uint8_t buffer[100];
  int32_t encoded_len;
  sns_memset(buffer, 0, sizeof(buffer));
//  sns_rc rc = SNS_RC_SUCCESS;
  sns_registry_read_req read_request;
  pb_buffer_arg data = (pb_buffer_arg){ .buf = reg_group_name,
    .buf_len = (strlen(reg_group_name) + 1) };

  read_request.name.arg = &data;
  read_request.name.funcs.encode = pb_encode_string_cb;

  encoded_len = pb_encode_request(buffer, sizeof(buffer),
      &read_request, sns_registry_read_req_fields, NULL);
  if(0 < encoded_len)
  {
    sns_request request = (sns_request){
      .request_len = encoded_len, .request = buffer,
      .message_id = SNS_REGISTRY_MSGID_SNS_REGISTRY_READ_REQ };
    state->reg_data_stream->api->send_request(state->reg_data_stream, &request);
  }

}
#else
sns_rc ct7117x_set_default_registry_cfg(sns_sensor *const this)
{
    ct7117x_state* state = (ct7117x_state *) this->state->state;
    sns_com_port_config *com_port_cfg = &state->com_port_info.com_config;

    // <general config>
    {
        state->is_dri         = CT7117X_DEFAULT_REG_CFG_ISDRI;
        state->supports_sync_stream =
            CT7117X_DEFAULT_REG_CFG_SUPPORT_SYN_STREAM;
        state->resolution_idx = CT7117X_DEFAULT_RES_IDX;
        state->registry_cfg_received = true;
    }

    // <platform configure>
    {
        sns_memscpy(
            com_port_cfg,
            sizeof(state->com_port_info.com_config),
            &( (sns_com_port_config) ct7117x_com_port_cfg_init_def ),
            sizeof(sns_com_port_config) );


        state->registry_rail_on_state  = CT7117X_DEFAULT_REG_CFG_RAIL_ON;
        strlcpy(state->rail_config.rails[0].name,
                RAIL_1,
                sizeof(state->rail_config.rails[0].name));
        strlcpy(state->rail_config.rails[1].name,
                RAIL_2,
                sizeof(state->rail_config.rails[1].name));
        state->rail_config.num_of_rails = NUM_OF_RAILS;

        state->registry_pf_cfg_received = true;
    }

#ifdef CT7117X_ENABLE_DUAL_SENSOR
    if(state->hardware_id)
    {
        com_port_cfg->slave_control = SLAVE_ADDRESS_1;
    }
#endif

    /**-----------------Register and Open COM Port-------------------------*/
    ct7117x_register_com_port(this);

    return SNS_RC_SUCCESS;
}
#endif
/*
*	
*/
void ct7117x_set_self_test_inst_config(sns_sensor *this,
                              sns_sensor_instance *instance)
{

  sns_request config;
  SNS_PRINTF(LOW, this, "ct7117x_set_self_test_inst_config");
  config.message_id = SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG;
  config.request_len = 0;
  config.request = NULL;

  this->instance_api->set_client_config(instance, &config);
}

/*
*	publish available	
* 센서 가용성 발행
*/
static void ct7117x_publish_available(sns_sensor *const this)
{
  SNS_PRINTF(LOW, this, "publish available");
  sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
  value.has_boolean = true;
  value.boolean = true;
  sns_publish_attribute(
      this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, true);
}

// 하드웨어 탐색 및 확인 함수
static bool ct7117x_discover_hw(sns_sensor * const this)
{
    ct7117x_state *state = (ct7117x_state*) this->state->state;
    /**-------------------Read and Confirm WHO-AM-I------------------------*/
    uint8_t buffer[1] = {0x07};
    bool hw_is_present = false;
    sns_rc rv = ct7117x_get_who_am_i(
        state->scp_service,state->com_port_info.port_handle, &buffer[0]);

    SNS_PRINTF(LOW, this, "who am i is 0x%x",buffer[0]);

    if (rv == SNS_RC_SUCCESS && buffer[0] == TEMP_WHOAMI_VALUE)
    {
        SNS_PRINTF(LOW, this, "RC_SUCCESS");
    }
    {
        ct7117x_get_calib_param(this);
        hw_is_present = true;
    }
    state->who_am_i = buffer[0];
    hw_is_present = true;
    /**------------------Power Down and Close COM Port--------------------*/
    state->scp_service->api->sns_scp_update_bus_power(
        state->com_port_info.port_handle,
        false);

    state->scp_service->api->sns_scp_close(state->com_port_info.port_handle);
    state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info.port_handle);

#if !CT7117X_POWERRAIL_DISABLED
    /**----------------------Turn Power Rail OFF--------------------------*/
    state->rail_config.rail_vote = SNS_RAIL_OFF;
    state->pwr_rail_service->api->sns_vote_power_rail_update(state->pwr_rail_service,
                                                             this,
                                                             &state->rail_config,
                                                             NULL);
#endif
    return hw_is_present;
}

/*
*	
*/
sns_rc ct7117x_sensor_notify_event(sns_sensor * const this)
{
  ct7117x_state *state = (ct7117x_state*) this->state->state;
  sns_service_manager *service_mgr = this->cb->get_service_manager(this);
  sns_stream_service *stream_svc = (sns_stream_service*)
              service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
  sns_time on_timestamp;
  sns_rc rv = SNS_RC_SUCCESS;
  sns_sensor_event *event = NULL;
  char buffer[60] = {0};

  SNS_PRINTF(LOW,this,"gu:enter sensor notify event");

  /*
  *	find all depend sensor suid
  * 의존성 센서들의 SUID 검색 및 레지스트리 스트림 생성
  */
  if (state->fw_stream) 
  {
    if ((0 == sns_memcmp(&state->acp_suid,
                           &((sns_sensor_uid){{0}}),
                           sizeof(state->acp_suid)))
     || (0 == sns_memcmp(&state->timer_suid,
                              &((sns_sensor_uid){{0}}),
                              sizeof(state->timer_suid)))
     || (0 == sns_memcmp(&state->irq_suid,
                              &((sns_sensor_uid){{0}}),
                              sizeof(state->irq_suid)))
#if !CT7117X_DISABLE_REGISTRY
     || (0 == sns_memcmp(&state->reg_suid,
                              &((sns_sensor_uid){{0}}),
                              sizeof(state->reg_suid)))
#endif
          )
    {
      ct7117x_sensor_process_suid_events(this);
#if !CT7117X_DISABLE_REGISTRY
      // place a request to registry sensor read request
      if((0 != sns_memcmp(&state->reg_suid,
                           &((sns_sensor_uid){{0}}),
                           sizeof(state->reg_suid))) &&
          (state->reg_data_stream == NULL))
      {
          stream_svc->api->create_sensor_stream(stream_svc,
              this, state->reg_suid , &state->reg_data_stream);

          ct7117x_sensor_create_registry_str(state->hardware_id,
                                             CT7117X_PLATFORM_CONFIG,
                                             buffer, sizeof(buffer));
          ct7117x_sensor_send_registry_request(this, buffer);


          ct7117x_sensor_create_registry_str(state->hardware_id,
                                             CT7117X_CONFIG_TEMP,
                                             buffer, sizeof(buffer));
          ct7117x_sensor_send_registry_request(this, buffer);
      }
#endif
    }
    else
    {
        sns_sensor_util_remove_sensor_stream(this, &state->fw_stream);
    }
  }

#if !CT7117X_POWERRAIL_DISABLED
  /**----------------------Handle a Timer Sensor event.-------------------*/
  if (NULL != state->timer_stream) 
  {
    event = state->timer_stream->api->peek_input(state->timer_stream);
    while (NULL != event)
    {
      pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)event->event,
                                                   event->event_len);
      sns_timer_sensor_event timer_event;
      SNS_PRINTF(LOW, this, "timer event message_id = %d", event->message_id);
      if(SNS_TIMER_MSGID_SNS_TIMER_SENSOR_EVENT == event->message_id)
      {
        if (pb_decode(&stream, sns_timer_sensor_event_fields, &timer_event))
        {
          /*check the hardware*/
          if (state->power_rail_pend_state == POWER_RAIL_PENDING_INIT)
          {
            state->hw_is_present = ct7117x_discover_hw(this);

            if (state->hw_is_present)
            {
               ct7117x_publish_available(this);
               SNS_PRINTF(ERROR, this, "sensor:%d initialize finished", state->sensor);
               
               /* change-20260204-hyungchul: Auto-create instance immediately after HW discovery */
               /* 하드웨어 발견 직후 인스턴스를 강제로 생성하여 클라이언트 없이도 동작 준비 */
               sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
               if (NULL == instance)
               {
                   instance = this->cb->create_instance(this, sizeof(ct7117x_instance_state));
                   SNS_PRINTF(HIGH, this, "change-20260204-hyungchul: Auto-created instance to force start");
               }
            }
            else
            {
               rv = SNS_RC_INVALID_STATE;
               SNS_PRINTF(ERROR, this, "temp HW absent");
            }
            state->power_rail_pend_state = POWER_RAIL_PENDING_NONE;
          }
		  /*set instance configure*/
          else if(state->power_rail_pend_state == POWER_RAIL_PENDING_SET_CLIENT_REQ)
          {
             sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
            if(NULL != instance)
            {
              ct7117x_instance_state *inst_state =
                 (ct7117x_instance_state *) instance->state->state;

             temp_reval_instance_config(this, instance, state->sensor);
              if(inst_state->new_self_test_request)
              {
                ct7117x_set_self_test_inst_config(this, instance);
              }
            }
            state->power_rail_pend_state  = POWER_RAIL_PENDING_NONE;
          }
        }
        else
        {
          SNS_PRINTF(ERROR, this, "pb_decode error");
        }
      }
      event = state->timer_stream->api->get_next_input(state->timer_stream);
    }
    /** Free up timer stream if not needed anymore */
    if (state->power_rail_pend_state == POWER_RAIL_PENDING_NONE)
    {
        sns_sensor_util_remove_sensor_stream(this, &state->timer_stream);
    }
  }
#endif

#if !CT7117X_DISABLE_REGISTRY
  /*registry event*/
  if(NULL != state->reg_data_stream)
  {
    SNS_PRINTF(LOW, this, "registry data stream");
    event = state->reg_data_stream->api->peek_input(state->reg_data_stream);
    while(NULL != event)
    {
      ct7117x_sensor_process_registry_event(this, event);
      if(state->registry_cfg_received)
      {
        ct7117x_publish_registry_attributes(this);
      }
      if(state->registry_pf_cfg_received)
      {
          /**-----------------Register and Open COM Port-------------------------*/
          ct7117x_register_com_port(this);
      }
      event = state->reg_data_stream->api->get_next_input(state->reg_data_stream);
    }
  }
#else
  if ( (state->com_port_info.port_handle == NULL) &&
      (state->pwr_rail_service == NULL) )
  {
    rv = ct7117x_set_default_registry_cfg(this);
    if(rv == SNS_RC_SUCCESS)
    {
      ct7117x_publish_registry_attributes(this);
    }
  }
#endif

#if !CT7117X_POWERRAIL_DISABLED
  /**---------------------Register Power Rails --------------------------*/
  if( !state->hw_is_present &&
      sns_suid_lookup_complete(&state->suid_lookup_data) &&
      state->registry_pf_cfg_received && state->registry_cfg_received &&
      NULL == state->pwr_rail_service)
  {
    state->rail_config.rail_vote = SNS_RAIL_OFF;
    state->pwr_rail_service =
        (sns_pwr_rail_service*)service_mgr->get_service(service_mgr,
                                                          SNS_POWER_RAIL_SERVICE);
    /* register power rails */
    state->pwr_rail_service->api->sns_register_power_rails(state->pwr_rail_service,
                                                           &state->rail_config);
    SNS_PRINTF(LOW, this, "register power rails: rail vote:%d, num of rails:%d [1]%s [2]%s rv=%d",
               state->rail_config.rail_vote,
               state->rail_config.num_of_rails,
               state->rail_config.rails[0].name,
               state->rail_config.rails[1].name,
               rv);

    /**---------------------Turn Power Rails ON----------------------------*/
    if (state->sensor == TEMP_TEMPERATURE)
    {
      state->rail_config.rail_vote = SNS_RAIL_ON_NPM;
    }
    else
    {
      state->rail_config.rail_vote = SNS_RAIL_OFF;
    }
    if( NULL != state->pwr_rail_service)
    {
      rv = state->pwr_rail_service->api->sns_vote_power_rail_update(
            state->pwr_rail_service, this, &state->rail_config, &on_timestamp);
        SNS_PRINTF(LOW, this, "vote power rails value:%d rv=%d",
                 state->rail_config.rail_vote, rv);
    }

    /**-------------Create a Timer stream for Power Rail ON timeout.--------------*/
    if (NULL == state->timer_stream)
    {
      SNS_PRINTF(LOW, this, "here create the timer stream");
      stream_svc->api->create_sensor_stream(stream_svc,
                                            this,
                                            state->timer_suid,
                                            &state->timer_stream);
      if (NULL != state->timer_stream)
      {
        ct7117x_sensor_start_power_rail_timer(
              this, sns_convert_ns_to_ticks(TEMP_OFF_TO_IDLE_MS * 1000 * 1000),
              POWER_RAIL_PENDING_INIT);
      }
    }
  }
#else
  state->hw_is_present = ct7117x_discover_hw(this);

  if (state->hw_is_present)
  {
    ct7117x_publish_available(this);
    SNS_PRINTF(LOW, this, "sensor:%d initialize finished", state->sensor);
  }
  else
  {
    rv = SNS_RC_INVALID_STATE;
    SNS_PRINTF(HIGH, this, "temp HW absent");
  }
#endif
  return rv;
}

/**
 * Decodes self test requests.
 *
 * @param[i] this              Sensor reference
 * @param[i] request           Encoded input request
 * @param[o] decoded_request   Decoded standard request
 * @param[o] test_config       decoded self test request
 *
 * @return bool True if decoding is successfull else false.
 */
static bool ct7117x_get_decoded_self_test_request(sns_sensor const *this, sns_request const *request,
                                                  sns_std_request *decoded_request,
                                                  sns_physical_sensor_test_config *test_config)
{
  pb_istream_t stream;
  pb_simple_cb_arg arg =
      { .decoded_struct = test_config,
        .fields = sns_physical_sensor_test_config_fields };
  decoded_request->payload = (struct pb_callback_s)
      { .funcs.decode = &pb_decode_simple_cb, .arg = &arg };
  stream = pb_istream_from_buffer(request->request,
                                  request->request_len);
  if(!pb_decode(&stream, sns_std_request_fields, decoded_request))
  {
    SNS_PRINTF(ERROR, this, "decode error");
    return false;
  }
  return true;
}


/**
 * Decodes a physical sensor self test request and updates
 * instance state with request info.
 *
 * @param[i] this      Sensor reference
 * @param[i] instance  Sensor Instance reference
 * @param[i] new_request Encoded request
 *
 * @return True if request is valid else false
 */
static bool ct7117x_extract_self_test_info(sns_sensor *this,
                              sns_sensor_instance *instance,
                              struct sns_request const *new_request)
{
  sns_std_request decoded_request;
  sns_physical_sensor_test_config test_config = sns_physical_sensor_test_config_init_default;
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  ct7117x_instance_state *inst_state = (ct7117x_instance_state*)instance->state->state;
  temp_self_test_info *self_test_info;

  if(state->sensor == TEMP_TEMPERATURE)
  {
    self_test_info = &inst_state->temperature_info.test_info; 
  }
  else
  {
    return false;
  }

  if(ct7117x_get_decoded_self_test_request(this, new_request, &decoded_request, &test_config))
  {
    self_test_info->test_type = test_config.test_type;
    self_test_info->test_client_present = true;
    return true;
  }
  else
  {
    return false;
  }
}

/**
 * Turns power rails off
 *
 * @paramp[i] this   Sensor reference
 *
 * @return none
 */
#if 0
static void ct7117x_turn_rails_off(sns_sensor *this)
{
  sns_sensor *sensor;

  for(sensor = this->cb->get_library_sensor(this, true);
      NULL != sensor;
      sensor = this->cb->get_library_sensor(this, false))
  {
    ct7117x_state *sensor_state = (ct7117x_state*)sensor->state->state;
    if(sensor_state->rail_config.rail_vote != SNS_RAIL_OFF)
    {
      sensor_state->rail_config.rail_vote = SNS_RAIL_OFF;
      sensor_state->pwr_rail_service->api->sns_vote_power_rail_update(sensor_state->pwr_rail_service,
                                                                      sensor,
                                                                      &sensor_state->rail_config,
                                                                      NULL);
    }
  }
}
#endif

#if 0
static void ct7117x_cancel_power_rail_timer(sns_sensor *const this)
{
  ct7117x_state *state = (ct7117x_state *)this->state->state;

  SNS_PRINTF(HIGH, this, "cancel_power_rail_timer: hw_id = %u, pend_state = %u",
             state->hardware_id, state->power_rail_pend_state);
  state->power_rail_pend_state = POWER_RAIL_PENDING_NONE;
  sns_sensor_util_remove_sensor_stream(this, &state->timer_stream);
}
#endif
static bool ct7117x_get_decoded_request(sns_sensor const *this, sns_request const *in_request,
    sns_std_request *decoded_request,
    sns_std_sensor_config *decoded_payload)
{

  UNUSED_VAR(this);
  pb_istream_t stream;
  pb_simple_cb_arg arg =
  { .decoded_struct = decoded_payload,
    .fields = sns_std_sensor_config_fields };
  decoded_request->payload = (struct pb_callback_s)
  { .funcs.decode = &pb_decode_simple_cb, .arg = &arg };
  stream = pb_istream_from_buffer(in_request->request,
      in_request->request_len);
  if(!pb_decode(&stream, sns_std_request_fields, decoded_request))
  {
    return false;
  }
  return true;
}

static sns_rc sns_ct7117x_handle_config(sns_sensor *const this,
     struct sns_request const *new_request)
{
  sns_rc rc = SNS_RC_NOT_SUPPORTED;
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  sns_std_request decoded_request;
  sns_std_sensor_config decoded_payload = {0};

  if(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG == new_request->message_id)
  {
    if(ct7117x_get_decoded_request(this, new_request, &decoded_request, &decoded_payload))
    {
      rc = SNS_RC_SUCCESS;
      if((state->sensor == TEMP_TEMPERATURE) &&
         decoded_payload.sample_rate > TEMP_ODR_0 &&
         decoded_payload.sample_rate <= TEMP_ODR_5)
      {
        rc = SNS_RC_SUCCESS;
      }
      else if((state->sensor == TEMP_TEMPERATURE) &&
         (decoded_payload.sample_rate <= TEMP_ODR_0||
         decoded_payload.sample_rate > TEMP_ODR_5))
      {
        rc = SNS_RC_INVALID_VALUE;
      }
    }
  }
   else if(SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG == new_request->message_id)
  {
      rc = SNS_RC_INVALID_TYPE;
  }
  return rc;
}

static void ct7117x_check_instance(sns_sensor *const this)
{
  sns_sensor_instance *instance = sns_sensor_util_get_shared_instance(this);
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  if(NULL != instance &&
     NULL == instance->cb->get_client_request(
                              instance, &state->my_suid, true))
  {
    /* change-20260204-hyungchul: Disable instance removal when clientless */
    /* 클라이언트가 없어도 인스턴스가 삭제되지 않도록 제거 로직을 주석 처리하여 유지 */
    // state->power_rail_pend_state = POWER_RAIL_PENDING_NONE;
    // sns_sensor_util_remove_sensor_stream(this, &state->timer_stream);
    // this->cb->remove_instance(instance);
    // ct7117x_turn_rails_off(this);
    SNS_PRINTF(HIGH, this, "change-20260204-hyungchul: Kept instance alive (ignored clientless removal)");
  }
}

sns_sensor_instance *ct7117x_sensor_set_client_request(sns_sensor *const this,
                                                struct sns_request const *exist_request,
                                                struct sns_request const *new_request,
                                                bool remove)
{
  sns_sensor_instance 	*instance = sns_sensor_util_get_shared_instance(this);
  ct7117x_state 		*state = (ct7117x_state *)this->state->state;

  bool 					reval_config = false;  
  sns_time 				on_timestamp;	
  sns_time 				delta;	


  SNS_PRINTF(HIGH, this, "### set_client_request - temp_id=%d/%d remove=%u",
                exist_request ? exist_request->message_id : -1,
                new_request ? new_request->message_id : -1, remove);
  
  if(remove)
  {
    if(NULL == instance) {
      return instance;
    }

    instance->cb->remove_client_request(instance, exist_request);

    /* Assumption: The FW will call deinit() on the instance before destroying it.
       Putting all HW resources (sensor HW, COM port, power rail)in
       low power state happens in Instance deinit().*/
    if(NULL != exist_request &&
        exist_request->message_id != SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG)
    {
      temp_reval_instance_config(this, instance, state->sensor);
    }
  }
  else
  {
    // 1. If new request then:
    //     a. Power ON rails.
    //     b. Power ON COM port - Instance must handle COM port power.
    //     c. Create new instance.
    //     d. Re-evaluate existing requests and choose appropriate instance config.
    //     e. set_client_config for this instance.
    //     f. Add new_request to list of requests handled by the Instance.
    //     g. Power OFF COM port if not needed- Instance must handle COM port power.
    //     h. Return the Instance.
    // 2. If there is an Instance already present:
    //     a. Add new_request to list of requests handled by the Instance.
    //     b. Remove exist_request from list of requests handled by the Instance.
    //     c. Re-evaluate existing requests and choose appropriate Instance config.
    //     d. set_client_config for the Instance if not the same as current config.
    //     e. publish the updated config.
    //     f. Return the Instance.
    // 3.  If "flush" request:
    //     a. Perform flush on the instance.
    //     b. Return NULL.

    if(NULL == instance && NULL != new_request
       && new_request->message_id != SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
    {
#if !CT7117X_POWERRAIL_DISABLED
      sns_time off2idle = sns_convert_ns_to_ticks(TEMP_OFF_TO_IDLE_MS*1000*1000);
      state->rail_config.rail_vote = state->registry_rail_on_state;
      state->pwr_rail_service->api->sns_vote_power_rail_update(
          state->pwr_rail_service,
          this,
          &state->rail_config,
          &on_timestamp);

      delta = sns_get_system_time() - on_timestamp;
      // Use on_timestamp to determine correct Timer value.
      SNS_PRINTF(HIGH, this,"debug set Power rails delta=%u o2i=%u",
                 (uint32_t)delta, (uint32_t)off2idle);
      if(delta < off2idle)
      {
        ct7117x_sensor_start_power_rail_timer(this,
            off2idle - delta,
            POWER_RAIL_PENDING_SET_CLIENT_REQ);
      } else {
        // rail is already ON
        reval_config = true;
      }
#else
      reval_config = true;
#endif
      /** create_instance() calls init() for the Sensor Instance */
      instance = this->cb->create_instance(this,
          sizeof(ct7117x_instance_state));
    }
    else if(NULL != instance)
    {
      if(NULL != new_request &&
            new_request->message_id == SNS_STD_MSGID_SNS_STD_FLUSH_REQ)
      {
        if(NULL != exist_request)
        {
          sns_sensor_util_send_flush_event(&state->my_suid, instance);
          instance = &sns_instance_no_error;
           SNS_PRINTF(HIGH, this," debug sns_instance_no_error");
        }
        else
        {
          instance = NULL;
          SNS_PRINTF(HIGH, this,"debug instance = NULL");
        }
        return instance;
      }
      else
      {
        reval_config = true;
        SNS_PRINTF(HIGH, this," debug reval_config = true");
      }
    }
    /** Add the new request to list of client_requests.*/
    if(NULL != instance)
    {
      ct7117x_instance_state *inst_state =
        (ct7117x_instance_state*)instance->state->state;
      bool request_supported = false;
      sns_rc decode_request_rc = SNS_RC_FAILED;

      if(NULL != new_request)
      {
        if (NULL != exist_request)
        {
          instance->cb->remove_client_request(instance, exist_request);
        }
        instance->cb->add_client_request(instance, new_request);
      }

      if(NULL != new_request &&
          ((new_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) ||
        (new_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_ON_CHANGE_CONFIG)))
      {
        decode_request_rc = sns_ct7117x_handle_config(this, new_request);
         SNS_PRINTF(HIGH, this," debug new_request->message_id == %d",new_request->message_id);
        if(SNS_RC_SUCCESS == decode_request_rc)
        {
          request_supported = true;
        }
      }
      else if((NULL != new_request) &&
                 (new_request->message_id == SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG))
      {
        SNS_PRINTF(LOW, this, "new_self_test_request received: Power rail [%u]",
                     state->power_rail_pend_state);
        if(ct7117x_extract_self_test_info(this, instance, new_request))
          {
            inst_state->new_self_test_request = true;
            sns_memscpy(&inst_state->irq_suid, sizeof(inst_state->irq_suid),
                        &state->irq_suid, sizeof(state->irq_suid));
            SNS_PRINTF(HIGH, this, "Self-test: Copying IRQ SUID. Valid? %d", 
                       (0 != sns_memcmp(&state->irq_suid, &((sns_sensor_uid){{0}}), sizeof(state->irq_suid))));
            ct7117x_set_self_test_inst_config(this, instance);
          }
          request_supported = true;
          temp_reval_instance_config(this, instance, state->sensor);
      }
  
      if(!request_supported)
      {
        sns_sensor_uid *suid = &state->my_suid;
        sns_std_error_event error_event;
        if(decode_request_rc == SNS_RC_NOT_SUPPORTED)
          error_event.error = SNS_STD_ERROR_NOT_SUPPORTED;
        else if(decode_request_rc == SNS_RC_FAILED)
          error_event.error = SNS_STD_ERROR_FAILED;
        else if(decode_request_rc == SNS_RC_INVALID_TYPE)
          error_event.error = SNS_STD_ERROR_INVALID_TYPE;
        else
          error_event.error = SNS_STD_ERROR_INVALID_VALUE;
        pb_send_event(instance,
                     sns_std_error_event_fields,
                     &error_event,
                     sns_get_system_time(),
                     SNS_STD_MSGID_SNS_STD_ERROR_EVENT,
                     suid);

        instance->cb->remove_client_request(instance, new_request);
        if(NULL != exist_request)
        {
          instance->cb->add_client_request(instance, exist_request);
        }
        instance = NULL;
      }
      else if(ct7117x_set_oem_client_request_msgid(this, new_request))
      {
        SNS_PRINTF(HIGH, this,"debug reval_config::%d && inst_state->instance_is_ready_to_configure::%d",
                     reval_config, inst_state->instance_is_ready_to_configure);
        if(reval_config)
        {
          temp_reval_instance_config(this, instance, state->sensor);
          SNS_PRINTF(HIGH, this,"debug reval_config::%d && inst_state->instance_is_ready_to_configure::%d",
                     reval_config, inst_state->instance_is_ready_to_configure);
        }
      }
    }
  }

  // Sensors are required to call remove_instance when clientless 
  ct7117x_check_instance(this);
  return instance;
}

/*
*	send suid request
* SUID 요청 전송
*/
void sns_see_temp_send_suid_req(sns_sensor *this, char * const data_type,
    uint32_t data_type_len)
{
  uint8_t buffer[50];
  sns_memset(buffer, 0, sizeof(buffer));
  ct7117x_state *state = (ct7117x_state*)this->state->state;
  sns_service_manager *manager = this->cb->get_service_manager(this);
  sns_stream_service *stream_service =
      (sns_stream_service*)manager->get_service(manager, SNS_STREAM_SERVICE);
  size_t encoded_len;
  pb_buffer_arg data = (pb_buffer_arg){.buf = data_type, .buf_len = data_type_len};

  sns_suid_req suid_req = sns_suid_req_init_default;
  suid_req.has_register_updates = true;
  suid_req.register_updates = true;
  suid_req.data_type.funcs.encode = &pb_encode_string_cb;
  suid_req.data_type.arg = &data;


  /* create the event/data stream for the platform resource */
  if (state->fw_stream == NULL) 
  {
    stream_service->api->create_sensor_stream(stream_service, this,
        sns_get_suid_lookup(), &state->fw_stream);
  }
  encoded_len = pb_encode_request(buffer,
          sizeof(buffer),
          &suid_req,
          sns_suid_req_fields,
          NULL);
  if (encoded_len > 0) 
  {
    sns_request request = (sns_request) {
          .request_len = encoded_len,
          .request = buffer,
          .message_id = SNS_SUID_MSGID_SNS_SUID_REQ };
    state->fw_stream->api->send_request(state->fw_stream, &request);
  }
}

/*
*	publish sensor attributes
* 센서 속성 발행
*/
static void temp_publish_attributes(sns_sensor * const this)
{
  ct7117x_state *state = (ct7117x_state*)this->state->state;
#if CT7117X_ATTRIBUTE_DISABLED
  {
    char const type_0[] = TEMPERATURE_TYPE_0;
    char const *type = type_0;
#ifdef CT7117X_ENABLE_DUAL_SENSOR
    char const type_1[] = TEMPERATURE_TYPE_1;
    if(state->hardware_id)
    {
        type = type_1;
    }
#endif
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
                      { .buf = type, .buf_len = strlen(type)+1 });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, true);
  }
#else
  {
    sns_std_attr_value_data values[] = { SNS_ATTR };
    sns_std_attr_value_data range1[] = { SNS_ATTR, SNS_ATTR };
    range1[0].has_flt = true;
    range1[0].flt = CT7117X_TEMPERATURE_RANGE_MIN;
    range1[1].has_flt = true;
    range1[1].flt = CT7117X_TEMPERATURE_RANGE_MAX;
    values[0].has_subtype = true;
    values[0].subtype.values.funcs.encode = sns_pb_encode_attr_cb;
    values[0].subtype.values.arg =
      &((pb_buffer_arg ) { .buf = range1, .buf_len = ARR_SIZE(range1) } );
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RANGES,
        values, ARR_SIZE(values), false);
  }
  { sns_std_attr_value_data values[] = {SNS_ATTR}; 
    values[0].has_sint = true;
	values[0].sint = stream_type;
	sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_STREAM_TYPE, values, ARR_SIZE(values), false);  
  } 
  {
    sns_std_attr_value_data values[] = {SNS_ATTR, SNS_ATTR};
    values[0].has_sint = true;
    values[0].sint = CT7117X_TEMPERATURE_LOW_POWER_CURRENT;
    values[1].has_sint = true;
    values[1].sint = CT7117X_TEMPERATURE_NORMAL_POWER_CURRENT;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_ACTIVE_CURRENT,
        values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR, SNS_ATTR};
    values[0].has_flt = true;
    values[0].flt = TEMP_ODR_1;
	values[1].has_flt = true;
	values[1].flt = TEMP_ODR_5;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RATES,
        values, ARR_SIZE(values), false);
  } 
  {
    sns_std_attr_value_data values[] = {SNS_ATTR};
    values[0].has_flt = true;
    values[0].flt = CT7117X_TEMPERATURE_RESOLUTION;
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_RESOLUTIONS,
        values, ARR_SIZE(values), false);
  }
  {
    sns_std_attr_value_data values[] = {SNS_ATTR, SNS_ATTR};
    char const op_mode1[] = TEMP_LPM;
    char const op_mode2[] = TEMP_NORMAL;

    values[0].str.funcs.encode = pb_encode_string_cb;
    values[0].str.arg = &((pb_buffer_arg)
        { .buf = op_mode1, .buf_len = sizeof(op_mode1) });
    values[1].str.funcs.encode = pb_encode_string_cb;
    values[1].str.arg = &((pb_buffer_arg)
        { .buf = op_mode2, .buf_len = sizeof(op_mode2) });

    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_OP_MODES,
        values, ARR_SIZE(values), false);
  }
  {
    char const proto_0[] = TEMPERATURE_PROTO_0;
    char const *proto = proto_0;
#ifdef CT7117X_ENABLE_DUAL_SENSOR
    char const proto_1[] = TEMPERATURE_PROTO_1;
    if(state->hardware_id)
    {
      proto = proto_1;
    }
#endif
    sns_std_attr_value_data values[] = {SNS_ATTR};
    values[0].str.funcs.encode = pb_encode_string_cb;
    values[0].str.arg = &((pb_buffer_arg)
        { .buf = proto, .buf_len = strlen(proto)+1 });
    sns_publish_attribute(this, SNS_STD_SENSOR_ATTRID_API,
        values, ARR_SIZE(values), false);
  }
  {
    char const name[] = "CT7117X-ADSP";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = name, .buf_len = sizeof(name) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_NAME, &value, 1, false);
  }
  {
    char const type_0[] = TEMPERATURE_TYPE_0;
    char const *type = type_0;
#ifdef CT7117X_ENABLE_DUAL_SENSOR
    char const type_1[] = TEMPERATURE_TYPE_1;
    if(state->hardware_id)
    {
      type = type_1;
    }
#endif
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = type, .buf_len = strlen(type)+1 });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_TYPE, &value, 1, false);
  }
  {
    char const vendor[] = "LGE-Himax";
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.str.funcs.encode = pb_encode_string_cb;
    value.str.arg = &((pb_buffer_arg)
        { .buf = vendor, .buf_len = sizeof(vendor) });
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_VENDOR, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = false;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_AVAILABLE, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = false;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_DYNAMIC, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = 0X0100;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_VERSION, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = 0;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_FIFO_SIZE, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = CT7117X_TEMPERATURE_SLEEP_CURRENT;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_SLEEP_CURRENT, &value, 1, false);
  }
  {
    float data[1] = {0};
    state->encoded_event_len =
        pb_get_encoded_size_sensor_stream_event(data, 1);
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_sint = true;
    value.sint = state->encoded_event_len;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_EVENT_SIZE, &value, 1, false);
  }
  {
    sns_std_attr_value_data value = sns_std_attr_value_data_init_default;
    value.has_boolean = true;
    value.boolean = true;
    sns_publish_attribute(
        this, SNS_STD_SENSOR_ATTRID_PHYSICAL_SENSOR, &value, 1, true);
  }
#endif
}

/*
*	temperature sensor init
* 온도 센서 초기화 (SUID 설정, 스트림 생성 등)
*/
sns_rc ct7117x_temperature_init(sns_sensor * const this)
{
  ct7117x_state *state = (ct7117x_state*) this->state->state;
  struct sns_service_manager *smgr = this->cb->get_service_manager(this);
  state->diag_service =
      (sns_diag_service *) smgr->get_service(smgr, SNS_DIAG_SERVICE);

  state->scp_service= (sns_sync_com_port_service *)
    smgr->get_service(smgr,SNS_SYNC_COM_PORT_SERVICE);
  state->sensor = TEMP_TEMPERATURE;
  state->sensor_client_present = false;

  sns_sensor_uid suid = (sns_sensor_uid) TEMPERATURE_SUID_0;
#ifdef CT7117X_ENABLE_DUAL_SENSOR
  state->hardware_id = this->cb->get_registration_index(this);
  if(state->hardware_id)
  {
      suid = (sns_sensor_uid) TEMPERATURE_SUID_1;
  }
#endif
  
  SNS_PRINTF(ERROR, this, "start register temp %d", state->hardware_id);

  sns_memscpy(&state->my_suid, sizeof(state->my_suid),
              &suid, sizeof(sns_sensor_uid));

  temp_publish_attributes(this);
  sns_see_temp_send_suid_req(this, "async_com_port", 15);
  sns_see_temp_send_suid_req(this, "timer", 6);
  sns_see_temp_send_suid_req(this, "interrupt", 9);
#if !CT7117X_DISABLE_REGISTRY
  sns_see_temp_send_suid_req(this, "registry", 9);
#endif
  return SNS_RC_SUCCESS;
}

/*
*	temperature sensor deinit
* 온도 센서 해제
*/
sns_rc ct7117x_temperature_deinit(sns_sensor * const this)
{
  UNUSED_VAR(this);
  return SNS_RC_SUCCESS;
}
