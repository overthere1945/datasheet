/*
 * 파일 목적: ADSP 환경에서 Himax DSP와 SPI 통신을 통해 가변 크기의 JPEG 데이터를 수신하고 공유 메모리에 적재하는 센서 인스턴스 동작 정의
 * 파일 기능:
 * 1. 인터럽트 발생 시 SPI 통신으로 헤더를 수신 및 검증
 * 2. 공유 메모리에 복사 및 플래그 동기화
 * 3. (해결) Protobuf 강제 캐스팅 버그를 pb_decode로 수정하여 ODR(샘플링 속도)을 정상화하고 이벤트 드롭을 해결함
 */

#include "sns_mem_util.h"
#include "sns_sensor_instance.h"
#include "sns_service_manager.h"
#include "sns_stream_service.h"
#include "sns_rc.h"
#include "sns_request.h"
#include "sns_time.h"
#include "sns_sensor_event.h"
#include "sns_types.h"
#include "sns_event_service.h"
#include "sns_memmgr.h"
#include "sns_com_port_priv.h"

#include "sns_ct7117x_hal.h"
#include "sns_ct7117x_sensor.h"
#include "sns_ct7117x_sensor_instance.h"

#include "sns_interrupt.pb.h"
#include "sns_async_com_port.pb.h"
#include "sns_timer.pb.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sns_pb_util.h"
#include "sns_async_com_port_pb_utils.h"
#include "sns_diag_service.h"
#include "sns_diag.pb.h"
#include "sns_sensor_util.h"
#include "sns_sync_com_port_service.h"
#include "sns_island.h"
#include <qurt.h>
#include <stdio.h>

#define SHARED_PHYS_ADDR  0x81EC0000
#define SHARED_SIZE       0x20000 

unsigned int myddr_base_addr = 0;
void *psram_virtual_addr = NULL;

qurt_mem_pool_t hwio_pool = 0;
qurt_mem_region_t shared_mem_region = 0;
qurt_mem_region_attr_t hwio_attr;
int qurt_init = 0;
int int_count = 0;

void ct7117x_start_sensor_temp_polling_timer(sns_sensor_instance *this)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;
    sns_timer_sensor_config req_payload = sns_timer_sensor_config_init_default;
    uint8_t buffer[50] = {0};
    sns_request timer_req = {
        .message_id = SNS_TIMER_MSGID_SNS_TIMER_SENSOR_CONFIG,
        .request    = buffer
    };
    sns_rc rc = SNS_RC_SUCCESS;

    if(NULL == state->temperature_timer_data_stream) {
        sns_service_manager *smgr = this->cb->get_service_manager(this);
        sns_stream_service *srtm_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
        rc = srtm_svc->api->create_sensor_instance_stream(srtm_svc,
                this, state->timer_suid, &state->temperature_timer_data_stream);
    }

    if(SNS_RC_SUCCESS != rc || NULL == state->temperature_timer_data_stream) {
        return;
    }

    req_payload.is_periodic = true;
    req_payload.start_time = sns_get_system_time();
    req_payload.timeout_period = state->temperature_info.sampling_intvl;

    timer_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload,
            sns_timer_sensor_config_fields, NULL);
    if(timer_req.request_len > 0) {
        state->temperature_timer_data_stream->api->send_request(state->temperature_timer_data_stream, &timer_req);
        state->temperature_info.timer_is_active = true;
    }
}

void ct7117x_handle_interrupt_event(sns_sensor_instance *const instance, sns_time timestamp)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
    SNS_INST_PRINTF(LOW, instance, "interrupt event");
    state->interrupt_timestamp = timestamp;

    uint8_t buffer[2048]; 
    uint32_t xfer_bytes = 0;
    uint32_t total_read_bytes = 0;

    if(int_count < 30) int_count=int_count+1;
    if(int_count > 20 && qurt_init <= 3)
    {
        if(QURT_EOK == qurt_mem_pool_attach("smem_pool", &hwio_pool))
        {
            qurt_mem_region_attr_init(&hwio_attr);
            qurt_mem_region_attr_set_cache_mode(&hwio_attr, QURT_MEM_CACHE_NONE_SHARED);
            qurt_mem_region_attr_set_mapping(&hwio_attr, QURT_MEM_MAPPING_PHYS_CONTIGUOUS);
            qurt_mem_region_attr_set_physaddr(&hwio_attr, SHARED_PHYS_ADDR);
            qurt_init = 1;

            if (QURT_EOK != qurt_mem_region_create(&shared_mem_region, SHARED_SIZE, hwio_pool, &hwio_attr)) {
                qurt_init = 2; 
            } else {
                if (QURT_EOK != qurt_mem_region_attr_get(shared_mem_region, &hwio_attr)) {
                    qurt_init = 3; 
                } else {
                    qurt_mem_region_attr_get_virtaddr(&hwio_attr, &myddr_base_addr);
                    qurt_init = 4; 
                }
            }
        }
    }

    if(qurt_init <= 3) {
        SNS_INST_PRINTF(HIGH, instance, "qurt_init: %d, int_count: %d", qurt_init, int_count);
    }
    
    {  
        sns_island_exit_internal();
        
        uint32_t header_len = 11;
        uint32_t jpeg_size = 0;
        uint32_t payload_read_bytes = 0;
        uint8_t *shared_mem_ptr = (uint8_t *)myddr_base_addr;

        uint32_t first_chunk = sizeof(buffer); 
        state->com_read(state->scp_service, state->com_port_info.port_handle,
                        0x00, buffer, first_chunk, &xfer_bytes);
        total_read_bytes += xfer_bytes;

        if(buffer[0] == 0xC0 && buffer[1] == 0x5A && buffer[2] == 0x01)
        {
            jpeg_size = (buffer[6] << 24) | (buffer[5] << 16) | (buffer[4] << 8) | buffer[3];
            SNS_INST_PRINTF(HIGH, instance, "Header Parsed successfully. JPEG Size: %u bytes", jpeg_size);

            if((header_len + jpeg_size + 1) > SHARED_SIZE) {
                jpeg_size = SHARED_SIZE - header_len - 1; 
            }

            if(qurt_init == 4 && shared_mem_ptr != NULL) {
                buffer[0] = 0x00; 
                memcpy(&shared_mem_ptr[1], buffer, xfer_bytes);
            }

            if (xfer_bytes > header_len) {
                payload_read_bytes += (xfer_bytes - header_len);
            }

            while(payload_read_bytes < jpeg_size) {
                uint32_t remain = jpeg_size - payload_read_bytes;
                uint32_t chunk = remain > sizeof(buffer) ? sizeof(buffer) : remain;
                
                state->com_read(state->scp_service, state->com_port_info.port_handle,
                                0x00, buffer, chunk, &xfer_bytes);

                if(qurt_init == 4 && shared_mem_ptr != NULL) {
                    void* current_dest = (void*)(shared_mem_ptr + 1 + header_len + payload_read_bytes);
                    memcpy(current_dest, buffer, xfer_bytes);
                }
                
                payload_read_bytes += xfer_bytes;
                total_read_bytes += xfer_bytes;
            }

            qurt_mem_barrier(); 

            if(qurt_init == 4 && shared_mem_ptr != NULL) {
                shared_mem_ptr[0] = 0xC0; 
            }

            sns_time current_sys_time = sns_get_system_time();
            
            static float dummy_temp_data = 1.0f;
            dummy_temp_data += 10.0f;             
            if (dummy_temp_data > 1000.0f) dummy_temp_data = 1.0f; 

            pb_send_sensor_stream_event(instance,
                                        &state->temperature_info.suid,
                                        current_sys_time, 
                                        SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT,
                                        SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH,
                                        &dummy_temp_data,
                                        1,
                                        state->encoded_imu_event_len);
                                        
            SNS_INST_PRINTF(HIGH, instance, "Interrupt Event sent AFTER data copy! Dummy Temp: %d", (int)dummy_temp_data);
        }
        else
        {
            SNS_INST_PRINTF(ERROR, instance, "Header Sync Failed!");
        }
    }
    SNS_INST_PRINTF(HIGH, instance, "Total read bytes: %u", total_read_bytes);
}

void ct7117x_send_config_event(sns_sensor_instance *const instance)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)instance->state->state;
    sns_std_sensor_physical_config_event phy_sensor_config = sns_std_sensor_physical_config_event_init_default;
    char operating_mode[] = "NORMAL";
    pb_buffer_arg op_mode_args;

    op_mode_args.buf = &operating_mode[0];
    op_mode_args.buf_len = sizeof(operating_mode);

    phy_sensor_config.has_sample_rate = true;
    phy_sensor_config.has_water_mark = true;
    phy_sensor_config.water_mark = 1;
    phy_sensor_config.operation_mode.funcs.encode = &pb_encode_string_cb;
    phy_sensor_config.operation_mode.arg = &op_mode_args;
    phy_sensor_config.has_active_current = true;
    phy_sensor_config.has_resolution = true;
    phy_sensor_config.range_count = 2;
    phy_sensor_config.stream_is_synchronous = false;
    phy_sensor_config.has_dri_enabled= true;
    phy_sensor_config.dri_enabled=false;

    if(state->deploy_info.publish_sensors & TEMP_TEMPERATURE)
    {
        phy_sensor_config.sample_rate = state->temperature_info.sampling_rate_hz;
        phy_sensor_config.has_active_current = true;
        phy_sensor_config.active_current = 3;
        phy_sensor_config.resolution = CT7117X_TEMPERATURE_RESOLUTION;
        phy_sensor_config.range_count = 2;
        phy_sensor_config.range[0] = CT7117X_TEMPERATURE_RANGE_MIN;
        phy_sensor_config.range[1] = CT7117X_TEMPERATURE_RANGE_MAX;

        pb_send_event(instance,
                sns_std_sensor_physical_config_event_fields,
                &phy_sensor_config,
                sns_get_system_time(),
                SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT,
                &state->temperature_info.suid);
    }
}

static void inst_cleanup(ct7117x_instance_state *state, sns_stream_service *stream_mgr)
{
    if(NULL != state->async_com_port_data_stream) {
        stream_mgr->api->remove_stream(stream_mgr, state->async_com_port_data_stream);
        state->async_com_port_data_stream = NULL;
    }
    if(NULL != state->temperature_timer_data_stream) {
        stream_mgr->api->remove_stream(stream_mgr, state->temperature_timer_data_stream);
        state->temperature_timer_data_stream = NULL;
    }
    if(NULL != state->interrupt_data_stream) {
        stream_mgr->api->remove_stream(stream_mgr, state->interrupt_data_stream);
        state->interrupt_data_stream = NULL;
    }
    if(NULL != state->scp_service) {
        state->scp_service->api->sns_scp_close(state->com_port_info.port_handle);
        state->scp_service->api->sns_scp_deregister_com_port(&state->com_port_info.port_handle);
        state->scp_service = NULL;
    }
}

void ct7117x_set_temperature_config(sns_sensor_instance *const this)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*)this->state->state;

    if(state->temperature_info.sampling_intvl > 0)
    {
        if (state->is_dri)
        {
            SNS_INST_PRINTF(HIGH, this, "Set Config: NORMAL (DRI Active)");
        }
        else
        {
            ct7117x_start_sensor_temp_polling_timer(this);
        }
    }
    else
    {
        if (!state->is_dri)
        {
            state->temperature_info.timer_is_active = false;
        }
        SNS_INST_PRINTF(LOW, this, "Set Config: SLEEP");
    }
}

sns_rc ct7117x_temp_inst_init(sns_sensor_instance * const this, sns_sensor_state const *sstate)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;
    ct7117x_state *sensor_state = (ct7117x_state*) sstate->state;
    float stream_data[1] = {0};
    
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*) service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);

    uint64_t buffer[10];
    pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)buffer, sizeof(buffer));
    sns_diag_batch_sample batch_sample = sns_diag_batch_sample_init_default;
    uint8_t arr_index = 0;
    float diag_temp[TEMP_NUM_AXES];
    pb_float_arr_arg arg = {.arr = (float*)diag_temp, .arr_len = TEMP_NUM_AXES, .arr_index = &arr_index};
    batch_sample.sample.funcs.encode = &pb_encode_float_arr_cb;
    batch_sample.sample.arg = &arg;

    state->scp_service = (sns_sync_com_port_service*) service_mgr->get_service(service_mgr, SNS_SYNC_COM_PORT_SERVICE);

    stream_mgr->api->create_sensor_instance_stream(stream_mgr, this, sensor_state->acp_suid, &state->async_com_port_data_stream);

    sns_memscpy(&state->com_port_info.com_config, sizeof(state->com_port_info.com_config),
            &sensor_state->com_port_info.com_config, sizeof(sensor_state->com_port_info.com_config));

    state->scp_service->api->sns_scp_register_com_port(&state->com_port_info.com_config, &state->com_port_info.port_handle);

    if(NULL == state->async_com_port_data_stream || NULL == state->com_port_info.port_handle) {
        inst_cleanup(state, stream_mgr);
        return SNS_RC_FAILED;
    }

    sns_memscpy(&state->temperature_info.suid, sizeof(state->temperature_info.suid), &sensor_state->my_suid, sizeof(state->temperature_info.suid));
    sns_memscpy(&state->timer_suid, sizeof(state->timer_suid), &sensor_state->timer_suid, sizeof(sensor_state->timer_suid));
    sns_memscpy(&state->irq_suid, sizeof(state->irq_suid), &sensor_state->irq_suid, sizeof(sensor_state->irq_suid));
    
    state->irq_num = 102; 

    sns_memscpy(&state->calib_param, sizeof(state->calib_param), &sensor_state->calib_param, sizeof(sensor_state->calib_param));
    state->interface = sensor_state->com_port_info.com_config.bus_instance;

    state->is_dri = true; 
    state->op_mode = FORCED_MODE;
    
    state->com_read = ct7117x_com_read_wrapper;
    state->com_write = ct7117x_com_write_wrapper;

    state->encoded_imu_event_len = pb_get_encoded_size_sensor_stream_event(stream_data, 1);
    state->diag_service =  (sns_diag_service*) service_mgr->get_service(service_mgr, SNS_DIAG_SERVICE);

    state->scp_service->api->sns_scp_open(state->com_port_info.port_handle);
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, false);

    {
        sns_data_stream* data_stream = state->async_com_port_data_stream;
        sns_com_port_config* com_config = &sensor_state->com_port_info.com_config;
        uint8_t pb_encode_buffer[100];
        sns_request async_com_port_request = { .message_id  = SNS_ASYNC_COM_PORT_MSGID_SNS_ASYNC_COM_PORT_CONFIG, .request     = &pb_encode_buffer };

        state->ascp_config.bus_type          = (com_config->bus_type == SNS_BUS_I2C) ? SNS_ASYNC_COM_PORT_BUS_TYPE_I2C : SNS_ASYNC_COM_PORT_BUS_TYPE_SPI;
        state->ascp_config.slave_control     = com_config->slave_control;
        state->ascp_config.reg_addr_type     = SNS_ASYNC_COM_PORT_REG_ADDR_TYPE_8_BIT;
        state->ascp_config.min_bus_speed_kHz = com_config->min_bus_speed_KHz;
        state->ascp_config.max_bus_speed_kHz = com_config->max_bus_speed_KHz;
        state->ascp_config.bus_instance      = com_config->bus_instance;

        async_com_port_request.request_len = pb_encode_request(pb_encode_buffer, sizeof(pb_encode_buffer), &state->ascp_config, sns_async_com_port_config_fields, NULL);
        data_stream->api->send_request(data_stream, &async_com_port_request);
    }

    if(pb_encode_tag(&stream, PB_WT_STRING, sns_diag_sensor_state_raw_sample_tag)) {
        if(pb_encode_delimited(&stream, sns_diag_batch_sample_fields, &batch_sample)) {
            state->log_raw_encoded_size = stream.bytes_written;
        }
    }

    if (state->is_dri)
    {
        if (NULL == state->interrupt_data_stream) {
            sns_service_manager *smgr = this->cb->get_service_manager(this);
            sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
            stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
        }

        if (NULL != state->interrupt_data_stream) {
            uint8_t buffer[20];
            sns_request irq_req = { .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ, .request = buffer };
            sns_interrupt_req req_payload = sns_interrupt_req_init_default;

            req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
            req_payload.interrupt_num = 102; 
            req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
            req_payload.is_chip_pin = true;
            req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

            irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
            if (irq_req.request_len > 0) {
                state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
            }
        }
    }

    state->temperature_info.sampling_rate_hz = 5.0f;
    state->temperature_info.sampling_intvl = sns_convert_ns_to_ticks(1000000000.0 / state->temperature_info.sampling_rate_hz);

    ct7117x_set_temperature_config(this);

    return SNS_RC_SUCCESS;
}

sns_rc ct7117x_temp_inst_deinit(sns_sensor_instance *const this)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;
    sns_service_manager *service_mgr = this->cb->get_service_manager(this);
    sns_stream_service *stream_mgr = (sns_stream_service*) service_mgr->get_service(service_mgr, SNS_STREAM_SERVICE);
    inst_cleanup(state, stream_mgr);
    return SNS_RC_SUCCESS;
}

sns_rc ct7117x_temp_inst_set_client_config(sns_sensor_instance * const this, sns_request const *client_request)
{
    ct7117x_instance_state *state = (ct7117x_instance_state*) this->state->state;

    state->client_req_id = client_request->message_id;
    
    // change(fix)-hyungchul-20260312-1830 시작
    // 설명: [컴파일 에러 해결] 이전 코드에서 에러 처리 로직을 제거함에 따라 사용되지 않게 된
    // 변수들로 인한 '-Werror,-Wunused-variable' 컴파일 에러를 막기 위해 선언부를 주석 처리합니다.
    // sns_service_manager *mgr = this->cb->get_service_manager(this);
    // sns_event_service *event_service = (sns_event_service*)mgr->get_service(mgr, SNS_EVENT_SERVICE);
    // change(fix)-hyungchul-20260312-1830 끝

    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, true);

    if (client_request->message_id == SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_CONFIG) 
    {
        // change(fix)-hyungchul-20260312-2100 시작
        // 설명: [치명적 버그 수정] 압축된 Protobuf 바이트 배열을 C 구조체 포인터로 강제 캐스팅하여 
        // 쓰레기값이 들어가고 이벤트가 영구 차단(Drop)되던 문제를 pb_decode를 사용하여 정석대로 수정합니다.
        float desired_sample_rate = 5.0f; // 기본 샘플링 속도 5Hz
        
        sns_std_sensor_config req_payload = sns_std_sensor_config_init_default;
        pb_istream_t stream = pb_istream_from_buffer((pb_byte_t*)client_request->request, client_request->request_len);
        
        if (pb_decode(&stream, sns_std_sensor_config_fields, &req_payload)) {
            desired_sample_rate = req_payload.sample_rate;
            SNS_INST_PRINTF(HIGH, this, "Client Config Decoded. Rate: %d", (int)desired_sample_rate);
        } else {
            SNS_INST_PRINTF(ERROR, this, "PB Decode Failed! Using default 5Hz");
        }

        if (desired_sample_rate == 0.0f) {
            desired_sample_rate = 5.0f;
        }

        // 쓰레기값 조건문을 삭제하고 무조건 ODR(샘플링 주기)을 갱신합니다.
        state->temperature_info.sampling_rate_hz = desired_sample_rate;
        state->temperature_info.sampling_intvl = sns_convert_ns_to_ticks(1000000000.0 / desired_sample_rate);
        
        ct7117x_set_temperature_config(this);
        ct7117x_send_config_event(this);

        // SEE 프레임워크 강제 활성화를 위한 초기화 더미 이벤트 전송
        sns_time current_sys_time = sns_get_system_time();
        float initial_dummy = 1.0f;
        pb_send_sensor_stream_event(this,
                                    &state->temperature_info.suid,
                                    current_sys_time,
                                    SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT,
                                    SNS_STD_SENSOR_SAMPLE_STATUS_ACCURACY_HIGH,
                                    &initial_dummy,
                                    1,
                                    state->encoded_imu_event_len);
        // change(fix)-hyungchul-20260312-2100 끝
    }
    else if (client_request->message_id == SNS_PHYSICAL_SENSOR_TEST_MSGID_SNS_PHYSICAL_SENSOR_TEST_CONFIG) 
    {
        bool is_irq_suid_valid = (0 != sns_memcmp(&state->irq_suid, &((sns_sensor_uid){{0}}), sizeof(state->irq_suid)));

        if (NULL == state->interrupt_data_stream && is_irq_suid_valid) {
            sns_service_manager *smgr = this->cb->get_service_manager(this);
            sns_stream_service *stream_svc = (sns_stream_service*)smgr->get_service(smgr, SNS_STREAM_SERVICE);
            stream_svc->api->create_sensor_instance_stream(stream_svc, this, state->irq_suid, &state->interrupt_data_stream);
        }

        if (NULL != state->interrupt_data_stream) {
            uint8_t buffer[20];
            sns_request irq_req = { .message_id = SNS_INTERRUPT_MSGID_SNS_INTERRUPT_REQ, .request = buffer };
            sns_interrupt_req req_payload = sns_interrupt_req_init_default;
            req_payload.interrupt_trigger_type = SNS_INTERRUPT_TRIGGER_TYPE_RISING;
            req_payload.interrupt_num = 102;
            req_payload.interrupt_pull_type = SNS_INTERRUPT_PULL_TYPE_PULL_DOWN;
            req_payload.is_chip_pin = true;
            req_payload.interrupt_drive_strength = SNS_INTERRUPT_DRIVE_STRENGTH_2_MILLI_AMP;

            irq_req.request_len = pb_encode_request(buffer, sizeof(buffer), &req_payload, sns_interrupt_req_fields, NULL);
            if (irq_req.request_len > 0) {
                state->interrupt_data_stream->api->send_request(state->interrupt_data_stream, &irq_req);
            }
        } 
        state->new_self_test_request = false;
    }
    state->scp_service->api->sns_scp_update_bus_power(state->com_port_info.port_handle, false);

    return SNS_RC_SUCCESS;
}
