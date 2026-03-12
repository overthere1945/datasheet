/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "sensor_factory.h"
#include "qsh_sensor.h"
#include "sns_std_sensor.pb.h"
/* Add 2026-03-09 start */
#include <mutex>
#include <vector>
#include "sensors_qti.h"
#include <log/log.h> // 안드로이드 표준 Logcat 출력을 위한 헤더 추가
/* Add 2026-03-09 end   */

static const char *QSH_DATATYPE_AMBIENT_TEMP = "ambient_temperature";

class ambient_temperature : public qsh_sensor
{
public:
//    ambient_temperature(suid sensor_uid, sensor_wakeup_type wakeup);              // remove 2026-03-09
    ambient_temperature(suid sensor_uid, sensor_wakeup_type wakeup, int hub_id);    // add 2026-03-09
    static const char* qsh_datatype() { return QSH_DATATYPE_AMBIENT_TEMP; }
    virtual void activate() override;
    virtual void deactivate() override;

// add 2026-03-12
private:
    // change(fix)-hyungchul-20260312-1630 시작
    // 설명: HAL 기본 클래스가 이벤트를 검열하고 씹는(Drop) 현상을 막기 위해,
    // 이벤트를 가로채서 안드로이드 OS로 무조건 강제 발송하도록 함수를 오버라이드 합니다.
    virtual void handle_sns_client_event(const sns_client_event_msg_sns_client_event& pb_event) override;
    // change(fix)-hyungchul-20260312-1630 끝
};

//ambient_temperature::ambient_temperature(suid sensor_uid, sensor_wakeup_type wakeup):
ambient_temperature::ambient_temperature(suid sensor_uid, sensor_wakeup_type wakeup, int hub_id):
    qsh_sensor(sensor_uid, wakeup)
{ 
/*  remove hyungchul 2026-03-12  
    set_type(SENSOR_TYPE_AMBIENT_TEMPERATURE);
    set_string_type(SENSOR_STRING_TYPE_AMBIENT_TEMPERATURE);
    set_sensor_typename("Ambient Temperature Sensor");    
*/    
    // change(fix)-hyungchul-20260312-1330 시작
    // 설명: 안드로이드 OS가 온도 센서(Type 13)에 대해 ON_CHANGE 모드를 강제 할당하여 인터럽트를 드롭시키는 현상을 원천 차단하기 위해,
    // 규격(1축, float 1개)이 정확히 일치하면서 동시에 연속 모드(Continuous)가 기본인 '압력 센서(Pressure, Type 6)'로 프레임워크에 신분을 위장합니다.
    // 주의: ADSP와의 통신 파이프라인 문자열("ambient_temperature")은 그대로 유지합니다.
    set_type(SENSOR_TYPE_PRESSURE);
    set_string_type(SENSOR_STRING_TYPE_PRESSURE);
    // Java 앱(MainActivity.java)이 "CT7117X" 키워드로 검색할 수 있도록 이름 지정
    set_sensor_typename("CT7117X-ADSP Image Stream");

    
    // change(fix)-hyungchul-20260312-1200 시작
    // 설명: 안드로이드 OS의 인터럽트 드롭 현상을 막기 위해, 이벤트를 무조건 통과시키는 연속 모드로 강제 변경합니다.
    set_reporting_mode(SENSOR_FLAG_CONTINUOUS_MODE);
    // change(fix)-hyungchul-20260312-1200 끝

    set_nowk_msgid(SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_PHYSICAL_CONFIG_EVENT);
//    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info());  // remove 2026-03-09

/*  add 2026-03-09 start    */
    // Detected Hub ID?? ???????.
    _qsh_intf_id = qsh_intf_resource_manager::get_instance().register_sensor(get_sensor_info(), hub_id);
    // sns_loge("[cbc] ambient_temperature: Constructor called. Force activating sensor...");
    // Debug: Force activate sensor to receive data without client request
//    sensor_params params;
//    params.sample_period_ns = 200000000; // 200ms (5Hz)
//    params.max_latency_ns = 0;
//    set_config(params);
//    activate();
    // sns_loge("[cbc] ambient_temperature: Force activation requested.");
/*  end 2026-03-09  */    

}

// change(add)-hyungchul-20260312-1700 시작
// 설명: 안드로이드 프레임워크가 센서를 정상적으로 켜고 끄는지 추적하기 위한 오버라이드
void ambient_temperature::activate()
{
    ALOGI("[VISION_HAL] activate() called from Android Framework!");
    qsh_sensor::activate();
}

void ambient_temperature::deactivate()
{
    ALOGI("[VISION_HAL] deactivate() called from Android Framework!");
    qsh_sensor::deactivate();
}
// change(add)-hyungchul-20260312-1700 끝

// change(fix)-hyungchul-20260312-1630 시작
// 설명: can_submit_sample 함수의 주파수(ODR) 필터링 때문에 1FPS 저빈도 데이터가 삭제되는 문제를 해결.
// 필터 검열을 완전히 제거하고 수신된 이벤트를 100% 무조건 안드로이드 OS로 밀어 넣습니다.
void ambient_temperature::handle_sns_client_event(const sns_client_event_msg_sns_client_event& pb_event)
{
    ALOGI("[VISION_HAL] Event Received from ADSP! msg_id: %d", pb_event.msg_id());
    
    // 필수: 기본 클래스의 상태 머신(Config, Active 등)이 정상 작동하도록 원본 핸들러를 반드시 호출합니다.
    qsh_sensor::handle_sns_client_event(pb_event);

    if (SNS_STD_SENSOR_MSGID_SNS_STD_SENSOR_EVENT == pb_event.msg_id()) {
        sns_std_sensor_event pb_sensor_event;
        pb_sensor_event.ParseFromString(pb_event.payload());

        if (pb_sensor_event.data_size() > 0) {
            Event hal_event = create_sensor_hal_event(pb_event.timestamp());
            
            // 데이터 배열을 깨끗하게 0으로 초기화
            // 설명: hidl_array 타입에 memset을 사용하면 발생하는 빌드 에러 해결을 위해 for 루프로 초기화
            for (int i = 0; i < 16; ++i) {
                hal_event.u.data[i] = 0.0f;
            }
            // 압력(Pressure) 센서는 1축 데이터이므로 data[0] 위치에 매핑합니다.
            hal_event.u.data[0] = pb_sensor_event.data(0);
            
            // 안드로이드 기본 logcat 시스템으로 출력하여 디버깅을 명확히 함
            ALOGI("[VISION_HAL] Forcing Push to OS... Value: %f", hal_event.u.data[0]);

            // 기존의 can_submit_sample(hal_event) 검열 로직을 삭제!
            // 무조건 프레임워크 이벤트 큐에 삽입 (1FPS 저속 이벤트 드롭 완전 차단)
            events.push_back(hal_event);
        }
    }
}
// change(fix)-hyungchul-20260312-1630 끝

static std::vector<std::unique_ptr<sensor>> get_available_ambient_temperature_sensors()
{
    std::vector<std::unique_ptr<sensor>> sensors;
    const char *datatype = ambient_temperature::qsh_datatype();
    
    // 1. Try Hub 1 (ADSP) first
    int hub_id = 1;
    std::vector<suid> suids = sensor_factory::instance().get_suids(datatype, hub_id);

    // 2. If not found, try Hub 2 (SDSP/SLPI)
    if (suids.empty()) {
        hub_id = 2;
        suids = sensor_factory::instance().get_suids(datatype, hub_id);
    }

    // sns_loge("[cbc] ambient_temperature: get_available_sensors called. Found %zu SUIDs for %s on Hub %d", suids.size(), datatype, hub_id);

    for (const auto& sensor_uid : suids) {
        try {
            sensors.push_back(std::make_unique<ambient_temperature>(sensor_uid, SENSOR_WAKEUP, hub_id));
            // sns_loge("[cbc] ambient_temperature: Created sensor with SUID high=%" PRIx64 " low=%" PRIx64, sensor_uid.high, sensor_uid.low);
        } catch (const std::exception& e) {
            sns_loge("[cbc] ambient_temperature: Failed to create sensor: %s", e.what());
        }
    }

    if (sensors.empty()) {
        // Fallback: Default to Hub 1 if not found anywhere
        hub_id = 1;
        // sns_loge("[cbc] ambient_temperature: No SUIDs found! Force creating sensor with HARDCODED SUID on Hub %d.", hub_id);
        // SUID from sns_ct7117x_sensor.h:
        // Low:  0x53, 0x45, 0x4e, 0x53, 0x59, 0x4c, 0x49, 0x4e ("SENSYLIN") -> 0x4E494C59534E4553
        // High: 0x4b, 0x43, 0x54, 0x37, 0x31, 0x31, 0x37, 0x41 ("KCT7117A") -> 0x413731313754434B
        suid dummy_suid;
        dummy_suid.low = 0x4E494C59534E4553;
        dummy_suid.high = 0x413731313754434B;
        sensors.push_back(std::make_unique<ambient_temperature>(dummy_suid, SENSOR_WAKEUP, hub_id));
    }
    return sensors;
}

/*  end 2026-03-09          */

static bool ambient_temperature_module_init()
{
    /* register supported sensor types with factory */
// remove 2026-03-09 start
/*    
    sensor_factory::register_sensor(
        SENSOR_TYPE_AMBIENT_TEMPERATURE,
        qsh_sensor::get_available_sensors<ambient_temperature>);
*/        
// end 2026-03-09
// remove 2026-03-12    sensor_factory::register_sensor(SENSOR_TYPE_AMBIENT_TEMPERATURE, get_available_ambient_temperature_sensors);    // add 2026-03-09
    
    // change(fix)-hyungchul-20260312-1700 시작
    // 설명: Android Framework가 PRESSURE 센서로 activate를 요청할 때 HAL이 이를 정확히 매핑할 수 있도록
    // factory 등록 속성을 SENSOR_TYPE_AMBIENT_TEMPERATURE에서 SENSOR_TYPE_PRESSURE로 완벽히 일치시킵니다.
    sensor_factory::register_sensor(SENSOR_TYPE_PRESSURE, get_available_ambient_temperature_sensors);
    // change(fix)-hyungchul-20260312-1700 끝

    sensor_factory::request_datatype(QSH_DATATYPE_AMBIENT_TEMP);
    return true;
}

SENSOR_MODULE_INIT(ambient_temperature_module_init);
