# Makefile.am - Automake script for proto files to generate equivalent *.pb.cc, *.pb.h, snsproto.la & qsh.la

%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(srcdir) $^

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable      \
              -fexceptions              \
              -DSNS_TARGET_LE_PLATFORM  \
              -I./                      \
              -I./client_api/           \
              -I../sensors-log/         \
              -I./qsh_interface/        \
              -I../utils/

proto_list = $(shell find  $(srcdir)/ -name "*.proto")
proto_filesdir = /etc/sensors/proto
proto_files_DATA = $(proto_list)

nodist_proto_SOURCES = nanopb.pb.cc                       \
                       sns_accel_cal.pb.cc                \
                       sns_accel.pb.cc                    \
                       sns_ambient_light.pb.cc            \
                       sns_ambient_temperature.pb.cc      \
                       sns_amd.pb.cc                      \
                       sns_aont.pb.cc                     \
                       sns_cal.pb.cc                      \
                       sns_client.pb.cc                   \
                       sns_da_test.pb.cc                  \
                       sns_device_mode.pb.cc              \
                       sns_device_orient.pb.cc            \
                       sns_diag.pb.cc                     \
                       sns_diag_sensor.pb.cc              \
                       sns_direct_channel.pb.cc           \
                       sns_dpc.pb.cc                      \
                       sns_fmv.pb.cc                      \
                       sns_game_rv.pb.cc                  \
                       sns_geomag_rv.pb.cc                \
                       sns_gyro_cal.pb.cc                 \
                       sns_gyro.pb.cc                     \
                       sns_gyro_rot_matrix.pb.cc          \
                       sns_hall.pb.cc                     \
                       sns_heart_beat.pb.cc               \
                       sns_heart_rate.pb.cc               \
                       sns_humidity.pb.cc                 \
                       sns_mag_cal.pb.cc                  \
                       sns_mag.pb.cc                      \
                       sns_offbody_detect.pb.cc           \
                       sns_pedometer.pb.cc                \
                       sns_physical_sensor_test.pb.cc     \
                       sns_pose_6dof.pb.cc                \
                       sns_ppg.pb.cc                      \
                       sns_pressure.pb.cc                 \
                       sns_proximity.pb.cc                \
                       sns_registry.pb.cc                 \
                       sns_rgb.pb.cc                      \
                       sns_rotv.pb.cc                     \
                       sns_sar.pb.cc                      \
                       sns_sensor_temperature.pb.cc       \
                       sns_sig_motion.pb.cc               \
                       sns_std_event_gated_sensor.pb.cc   \
                       sns_std.pb.cc                      \
                       sns_std_sensor.pb.cc               \
                       sns_std_type.pb.cc                 \
                       sns_suid.pb.cc                     \
                       sns_thermopile.pb.cc               \
                       sns_tilt.pb.cc                     \
                       sns_tilt_to_wake.pb.cc             \
                       sns_ultra_violet.pb.cc             \
                       sns_wrist_tilt_gesture.pb.cc

BUILT_SOURCES = nanopb.pb.h                       \
                sns_accel_cal.pb.h                \
                sns_accel.pb.h                    \
                sns_ambient_light.pb.h            \
                sns_ambient_temperature.pb.h      \
                sns_amd.pb.h                      \
                sns_aont.pb.h                     \
                sns_cal.pb.h                      \
                sns_client.pb.h                   \
                sns_da_test.pb.h                  \
                sns_device_mode.pb.h              \
                sns_device_orient.pb.h            \
                sns_diag.pb.h                     \
                sns_diag_sensor.pb.h              \
                sns_direct_channel.pb.h           \
                sns_dpc.pb.h                      \
                sns_fmv.pb.h                      \
                sns_game_rv.pb.h                  \
                sns_geomag_rv.pb.h                \
                sns_gyro_cal.pb.h                 \
                sns_gyro.pb.h                     \
                sns_gyro_rot_matrix.pb.h          \
                sns_hall.pb.h                     \
                sns_heart_beat.pb.h               \
                sns_heart_rate.pb.h               \
                sns_humidity.pb.h                 \
                sns_mag_cal.pb.h                  \
                sns_mag.pb.h                      \
                sns_offbody_detect.pb.h           \
                sns_pedometer.pb.h                \
                sns_physical_sensor_test.pb.h     \
                sns_pose_6dof.pb.h                \
                sns_ppg.pb.h                      \
                sns_pressure.pb.h                 \
                sns_proximity.pb.h                \
                sns_registry.pb.h                 \
                sns_rgb.pb.h                      \
                sns_rotv.pb.h                     \
                sns_sar.pb.h                      \
                sns_sensor_temperature.pb.h       \
                sns_sig_motion.pb.h               \
                sns_std_event_gated_sensor.pb.h   \
                sns_std.pb.h                      \
                sns_std_sensor.pb.h               \
                sns_std_type.pb.h                 \
                sns_suid.pb.h                     \
                sns_thermopile.pb.h               \
                sns_tilt.pb.h                     \
                sns_tilt_to_wake.pb.h             \
                sns_ultra_violet.pb.h             \
                sns_wrist_tilt_gesture.pb.h

sources_snsapi = $(nodist_proto_SOURCES) \
                 ./client_api/sns_client_api_v01.c

include_HEADERS = $(BUILT_SOURCES)       \
                  ./client_api/sns_client_api_v01.h

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libsnsapi.la

requiredlibs_snsapi = @DIAG_LIBS@          \
                      @QMIFRAMEWORK_LIBS@  \
                      @GLIB_LIBS@          \
                      @PROTOBUF_LIBS@

libsnsapi_la_CC = @CC@
libsnsapi_la_SOURCES = $(sources_snsapi)
libsnsapi_la_CFLAGS = $(AM_CFLAGS) @DIAG_CFLAGS@ @QMIFRAMEWORK_CFLAGS@ @GLIB_CFLAGS@ @PROTOBUF_CFLAGS@
libsnsapi_la_LDFLAGS = -shared -version-info 1:0:0 @LDFLAGS@
libsnsapi_la_LIBADD = $(requiredlibs_snsapi)