#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE([ foreign -Wnone ])
AC_CONFIG_SRCDIR([/src/usbdc/usbdc_host_handle.c])
AM_SILENT_RULES([yes])
# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB
# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday select])

AC_CONFIG_FILES([Makefile
                 build/Makefile])
AC_OUTPUT
