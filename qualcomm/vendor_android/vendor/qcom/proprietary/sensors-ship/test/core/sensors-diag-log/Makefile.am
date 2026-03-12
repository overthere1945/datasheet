# Makefile.am - Automake script for Sensors-diag log

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-result         \
              -fexceptions              \
              -I./inc/                  \
              -DGOOGLE_PROTOBUF_NO_RTTI

cpp_sources = ./src/sensor_diag_log.cpp

requiredlibs = $(SENSORSUTILS_LIBS)                    \
               $(SENSORSAPI_LIBS)             \
               $(SENSINGHUB_LIBS)                       \
               $(DIAG_LIBS)

library_includedir = $(pkgincludedir)

include_HEADERS = ./inc/sensor_diag_log.h

lib_LTLIBRARIES = libsnsdiaglog.la
libsnsdiaglog_la_CC = @CC@
libsnsdiaglog_la_SOURCES = $(cpp_sources)
libsnsdiaglog_la_CPPFLAGS = $(AM_CPPFLAGS) @DIAG_CFLAGS@ @SENSINGHUB_CFLAGS@ @SENSORSUTILS_CFLAGS@ @SENSORSAPI_CFLAGS@
libsnsdiaglog_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
libsnsdiaglog_la_LIBADD = $(requiredlibs)
