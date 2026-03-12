# Makefile.am - Automake script for libQshSession

AM_CPPFLAGS = -Werror                   \
              -I./                      \
              -I./inc/                  \
              -Wno-return-type   \
              -fpermissive         \
              -I../qshQmiIDL/

cpp_sources =  ./src/qmiSession.cpp     \
               ./src/sessionImpl.cpp

requiredlibs = ../qshQmiIDL/libQshQmiIDL.la \
               -ldl \
               @SENSORSUTILS_LIBS@   \
               @PROTOBUF_LIBS@ \
               @QMIFRAMEWORK_LIBS@ \
               @SENSINGHUB_LIBS@        \
               @GLIB_LIBS@

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libQshSession.la
libQshSession_la_CC = @CC@
libQshSession_la_SOURCES = $(cpp_sources)
libQshSession_la_CPPFLAGS = $(AM_CPPFLAGS) @QMIFRAMEWORK_CFLAGS@ @PROTOBUF_CFLAGS@ @SENSINGHUB_CFLAGS@ @SENSORSUTILS_CFLAGS@ @SENSORSLOOKUP_CFLAGS@
libQshSession_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
if USE_GLIB
libQshSession_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
libQshSession_la_LDFLAGS += @GLIB_LIBS@
endif
libQshSession_la_LIBADD = $(requiredlibs)
