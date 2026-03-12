pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensors-registry.pc

EXTRA_DIST = $(pkgconfig_DATA)

ACLOCAL_AMFLAGS = -I m4

version_dir = "/etc/sensors/registry"

reg_config_dir = "/etc/sensors"

common_list = $(shell find  $(srcdir)/registry/hub1/sensors/common/*.json -type f)
common_jsondir   = $(reg_config_dir)/config
common_json_DATA = $(common_list)

if QCM6490_TARGET
qcm6490_list = $(shell find  $(srcdir)/registry/hub1/sensors/qcm6490/*.json -type f)
qcm6490_jsondir = $(reg_config_dir)/config
qcm6490_json_DATA = $(qcm6490_list)

jsonlst_filesdir = $(reg_config_dir)/config
jsonlst_files_DATA = $(srcdir)/registry/hub1/sensors/qcm6490/json.lst

regconfig_filesdir = $(reg_config_dir)
regconfig_files_DATA = $(srcdir)/registry/hub1/sensors/qcm6490/sns_reg_config

endif

sns_reg_verdir = $(version_dir)
sns_reg_ver_DATA = $(srcdir)/registry/hub1/sns_reg_version
