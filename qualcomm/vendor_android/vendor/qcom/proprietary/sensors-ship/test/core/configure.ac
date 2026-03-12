#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for sensors
#
# Process this file with autoconf to produce a configure script
# Requires autoconf tool later than 2.61
AC_PREREQ(2.61)

# Initialize the sensors package version 1.0.0
AC_INIT([sensors-test-core], 1.0.0)

# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Disables auto rebuilding of configure, Makefile.ins
AM_MAINTAINER_MODE

# defines some macros variable to be included by source
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

# Checks for libraries
PKG_CHECK_MODULES([SENSINGHUB], [sensing-hub])
AC_SUBST([SENSINGHUB_CFLAGS])
AC_SUBST([SENSINGHUB_LIBS])

PKG_CHECK_MODULES([SENSORSUTILS], [sensors-utils])
AC_SUBST([SENSORSUTILS_CFLAGS])
AC_SUBST([SENSORSUTILS_LIBS])

PKG_CHECK_MODULES([SENSORSCORE], [sensors-core])
AC_SUBST([SENSORSCORE_CFLAGS])
AC_SUBST([SENSORSCORE_LIBS])

PKG_CHECK_MODULES([PROTOBUF], [protobuf])
AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])
AC_SUBST(PROTOBUF_VERSION)

AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])
AC_SUBST(PROTOBUF_VERSION)

PKG_CHECK_MODULES([DIAG], [diag])
AC_SUBST([DIAG_CFLAGS])
AC_SUBST([DIAG_LIBS])

PKG_CHECK_MODULES([SENSORSAPI], [sensors-api])
AC_SUBST([SENSORSAPI_CFLAGS])
AC_SUBST([SENSORSAPI_LIBS])

AC_ARG_WITH(sanitized-headers,
        AS_HELP_STRING([--with-sanitized-headers=DIR],
                [Specify the location of the sanitized Linux headers]),
        [CPPFLAGS="$CPPFLAGS -idirafter $withval"])

AC_ARG_WITH([common_includes],
      AS_HELP_STRING([--with-common-includes=@<:@dir@:>@],
         [Specify the location of the common headers]),
            [common_incdir=$withval],
               with_common_includes=no)

if test "x$with_common_includes" != "xno"; then
   CFLAGS="${CFLAGS} -I${common_incdir}"
fi

AC_ARG_WITH([glib],
       AS_HELP_STRING([--with-glib],
          [enable glib, building HLOS systems which use glib]))

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes, AC_MSG_ERROR(GLib >= 2.16 is required))
GLIB_CFLAGS="$GLIB_CFLAGS"
GLIB_LIBS="$GLIB_LIBS"
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")

m4_define([LT_MAJOR], [1])
m4_define([LT_MINOR], [0])
m4_define([LT_PATCH], [0])

AC_SUBST([LT_VERSION], LT_MAJOR.LT_MINOR.LT_PATCH)
AC_SUBST([LT_VERSION_NUMBER], LT_MAJOR:LT_MINOR:LT_PATCH)

AC_CONFIG_FILES([             \
        Makefile              \
        sensors-diag-log/Makefile    \
        USTA/native/Makefile    \
        see_salt/Makefile            \
        sensors-test-core.pc
               ])
AC_OUTPUT

