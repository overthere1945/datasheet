LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
# List of power scripts
sns_power_scripts_files := $(wildcard $(LOCAL_PATH)/*.txt)
$(info "Adding power scripts files  = $(sns_power_scripts_files)")
define _add_power_files
 include $$(CLEAR_VARS)
 LOCAL_MODULE := $(notdir $(1))
 LOCAL_MODULE_STEM := $(notdir $(1))
 sns_power_scripts_modules += $$(LOCAL_MODULE)
 LOCAL_SRC_FILES := $(1:$(LOCAL_PATH)/%=%)
 LOCAL_MODULE_TAGS := optional
 LOCAL_MODULE_CLASS := ETC
 LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR_ETC)/sensors/scripts
include $$(BUILD_PREBUILT)
endef

sns_power_scripts_modules :=
sns_config :=
$(foreach _conf-file, $(sns_power_scripts_files), $(eval $(call _add_power_files,$(_conf-file))))

include $(CLEAR_VARS)
 LOCAL_MODULE := sns_power_scripts
 LOCAL_MODULE_TAGS := optional
 LOCAL_REQUIRED_MODULES := $(sns_power_scripts_modules)
include $(BUILD_PHONY_PACKAGE)
