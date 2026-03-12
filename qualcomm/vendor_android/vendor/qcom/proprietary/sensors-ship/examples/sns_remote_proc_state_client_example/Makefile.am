# Makefile.am - Automake script for sns_remote_proc_state_client_example

root_libdir  = "/lib"

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-write-strings        \
              -fexceptions              \
              -DSNS_TARGET_LE_PLATFORM  \
              -I./                      \
              -I../../qsh/              \
              -I../../rpc/remote_proc_state_stub/inc/ \
              -I../../utils

cpp_sources = ./sns_remote_proc_state_client_example.cpp

requiredlibs = -ldl \
               ../../qsh/libsnsapi.la \
               ../../rpc/remote_proc_state_stub/libsns_remote_proc_state_stub.la \
               @PROTOBUF_LIBS@

bin_PROGRAMS = sns_remote_proc_state_client_example
sns_remote_proc_state_client_example_SOURCES = $(cpp_sources)
sns_remote_proc_state_client_example_CC = @CC@
sns_remote_proc_state_client_example_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@
if USE_GLIB
 sns_remote_proc_state_client_example_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sns_remote_proc_state_client_example_LDFLAGS = @GLIB_LIBS@
endif
sns_remote_proc_state_client_example_LDADD = $(requiredlibs)