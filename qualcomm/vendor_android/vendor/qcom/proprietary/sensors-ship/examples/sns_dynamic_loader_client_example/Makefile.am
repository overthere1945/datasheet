# Makefile.am - Automake script for sns_dynamic_loader_client_example

root_libdir  = "/lib"

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -fexceptions              \
              -Wno-write-strings        \
              -DSNS_TARGET_LE_PLATFORM  \
              -I./                      \
              -I../../qsh/              \
              -I../../rpc/dynamic_loader_stub/inc/

cpp_sources = ./sns_dynamic_loader_client_example.cpp

requiredlibs = -ldl \
               ../../qsh/libsnsapi.la \
               ../../rpc/dynamic_loader_stub/libsns_dynamic_loader_stub.la \
               @PROTOBUF_LIBS@

bin_PROGRAMS = sns_dynamic_loader_client_example
sns_dynamic_loader_client_example_SOURCES = $(cpp_sources)
sns_dynamic_loader_client_example_CC = @CC@
sns_dynamic_loader_client_example_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@
if USE_GLIB
 sns_dynamic_loader_client_example_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sns_dynamic_loader_client_example_LDFLAGS = @GLIB_LIBS@
endif
sns_dynamic_loader_client_example_LDADD = $(requiredlibs)