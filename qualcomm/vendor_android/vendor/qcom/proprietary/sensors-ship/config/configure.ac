#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for sensors
#
# Process this file with autoconf to produce a configure script
# Requires autoconf tool later than 2.61
AC_PREREQ(2.61)

# Initialize the sensors package version 1.0.0
AC_INIT([sensors-registry], 1.0.0)

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

AC_ARG_ENABLE(target-qcm6490,
         AS_HELP_STRING([--enable-target-qcm6490], [enable for qcm6490 target]),
        [QCM6490="yes"],
         [QCM6490="no"])
AM_CONDITIONAL([QCM6490_TARGET], [test "x${QCM6490}" != "xno"])

AC_CONFIG_FILES([             \
        Makefile              \
        sensors-registry.pc
               ])
AC_OUTPUT

