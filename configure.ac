#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(sblim-sfcc, 2.2.6, sblim-devel@lists.sourceforge.net)
AC_CONFIG_SRCDIR([cimc/cimcclient.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE

AC_ARG_ENABLE(http-chunking,
	[AC_HELP_STRING([--disable-http-chunking],
		[disable http chunking support. May be necessary for older CURL versions.])],
	[enable_http_chunking=$enableval],
	[enable_http_chunking="yes"]
	)
		
# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h stddef.h stdlib.h string.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for libraries
AC_CHECK_LIB(curl,curl_easy_init,[LIBCURL=-lcurl],[AC_MSG_ERROR([Could not find required libcurl])])
AC_CHECK_LIB(pthread,main)

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([gettimeofday localtime_r memset strcasecmp strdup strncasecmp strstr tzset])

#SFCB Library location
# Set with "configure SFCB_LIBDIR=<dir>
AC_SUBST(SFCB_LIBDIR)
if test -z "$SFCB_LIBDIR"
then
    SFCB_LIBDIR="$libdir/sfcb"
fi

# MELUNIT
AM_CONDITIONAL(HAVE_MELUNIT_CXX,[test 0 = 1 ])

if test $enable_http_chunking != yes 
then
   AC_DEFINE(HAVE_HTTP_CHUNKING,1,[Indication support enabled.])
fi

AC_CONFIG_FILES([Makefile sfcc.spec TEST/Makefile ])

#### Platform Specific Configuration ####
AC_CANONICAL_HOST
case $host_os in
  darwin* )
    HOST_LDFLAGS="-Wl"
 
    ;;
  *)
    HOST_LDFLAGS="-Wl,--version-script"
    ;;
esac

AC_SUBST(HOST_LDFLAGS)

AC_OUTPUT
