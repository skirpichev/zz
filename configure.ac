# Process this file with autoconf to produce a configure script.

define(ZZ_COPYRIGHT,[[
Copyright (C) 2024-2026 Sergey B Kirpichev

This file is part of the ZZ Library.

The ZZ Library is free software: you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (LGPL) as published by the
Free Software Foundation; either version 3 of the License, or (at your option)
any later version.  See <https://www.gnu.org/licenses/>.

]])
define(ZZ_COPYRIGHT_C,[[/*
    Copyright (C) 2024-2026 Sergey B Kirpichev

    This file is part of the ZZ Library.

    The ZZ Library is free software: you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License (LGPL) as
    published by the Free Software Foundation; either version 3 of the License,
    or (at your option) any later version.  See
    <https://www.gnu.org/licenses/>.
*/]])

AC_COPYRIGHT(ZZ_COPYRIGHT)
AH_TOP(ZZ_COPYRIGHT_C)

AC_PREREQ(2.62)

AC_INIT([zz],[0.9.0a3],[skirpichev@gmail.com])
#AC_CONFIG_SRCDIR(zz.h)
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([gnu no-dependencies])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_PROG_MAKE_SET

# Check for which system.
AC_CANONICAL_HOST

# Disable unnecessary libtool tests...
define([AC_LIBTOOL_LANG_CXX_CONFIG],[:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG],[:])dnl
define([AC_LIBTOOL_LANG_GCJ_CONFIG],[:])dnl

AX_VALGRIND_DFLT([sgcheck], [off])
AX_VALGRIND_DFLT([drd], [off])
AX_VALGRIND_DFLT([helgrind], [off])
AX_VALGRIND_CHECK()

# Checks for programs.
AC_PROG_CC
AC_LANG(C)

# Check for C17 support and add the flag if available, otherwise fail
AX_CHECK_COMPILE_FLAG([-std=c11], [CFLAGS="$CFLAGS -std=c17"],
  [AC_MSG_ERROR([A C17 compliant compiler is required])], [-Wall -Wextra])

LT_INIT

AC_ARG_WITH([gmp],
            [AS_HELP_STRING([--with-gmp=DIR],
                           [GMP install directory])],
            [
            CPPFLAGS="-I$withval/include $CPPFLAGS"
            LDFLAGS="-L$withval/lib $LDFLAGS"
            ])

# Checks for libraries...
AC_SEARCH_LIBS([nextafter], [m])
AC_CHECK_LIB(gmp,__gmpz_init,,AC_MSG_ERROR([GNU GMP library is required.]))

# We need mpn_sizeinbase()
AC_MSG_CHECKING(for recent GMP)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include "gmp.h"
#if (__GNU_MP_VERSION*100+__GNU_MP_VERSION_MINOR*10 < 600)
# error "GMP 6.0.0 or newer is required"
error
#endif
  ]])],[AC_MSG_RESULT(yes)],[
   AC_MSG_RESULT(no)
   AC_MSG_ERROR([GMP 6.0.0 or newer is required])
])

AC_CHECK_LIB([pthread], [pthread_create], [HAVE_PTHREAD=yes], [HAVE_PTHREAD=no])
AS_IF([test "x$HAVE_PTHREAD" = xyes],
      [AC_DEFINE_UNQUOTED([HAVE_PTHREAD_H], [1], [Define to 1 if you have the <pthread.h> header file.])
       LIBS="$LIBS -lpthread"],
      [AC_DEFINE_UNQUOTED([HAVE_PTHREAD_H], [0], [Define to 0 if you do not have the <pthread.h> header file.])])

# Check for working setrlimit(RLIMIT_AS)
AC_CACHE_CHECK([setrlimit() can set RLIMIT_AS], [ac_cv_can_set_rlimit_as],
[AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/resource.h>
int
main(void)
{
    struct rlimit new, old;

    if (getrlimit(RLIMIT_AS, &old)) {
        return 1;
    }
    new.rlim_max = old.rlim_max;
    new.rlim_cur = 64*1000*1000;
    if (setrlimit(RLIMIT_AS, &new)) {
        return 1;
    }
    return 0;
}]])], [ac_cv_can_set_rlimit_as=yes],
[ac_cv_can_set_rlimit_as=no],
[ac_cv_can_set_rlimit_as=no])])
if test "$ac_cv_can_set_rlimit_as" = "yes"; then
    AC_DEFINE([CAN_SET_RLIMIT_AS], [1],
              [Defined if setrlimit() can set RLIMIT_AS.])
fi

# Checks for header files.
AC_CHECK_HEADERS([assert.h ctype.h float.h inttypes.h math.h setjmp.h
                  stdlib.h string.h stdbool.h stddef.h stdint.h])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_HEADERS([valgrind/valgrind.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

AC_ARG_ENABLE(gcov,[AS_HELP_STRING([--enable-gcov], [enable coverage test])])
AM_CONDITIONAL([ENABLE_GCOV],[test "x${enable_gcov}" = "xyes"])

# Configs for Windows DLLs
AC_SUBST(ZZ_LDFLAGS)
#
# Additional checks on windows
# libtool requires "-no-undefined" for win32 dll
# It also disables the tests involving the linking with LIBGMP if DLL
#
AC_DEFUN([ZZ_WINDOWS], [
   if test "$enable_shared" = yes; then
     ZZ_LDFLAGS="$ZZ_LDFLAGS -no-undefined"
     AC_MSG_CHECKING(for DLL/static gmp)
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include "gmp.h"
#if !__GMP_LIBGMP_DLL
#error
error
#endif
     ]], [[]])],[AC_MSG_RESULT(DLL)],[
  AC_MSG_RESULT(static)
  AC_MSG_ERROR([gmp is not available as a DLL: use --enable-static --disable-shared]) ])
   else
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include "gmp.h"
#if __GMP_LIBGMP_DLL
#error
error
#endif
     ]], [[]])],[AC_MSG_RESULT(static)],[
  AC_MSG_RESULT(DLL)
  AC_MSG_ERROR([gmp is only available as a DLL: use --disable-static --enable-shared]) ])
  fi
  ;;
])
case $host in
  *-*-cygwin* | *-*-mingw* | *-*-pw32* | *-*-os2*)
     ZZ_WINDOWS
esac

AC_CONFIG_FILES([makefile doc/makefile tests/makefile zz.pc])
AC_OUTPUT
