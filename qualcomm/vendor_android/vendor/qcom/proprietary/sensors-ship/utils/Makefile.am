pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensors-utils.pc

EXTRA_DIST = $(pkgconfig_DATA)

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable     \
              -Wno-unused-result        \
              -Wno-sign-compare     \
              -fexceptions              \
              -I./inc/   \
              -I./

cpp_sources_snsutils = ./src/sensors_log.cpp    \
                       ./src/wakelock_utils.cpp    \
                       ./src/sensors_timer.cpp     \
                       ./src/sensors_ssr.cpp         \
                       ./src/sensors_json_parser.cpp

include_HEADERS = ./inc/*.h

lib_LTLIBRARIES = libsnsutils.la
libsnsutils_la_CC = @CC@
libsnsutils_la_SOURCES = $(cpp_sources_snsutils)
libsnsutils_la_CPPFLAGS = $(AM_CPPFLAGS)
libsnsutils_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libsnsutils_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libsnsutils_la_LDFLAGS += @GLIB_LIBS@ -lpropertyvault
endif
libsnsutils_la_LIBADD = $(requiredlibs)

