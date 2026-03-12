pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensors-lookup.pc

EXTRA_DIST = $(pkgconfig_DATA)

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -fexceptions              \
              -I./

cpp_sources_suidlookup =  suid_lookup.cpp

include_HEADERS = ./suid_lookup.h

requiredlibs_suidlookup = $(SENSORSUTILS_LIBS)         \
                          $(SENSINGHUB_LIBS)

lib_LTLIBRARIES = libsuidlookup.la
libsuidlookup_la_CC = @CC@
libsuidlookup_la_SOURCES = $(cpp_sources_suidlookup)
libsuidlookup_la_CPPFLAGS = $(AM_CPPFLAGS)  @SENSINGHUB_CFLAGS@ @SENSORSUTILS_CFLAGS@
libsuidlookup_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsuidlookup_la_LIBADD = $(requiredlibs)