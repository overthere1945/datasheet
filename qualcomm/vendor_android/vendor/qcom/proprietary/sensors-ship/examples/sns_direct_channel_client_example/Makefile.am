# Makefile.am - Automake script for sns_direct_channel_client_example

root_libdir  = "/lib"

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-format-extra-args    \
              -Wno-write-strings        \
              -fexceptions              \
              -DSNS_TARGET_LE_PLATFORM  \
              -I./                      \
              -I../../qsh/              \
              -I../../rpc/direct_channel_stub/inc/ \
              -I../../utils

cpp_sources = ./sns_direct_channel_client.cpp

requiredlibs = -ldl \
               ../../qsh/libsnsapi.la \
               ../../utils/libsnsutils.la \
               ../../rpc/direct_channel_stub/libsns_direct_channel_stub.la \
               @PROTOBUF_LIBS@

bin_PROGRAMS = sns_direct_channel_client_example
sns_direct_channel_client_example_SOURCES = $(cpp_sources)
sns_direct_channel_client_example_CC = @CC@
sns_direct_channel_client_example_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@
if USE_GLIB
 sns_direct_channel_client_example_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sns_direct_channel_client_example_LDFLAGS = @GLIB_LIBS@
endif
sns_direct_channel_client_example_LDADD = $(requiredlibs)