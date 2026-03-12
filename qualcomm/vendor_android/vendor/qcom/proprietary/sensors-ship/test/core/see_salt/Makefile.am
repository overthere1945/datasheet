# Makefile.am - Automake script for SEESalt libraries

AM_CPPFLAGS = -Werror                       \
              -Wno-unused-parameter         \
              -fexceptions                  \
              -I./                          \
              -I../USTA/native           \
              -I../sensors-diag-log/inc

cpp_sources = ./salt_usta.cpp           \
              ./see_salt.cpp            \
              ./por.cpp                 \
              ./parse.cpp               \
              ./see_salt_payloads.cpp   \
              ./see_salt_sensor.cpp     \
              ./see_salt_sleep.cpp

requiredlibs = ../USTA/native/libUSTANative.la \
                      @SENSORSUTILS_LIBS@

include_HEADERS = ./see_salt.h \
                  ./see_salt_rc.h \
                  ./see_salt_sensor.h \
                  ./see_salt_request.h \
                  ./see_salt_payloads.h

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libSEESalt.la
libSEESalt_la_CC = @CC@
libSEESalt_la_SOURCES = $(cpp_sources)
libSEESalt_la_CPPFLAGS = $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@ @SENSORSUTILS_CFLAGS@ @SENSORSCORE_CFLAGS@ @SENSORSAPI_CFLAGS@
libSEESalt_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libSEESalt_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libSEESalt_la_LDFLAGS = @GLIB_LIBS@
endif
libSEESalt_la_LIBADD = $(requiredlibs)

