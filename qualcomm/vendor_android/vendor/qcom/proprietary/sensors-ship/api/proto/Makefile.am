# Makefile.am - Automake script for proto files to generate equivalent *.pb.cc, *.pb.h

%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --proto_path=@SENSINGHUB_API_PATH@ --cpp_out=$(srcdir) $^

include_HEADERS = $(srcdir)/sns_tilt_to_wake.pb.h       \
                  $(srcdir)/sns_heart_beat.pb.h         \
                  $(srcdir)/sns_wrist_tilt_gesture.pb.h \
                  $(srcdir)/sns_gyro_rot_matrix.pb.h    \
                  $(srcdir)/sns_registry.pb.h           \
                  $(srcdir)/sns_da_test.pb.h            \
                  $(srcdir)/sns_activity_recognition.pb.h \
                  $(srcdir)/sns_client_sensor.pb.h      \
                  $(srcdir)/sns_client_batch.pb.h      \
                  $(srcdir)/sns_time.pb.h      \
                  $(srcdir)/sns_step_detect.pb.h        \
                  $(srcdir)/sns_dpc.pb.h                \
                  $(srcdir)/sns_pose_6dof.pb.h          \
                  $(srcdir)/sns_ppg.pb.h                \
                  $(srcdir)/sns_heart_rate.pb.h         \
                  $(srcdir)/sns_accel_cal.pb.h          \
                  $(srcdir)/sns_thermopile.pb.h         \
                  $(srcdir)/sns_rgb.pb.h                \
                  $(srcdir)/sns_ultra_violet.pb.h       \
                  $(srcdir)/sns_offbody_detect.pb.h     \
                  $(srcdir)/sns_aont.pb.h               \
                  $(srcdir)/sns_diag.pb.h               \
                  $(srcdir)/sns_diag_sensor.pb.h        \
                  $(srcdir)/sns_fmv.pb.h               \
                  $(srcdir)/sns_ccd_gesture.pb.h        \
                  $(srcdir)/sns_delta_angle.pb.h

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable      \
              -fexceptions              \
              @SENSINGHUB_CFLAGS@       \
              -I./

proto_list = $(shell find  $(srcdir)/ -name "*.proto")
proto_filesdir = /etc/sensors/proto
proto_files_DATA = $(proto_list)

cpp_sources = $(srcdir)/sns_tilt_to_wake.pb.cc       \
              $(srcdir)/sns_heart_beat.pb.cc         \
              $(srcdir)/sns_wrist_tilt_gesture.pb.cc \
              $(srcdir)/sns_gyro_rot_matrix.pb.cc    \
              $(srcdir)/sns_activity_recognition.pb.cc   \
              $(srcdir)/sns_client_sensor.pb.cc          \
              $(srcdir)/sns_client_batch.pb.cc      \
              $(srcdir)/sns_time.pb.cc      \
              $(srcdir)/sns_step_detect.pb.cc            \
              $(srcdir)/sns_registry.pb.cc           \
              $(srcdir)/sns_da_test.pb.cc            \
              $(srcdir)/sns_dpc.pb.cc                \
              $(srcdir)/sns_pose_6dof.pb.cc          \
              $(srcdir)/sns_ppg.pb.cc                \
              $(srcdir)/sns_heart_rate.pb.cc         \
              $(srcdir)/sns_accel_cal.pb.cc          \
              $(srcdir)/sns_thermopile.pb.cc         \
              $(srcdir)/sns_rgb.pb.cc                \
              $(srcdir)/sns_ultra_violet.pb.cc       \
              $(srcdir)/sns_offbody_detect.pb.cc     \
              $(srcdir)/sns_aont.pb.cc               \
              $(srcdir)/sns_diag.pb.cc               \
              $(srcdir)/sns_diag_sensor.pb.cc        \
              $(srcdir)/sns_fmv.pb.cc               \
              $(srcdir)/sns_ccd_gesture.pb.cc        \
              $(srcdir)/sns_delta_angle.pb.cc

requiredlibs_sensinghubapiprop = $(SENSINGHUB_LIBS) \
                                 $(PROTOBUF_LIBS)

lib_LTLIBRARIES = libsensinghubapiprop.la
libsensinghubapiprop_la_CC = @CC@
libsensinghubapiprop_la_SOURCES = $(cpp_sources)
libsensinghubapiprop_la_CPPFLAGS = $(AM_CPPFLAGS)
libsensinghubapiprop_la_CFLAGS = $(AM_CFLAGS) @PROTOBUF_CFLAGS@ @SENSINGHUB_CFLAGS@
libsensinghubapiprop_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsensinghubapiprop_la_LIBADD = $(requiredlibs_sensinghubapiprop)
