# Makefile.am - Automake script for libsns_device_mode_stub.la

AM_CPPFLAGS = -Werror               \
              -Wall                 \
              -fno-strict-aliasing  \
              -I./inc/

libsns_device_mode_stub_c_sources = src/sns_device_mode_stub.c

libsns_device_mode_stub_requiredlibs =  -lc       \
                                        -lcdsprpc

library_includedir = $(pkgincludedir)

# install library interface header into ${includedir}
include_HEADERS = inc/sns_device_mode.h

lib_LTLIBRARIES = libsns_device_mode_stub.la
libsns_device_mode_stub_la_CC = @CC@
libsns_device_mode_stub_la_SOURCES = $(libsns_device_mode_stub_c_sources)
libsns_device_mode_stub_la_CPPFLAGS = $(AM_CPPFLAGS)
libsns_device_mode_stub_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
libsns_device_mode_stub_la_LIBADD = $(libsns_device_mode_stub_requiredlibs)
