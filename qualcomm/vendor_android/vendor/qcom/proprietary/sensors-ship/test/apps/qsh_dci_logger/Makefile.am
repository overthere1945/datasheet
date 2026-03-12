# Copyright (c) 2024 Qualcomm Technologies, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# Makefile.am - Automake script for qsh_dci_logger
#

diag_header_path=${WORKSPACE}/coretech-config-vendor/diag/include

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -fexceptions              \
              -I./inc                   \
              -I$(diag_header_path)

requiredlibs = -llog                    \
               -ldiag                   \
               -lcutils

cpp_sources = ./src/qsh_dci_logger.cpp  \
              ./src/main.cpp

bin_PROGRAMS =  qsh_dci_logger
qsh_dci_logger_SOURCES  = $(cpp_sources)
qsh_dci_logger_CC       = @CC@
qsh_dci_logger_CPPFLAGS = $(AM_CPPFLAGS)
if USE_GLIB
 qsh_dci_logger_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 qsh_dci_logger_LDFLAGS   = -lpthread @GLIB_LIBS@
endif
qsh_dci_logger_LDADD = $(requiredlibs)
