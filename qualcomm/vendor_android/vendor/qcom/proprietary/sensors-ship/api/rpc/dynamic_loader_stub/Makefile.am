# Makefile.am - Automake script for libsns_dynamic_loader_stub.la

AM_CPPFLAGS = -Werror               \
              -Wall                 \
              -Wno-int-in-bool-context \
              -fno-strict-aliasing  \
              -I./inc/

libsns_dynamic_loader_stub_c_sources = src/sns_dynamic_loader_stub.c

libsns_dynamic_loader_stub_requiredlibs =  -lc       \
                                        -lcdsprpc

library_includedir = $(pkgincludedir)

# install library interface header into ${includedir}
include_HEADERS = inc/sns_dynamic_loader.h

lib_LTLIBRARIES = libsns_dynamic_loader_stub.la
libsns_dynamic_loader_stub_la_CC = @CC@
libsns_dynamic_loader_stub_la_SOURCES = $(libsns_dynamic_loader_stub_c_sources)
libsns_dynamic_loader_stub_la_CPPFLAGS = $(AM_CPPFLAGS)
libsns_dynamic_loader_stub_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
libsns_dynamic_loader_stub_la_LIBADD = $(libsns_dynamic_loader_stub_requiredlibs)
