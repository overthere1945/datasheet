# TODO: can this be removed? 
USE_SENSOR_MULTI_HAL := true

ifeq ($(TARGET_SUPPORTS_WEARABLES), true)
USE_SENSOR_GLINK_SUPPORT := true
endif

# Below macro needs to be removed once SENSORS TP is fully enabled
# Same can controled by sensors_techpack_env.sh from techpack-sensors
BUILD_SENSORS_TECHPACK_SOURCE := true

#default values for the macros
$(call soong_config_set,qtisensors,hy00,false)
$(call soong_config_set,qtisensors,hy11,false)
$(call soong_config_set,qtisensors,hy22,false)
$(call soong_config_set,qtisensors,hwasan,false)

ifneq ($(BUILD_SENSORS_TECHPACK_SOURCE), true)
$(call soong_config_set,qtisensors,hy00,true)
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hy22,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif

ifeq (,$(wildcard $(QCPATH)/sensors-android-internal))
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif

ifeq (,$(wildcard $(QCPATH)/sensors-android))
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hy22,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif