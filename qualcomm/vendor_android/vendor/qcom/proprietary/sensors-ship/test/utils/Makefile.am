# Makefile.am - Automake script for sensor testapp utils library
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensors-test-utils.pc

EXTRA_DIST = $(pkgconfig_DATA)

AM_CPPFLAGS = -Werror                       \
              -Wno-unused-parameter         \
              -fexceptions                  \
              -I./

cpp_sources = ./util_wakelock.cpp

requiredlibs = $(SENSORSUTILS_LIBS)

library_includedir = $(pkgincludedir)

include_HEADERS = ./util_wakelock.h

lib_LTLIBRARIES = libSensorTest.la
libSensorTest_la_CC = @CC@
libSensorTest_la_SOURCES = $(cpp_sources)
libSensorTest_la_CPPFLAGS = $(AM_CPPFLAGS) @SENSORSUTILS_CFLAGS@
libSensorTest_la_LDFLAGS = -shared  -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libSensorTest_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libSensorTest_la_LDFLAGS = @GLIB_LIBS@
endif
libSensorTest_la_LIBADD = $(requiredlibs)
