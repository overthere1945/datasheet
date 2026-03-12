# Makefile.am - Automake script for see_workhorse

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable      \
              -Wno-sign-compare         \
              -fexceptions              \
              -Wno-unused-function      \
              -I./

requiredlibs = $(SENSORSTESTCORE_LIBS)      \
               $(SENSORSTESTUTILS_LIBS)

cpp_sources = ./see_workhorse.cpp       \
              ./display_active.cpp

bin_PROGRAMS =  see_workhorse
see_workhorse_SOURCES = $(cpp_sources)
see_workhorse_CC = @CC@
see_workhorse_CPPFLAGS = $(AM_CPPFLAGS) @SENSORSTESTCORE_CFLAGS@ @SENSORSTESTUTILS_CFLAGS@
if USE_GLIB
 see_workhorse_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 see_workhorse_LDFLAGS = @GLIB_LIBS@
endif
see_workhorse_LDADD = $(requiredlibs)
