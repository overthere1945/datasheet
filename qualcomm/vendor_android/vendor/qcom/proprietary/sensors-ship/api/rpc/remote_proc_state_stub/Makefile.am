# Makefile.am - Automake script for libsns_remote_proc_state_stub.la

AM_CPPFLAGS = -Werror               \
              -Wall                 \
              -fno-strict-aliasing  \
              -I./inc/

libsns_remote_proc_state_stub_c_sources = src/sns_remote_proc_state_stub.c

libsns_remote_proc_state_stub_requiredlibs =  -lc       \
                                          -lcdsprpc

library_includedir = $(pkgincludedir)

# install library interface header into ${includedir}
include_HEADERS = inc/sns_remote_proc_state.h

lib_LTLIBRARIES = libsns_remote_proc_state_stub.la
libsns_remote_proc_state_stub_la_CC = @CC@
libsns_remote_proc_state_stub_la_SOURCES = $(libsns_remote_proc_state_stub_c_sources)
libsns_remote_proc_state_stub_la_CPPFLAGS = $(AM_CPPFLAGS)
libsns_remote_proc_state_stub_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
libsns_remote_proc_state_stub_la_LIBADD = $(libsns_remote_proc_state_stub_requiredlibs)
