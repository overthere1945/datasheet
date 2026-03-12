# Makefile.am - Automake script for sscrpcd

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-result        \
              -fexceptions              \
              -I./

if ENABLE_VERSIONED_LIB
  AM_CPPFLAGS += -DSNS_VERSIONED_LIB_ENABLED
endif

cpp_sources = ./src/sscrpcd.cpp

requiredlibs = $(SENSORSUTILS_LIBS)       \
                       -lcdsprpc                  \
                       -ldl

bin_PROGRAMS = sscrpcd
sscrpcd_SOURCES = $(cpp_sources)
sscrpcd_CC = @CC@
sscrpcd_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) @PROTOBUF_CFLAGS@ @SENSORSUTILS_CFLAGS@
if USE_GLIB
 sscrpcd_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sscrpcd_LDFLAGS = @GLIB_LIBS@
endif
sscrpcd_LDADD = $(requiredlibs)

if USE_SYSTEMD
systemdunitdir = @SYSTEMD_UNITDIR@
systemdunit_DATA = sscrpcd.service
endif
