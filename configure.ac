AC_PREREQ([2.69])

AC_INIT([libkamoflage], [1.0], [pltxtra@vuknob.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AM_PROG_AR
LT_INIT

AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([gnuVG], [gnuVG])
PKG_CHECK_MODULES([libsvgandroid], [libsvgandroid])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 libkamoflage.pc
 Makefile
 src/Makefile
])

AC_OUTPUT
