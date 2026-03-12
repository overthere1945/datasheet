ifeq ($(TARGET_SUPPORTS_WEARABLES),true)
#Enable WearQSTA
PRODUCT_PROPERTY_OVERRIDES += ro.vendor.sensors.wearqstp=1
#WearQSTA Wakelock by default disabled
PRODUCT_PROPERTY_OVERRIDES += ro.vendor.sensors.wearqstp.lock=0
endif

#SNS_LA_VENDOR_PRODUCTS
ifeq ($(TARGET_USES_QMAA), true)
ifeq ($(TARGET_USES_QMAA_OVERRIDE_SENSORS),true)
SNS_LA_VENDOR_PRODUCTS := hals.conf
ifeq ($(PRODUCT_ENABLE_QESDK),true)
SNS_LA_VENDOR_PRODUCTS += qsap_sensors
endif
ifneq ($(TARGET_HAS_LOW_RAM), true)
SNS_LA_VENDOR_PRODUCTS += vendor.qti.hardware.sensorscalibrate-service.rc
endif
endif
else
SNS_LA_VENDOR_PRODUCTS := hals.conf
ifeq ($(PRODUCT_ENABLE_QESDK),true)
SNS_LA_VENDOR_PRODUCTS += qsap_sensors
endif
ifneq ($(TARGET_HAS_LOW_RAM), true)
SNS_LA_VENDOR_PRODUCTS += vendor.qti.hardware.sensorscalibrate-service.rc
endif
endif

SNS_LA_VENDOR_PRODUCTS += sensors.qsh
SNS_LA_VENDOR_PRODUCTS += init.vendor.sensors.rc
SNS_LA_VENDOR_PRODUCTS += android.hardware.sensors-service.multihal
SNS_LA_VENDOR_PRODUCTS += android.hardware.sensors@2.0-ScopedWakelock

ifeq ($(USE_SENSOR_GLINK_SUPPORT), true)
SNS_LA_VENDOR_PRODUCTS += init.vendor.sensors.glink.rc
endif

SNS_LA_VENDOR_PRODUCTS += vendor.qti.hardware.sensorscalibrate-service
SNS_LA_VENDOR_PRODUCTS += libsensorcal
ifeq ($(TARGET_HAS_LOW_RAM), true)
SNS_LA_VENDOR_PRODUCTS += vendor.qti.hardware.sensorscalibrate-service.disable.rc
endif

#SNS_LA_VENDOR_PRODUCTS_DEBUG
SNS_LA_VENDOR_PRODUCTS_DBG := QSensorTest
SNS_LA_VENDOR_PRODUCTS_DBG += init.qcom.sensors.sh
SNS_LA_VENDOR_PRODUCTS_DBG += UnifiedSensorTestApp
SNS_LA_VENDOR_PRODUCTS_DBG += sensors.debug

PRODUCT_PACKAGES += $(SNS_LA_VENDOR_PRODUCTS)
PRODUCT_PACKAGES_DEBUG += $(SNS_LA_VENDOR_PRODUCTS_DBG)
