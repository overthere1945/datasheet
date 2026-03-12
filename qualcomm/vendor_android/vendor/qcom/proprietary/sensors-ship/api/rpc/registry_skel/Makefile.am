# Makefile.am - Automake script for sns_registry_skel

AM_CPPFLAGS = -Werror               \
              -Wall                 \
              -fexceptions          \
              -fno-strict-aliasing  \
              -I./inc

c_sources = src/sns_registry_imp.c \
            src/sns_registry_skel.c

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libsns_registry_skel.la
libsns_registry_skel_la_CC = @CC@
libsns_registry_skel_la_SOURCES = $(c_sources)
libsns_registry_skel_la_CPPFLAGS = $(AM_CPPFLAGS)
libsns_registry_skel_la_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libsns_registry_skel_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libsns_registry_skel_la_LDFLAGS += @GLIB_LIBS@
endif
libsns_registry_skel_la_LIBADD = $(requiredlibs)
