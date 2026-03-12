ifeq ($(TARGET_BOARD_PLATFORM),sun)
# Sensor conf files
SNS_PRODUCT_SKU_SUN := sku_sun
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.sensor.ambient_temperature.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.ambient_temperature.xml \
    frameworks/native/data/etc/android.hardware.sensor.relative_humidity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.relative_humidity.xml \
    frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SUN)/android.hardware.sensor.hifi_sensors.xml

PRODUCT_PROPERTY_OVERRIDES += sensor.barometer.high_quality.implemented=true
endif

ifeq ($(TARGET_BOARD_PLATFORM),canoe)
# Sensor conf files
SNS_PRODUCT_SKU_CANOE := sku_canoe
SNS_PRODUCT_SKU_ALOR := sku_alor
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.sensor.ambient_temperature.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.ambient_temperature.xml \
    frameworks/native/data/etc/android.hardware.sensor.relative_humidity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.relative_humidity.xml \
    frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_CANOE)/android.hardware.sensor.hifi_sensors.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.sensor.ambient_temperature.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.ambient_temperature.xml \
    frameworks/native/data/etc/android.hardware.sensor.relative_humidity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.relative_humidity.xml \
    frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_ALOR)/android.hardware.sensor.hifi_sensors.xml

PRODUCT_PROPERTY_OVERRIDES += sensor.barometer.high_quality.implemented=true
endif

ifeq ($(TARGET_BOARD_PLATFORM),seraph)
# Sensor conf files
SNS_PRODUCT_SKU_SERAPH := sku_seraph
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/$(SNS_PRODUCT_SKU_SERAPH)/android.hardware.sensor.stepdetector.xml
endif

ifeq ($(TARGET_BOARD_PLATFORM), vienna)
# Sensor conf files
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepdetector.xml

ifeq ($(TARGET_SUPPORTS_WEAR_OS), true)
PRODUCT_COPY_FILES += device/google/clockwork/data/etc/com.google.clockwork.hardware.sensor.llob.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.google.clockwork.hardware.sensor.llob.xml
endif
endif

PRODUCT_PROPERTY_OVERRIDES += persist.vendor.sensors.enable.mag_filter=true

#SNS_VENDOR_PRODUCTS
ifeq ($(TARGET_USES_QMAA), true)

ifeq ($(TARGET_USES_QMAA_OVERRIDE_SENSORS),true)
SNS_VENDOR_PRODUCTS := sscrpcd
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub1_reg_config
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub1_config_module
SNS_VENDOR_PRODUCTS += sensors_hub1_registry
SNS_VENDOR_PRODUCTS += sns_hub1_reg_version

ifeq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub2_reg_config
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub2_config_module
SNS_VENDOR_PRODUCTS += sns_hub2_reg_version
SNS_VENDOR_PRODUCTS += sensors_hub2_registry
endif
ifeq ($(PRODUCT_ENABLE_QESDK),true)
SNS_VENDOR_PRODUCTS += qsap_sensors
SNS_VENDOR_PRODUCTS += libqesdk_qshsession
SNS_VENDOR_PRODUCTS += odpmanagerlib-aar
endif

ifneq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_odp_version
endif

ifneq ($(TARGET_BOARD_PLATFORM),seraph)
ifneq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_oispd_reg_config
SNS_VENDOR_PRODUCTS += sns_oispd_config_module
endif
endif

endif

else

SNS_VENDOR_PRODUCTS := sscrpcd
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub1_reg_config
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub1_config_module
SNS_VENDOR_PRODUCTS += sensors_hub1_registry
SNS_VENDOR_PRODUCTS += sns_hub1_reg_version

ifeq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub2_reg_config
SNS_VENDOR_PRODUCTS += sns_sensorspd_hub2_config_module
SNS_VENDOR_PRODUCTS += sns_hub2_reg_version
SNS_VENDOR_PRODUCTS += sensors_hub2_registry
endif

ifeq ($(PRODUCT_ENABLE_QESDK),true)
SNS_VENDOR_PRODUCTS += qsap_sensors
SNS_VENDOR_PRODUCTS += libqesdk_qshsession
SNS_VENDOR_PRODUCTS += odpmanagerlib-aar
endif

ifneq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_odp_version
endif

ifneq ($(TARGET_BOARD_PLATFORM),seraph)
ifneq ($(TARGET_BOARD_PLATFORM),vienna)
SNS_VENDOR_PRODUCTS += sns_oispd_reg_config
SNS_VENDOR_PRODUCTS += sns_oispd_config_module
endif
endif

endif

# Implementation for ISession
SNS_VENDOR_PRODUCTS += libQshSession
SNS_VENDOR_PRODUCTS += libQshQmiIDL

SNS_VENDOR_PRODUCTS += libsns_registry_skel
SNS_VENDOR_PRODUCTS += libsnsdiaglog
SNS_VENDOR_PRODUCTS += libqsh
SNS_VENDOR_PRODUCTS += libsnsapi
SNS_VENDOR_PRODUCTS += libsnsutils
SNS_VENDOR_PRODUCTS += libsns_dynamic_loader_stub
SNS_VENDOR_PRODUCTS += libsensinghubapi-prop
SNS_VENDOR_PRODUCTS += libsuidlookup

ifneq ($(TARGET_SUPPORTS_WEARABLES),true)
SNS_VENDOR_PRODUCTS += libsns_device_mode_stub
endif
SNS_VENDOR_PRODUCTS += libsns_direct_channel_stub
SNS_VENDOR_PRODUCTS += libsns_remote_proc_state_stub

#SNS_VENDOR_PRODUCTS_DEBUG
SNS_VENDOR_PRODUCTS_DBG := see_selftest
SNS_VENDOR_PRODUCTS_DBG += see_workhorse
SNS_VENDOR_PRODUCTS_DBG += ssc_drva_test
SNS_VENDOR_PRODUCTS_DBG += ssc_sensor_info
SNS_VENDOR_PRODUCTS_DBG += libSEESalt
SNS_VENDOR_PRODUCTS_DBG += libUSTANative
SNS_VENDOR_PRODUCTS_DBG += qsh_dci_logger

SNS_VENDOR_PRODUCTS_DBG += sns_power_scripts

SNS_VENDOR_PRODUCTS_DBG += sns_accel_cal.proto
SNS_VENDOR_PRODUCTS_DBG += sns_activity_recognition.proto
SNS_VENDOR_PRODUCTS_DBG += sns_aont.proto
SNS_VENDOR_PRODUCTS_DBG += sns_ccd_gesture.proto
SNS_VENDOR_PRODUCTS_DBG += sns_client_batch.proto
SNS_VENDOR_PRODUCTS_DBG += sns_client_glink.proto
SNS_VENDOR_PRODUCTS_DBG += sns_client_sensor.proto
SNS_VENDOR_PRODUCTS_DBG += sns_da_test.proto
SNS_VENDOR_PRODUCTS_DBG += sns_delta_angle.proto
SNS_VENDOR_PRODUCTS_DBG += sns_diag.proto
SNS_VENDOR_PRODUCTS_DBG += sns_diag_sensor.proto
SNS_VENDOR_PRODUCTS_DBG += sns_dpc.proto
SNS_VENDOR_PRODUCTS_DBG += sns_fmv.proto
SNS_VENDOR_PRODUCTS_DBG += sns_gyro_rot_matrix.proto
SNS_VENDOR_PRODUCTS_DBG += sns_heart_beat.proto
SNS_VENDOR_PRODUCTS_DBG += sns_heart_rate.proto
SNS_VENDOR_PRODUCTS_DBG += sns_offbody_detect.proto
SNS_VENDOR_PRODUCTS_DBG += sns_pose_6dof.proto
SNS_VENDOR_PRODUCTS_DBG += sns_ppg.proto
SNS_VENDOR_PRODUCTS_DBG += sns_registry.proto
SNS_VENDOR_PRODUCTS_DBG += sns_rgb.proto
SNS_VENDOR_PRODUCTS_DBG += sns_step_detect.proto
SNS_VENDOR_PRODUCTS_DBG += sns_thermopile.proto
SNS_VENDOR_PRODUCTS_DBG += sns_tilt_to_wake.proto
SNS_VENDOR_PRODUCTS_DBG += sns_time.proto
SNS_VENDOR_PRODUCTS_DBG += sns_ultra_violet.proto
SNS_VENDOR_PRODUCTS_DBG += sns_wrist_tilt_gesture.proto
SNS_VENDOR_PRODUCTS_DBG += sns_electro_neuro_graph.proto
SNS_VENDOR_PRODUCTS_DBG += sns_skin_temperature.proto

PRODUCT_PACKAGES += $(SNS_VENDOR_PRODUCTS)
PRODUCT_PACKAGES_DEBUG += $(SNS_VENDOR_PRODUCTS_DBG)
