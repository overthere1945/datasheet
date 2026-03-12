# Makefile.am - Automake script for USTANative

AM_CPPFLAGS = -Werror                         \
              -Wno-unused-parameter           \
              -Wno-unused-result              \
              -fexceptions                    \
              -fno-permissive                 \
              -I./                            \
              -I../../sensors-diag-log/inc/
              
cpp_sources = ./sensor_cxt.cpp              \
              ./sensor_suid.cpp             \
              ./sensor.cpp                  \
              ./sensor_descriptor_pool.cpp  \
              ./sensor_message_parser.cpp   \
              ./sensor_attributes.cpp       \
              ./data_stream.cpp             \
              ./sensor_direct_channel.cpp

requiredlibs = ../../sensors-diag-log/libsnsdiaglog.la  \
               @SENSORSAPI_LIBS@                             \
               @SENSORSCORE_LIBS@                         \
               @PROTOBUF_LIBS@                             \
               @SENSINGHUB_LIBS@

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libUSTANative.la
libUSTANative_la_CC = @CC@
libUSTANative_la_SOURCES = $(cpp_sources)
libUSTANative_la_CPPFLAGS = $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@ @SENSINGHUB_CFLAGS@ @SENSORSCORE_CFLAGS@ @SENSORSAPI_CFLAGS@ 
libUSTANative_la_LDFLAGS = -shared  -version-number @LT_VERSION_NUMBER@
if USE_GLIB
libUSTANative_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
libUSTANative_la_LDFLAGS += @GLIB_LIBS@ -lpropertyvault
endif
libUSTANative_la_LIBADD = $(requiredlibs)
