# Makefile.am - Automake script for ssc_drva_test

AM_CPPFLAGS = -Werror                       \
              -Wall                         \
              -Wno-unused-parameter         \
              -Wno-unused-variable          \
              -Wno-unused-result            \
              -fexceptions                  \
              -I./                          \
              @SENSORSTESTUTILS_CFLAGS@                         \
              @SENSORSCORE_CFLAGS@   \
              @SENSORSTESTCORE_CFLAGS@   \
              @SENSORSUTILS_CFLAGS@   \
              @SENSINGHUB_CFLAGS@        \
              @SENSORSAPI_CFLAGS@

cpp_sources = ./ssc_drva_test.cpp

requiredlibs = $(SENSORSUTILS_LIBS)              \
               $(SENSORSTESTCORE_LIBS)              \
                       $(SENSORSLOOKUP_LIBS)              \
                       $(SENSORSCORE_LIBS)              \
                       $(SENSORSTESTUTILS_LIBS)              \
                       $(SENSINGHUB_LIBS)               \
                       $(SENSORSAPI_LIBS)                \
               -ldl                                     \
               $(PROTOBUF_LIBS)

bin_PROGRAMS =  ssc_drva_test
ssc_drva_test_SOURCES = $(cpp_sources)
ssc_drva_test_CC = @CC@
ssc_drva_test_CPPFLAGS = $(AM_CPPFLAGS)
ssc_drva_test_LDFLAGS = -shared -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 ssc_drva_test_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 ssc_drva_test_LDFLAGS += @GLIB_LIBS@
endif
ssc_drva_test_LDADD = $(requiredlibs)
