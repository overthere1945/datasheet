# Makefile.am - Automake script for ssc_sensor_info

AM_CPPFLAGS = -Werror                       \
              -Wall                         \
              -Wno-unused-parameter         \
              -Wno-unused-variable          \
              -Wunused-but-set-variable     \
              -Wno-error=array-bounds       \
              -Werror=sign-compare          \
              -fexceptions                  \
              -I./                          \
              @SENSORSTESTUTILS_CFLAGS@                         \
              @SENSORSCORE_CFLAGS@   \
              @SENSORSTESTCORE_CFLAGS@   \
              @SENSORSUTILS_CFLAGS@   \
              @SENSORSAPI_CFLAGS@

requiredlibs = $(SENSORSUTILS_LIBS)              \
               $(SENSORSTESTCORE_LIBS)              \
               $(SENSORSLOOKUP_LIBS)              \
               $(SENSORSCORE_LIBS)              \
               -ldl                                     \
               $(PROTOBUF_LIBS)            \
               $(SENSINGHUB_LIBS)

c_sources = ./ssc_sensor_info.cpp

bin_PROGRAMS =  ssc_sensor_info
ssc_sensor_info_SOURCES = $(c_sources)
ssc_sensor_info_CC = @CC@
ssc_sensor_info_CPPFLAGS = $(AM_CPPFLAGS)
if USE_GLIB
 ssc_sensor_info_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@ @SENSINGHUB_CFLAGS@
 ssc_sensor_info_LDFLAGS = @GLIB_LIBS@
endif
ssc_sensor_info_LDADD = $(requiredlibs)
