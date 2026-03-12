# Makefile.am - Automake script for sns_device_mode_example

root_libdir  = "/lib"

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -fexceptions              \
              -DSNS_TARGET_LE_PLATFORM  \
              -I./                      \
              -I../../qsh/              \
              -I../../rpc/device_mode_stub/inc/

cpp_sources = ./sns_device_mode_example.cpp

requiredlibs = -ldl \
               ../../qsh/libsnsapi.la \
               ../../rpc/device_mode_stub/libsns_device_mode_stub.la \
               @PROTOBUF_LIBS@

bin_PROGRAMS = sns_device_mode_example
sns_device_mode_example_SOURCES = $(cpp_sources)
sns_device_mode_example_CC = @CC@
sns_device_mode_example_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@
if USE_GLIB
 sns_device_mode_example_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sns_device_mode_example_LDFLAGS = @GLIB_LIBS@
endif
sns_device_mode_example_LDADD = $(requiredlibs)
