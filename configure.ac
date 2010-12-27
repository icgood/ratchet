#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([ratchet], [0.2], [Ian Good <icgood@gmail.com>])
AM_INIT_AUTOMAKE([color-tests silent-rules])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([ratchet/ratchet.c])
AC_CONFIG_HEADERS([config.h])

#####################
# Checks for programs.
AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

#####################
# Checks for libraries.

# Lua
AX_LUA_LIBS
if test "x${LUA_LIB}" == "x"; then
	AC_MSG_ERROR([Lua libraries required for building.])
fi

# Libevent
AC_DEFINE([HAVE_LIBEVENT], [0], [Define to 1 if you have the libevent library.])
AC_CHECK_HEADERS([event.h], [have_libevent=yes], [have_libevent=no])
AC_SEARCH_LIBS([event_set], [event], [], [have_libevent=no])
if test "x${have_libevent}" != "xyes"; then
	AC_MSG_NOTICE([Libevent libraries required for building.])
fi
AM_CONDITIONAL([HAVE_LIBEVENT], [test "x${have_libevent}" = "xyes"])

# epoll
AC_DEFINE([HAVE_EPOLL], [0], [Define to 1 if you have the Linux epoll(7) interface.])
AC_DEFINE([HAVE_EPOLL_PWAIT], [0], [Define to 1 if you have the epoll_pwait function.])
AX_HAVE_EPOLL([AC_DEFINE([HAVE_EPOLL], [1])], [])
AX_HAVE_EPOLL_PWAIT([AC_DEFINE([HAVE_EPOLL_PWAIT], [1])], [])
if test "x${ax_cv_have_epoll}" != "xyes"; then
	AC_MSG_NOTICE([Epoll will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_EPOLL], [test "x${ax_cv_have_epoll}" = "xyes"])

# ZeroMQ 2.1.0+
AC_DEFINE([HAVE_ZMQ], [0], [Define to 1 if you have the ZeroMQ library.])
AC_CHECK_HEADERS([zmq.h], [have_zmq=yes], [have_zmq=no])
AC_SEARCH_LIBS([zmq_init], [zmq], [], [have_zmq=no])
AC_MSG_CHECKING([for ZeroMQ version 2.1.0 or higher])
AC_COMPUTE_INT([have_zmq_version], [ZMQ_VERSION], [@%:@include <zmq.h>], [have_zmq_version=0])
AS_IF([test $have_zmq_version -ge 20100], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no]); have_zmq=no])
if test "x${have_zmq}" != "xyes"; then
	AC_MSG_NOTICE([ZeroMQ will not be included in the ratchet library.])
else
	AC_DEFINE([HAVE_ZMQ], [1])
fi
AM_CONDITIONAL([HAVE_ZMQ], [test "x${have_zmq}" = "xyes"])

# timerfd
AC_DEFINE([HAVE_TIMERFD], [0], [Define to 1 if you have the timerfd function suite.])
AC_CHECK_HEADERS([sys/timerfd.h], [have_timerfd=yes], [have_timerfd=no])
AC_CHECK_FUNC([timerfd_create], [], [have_timerfd=no])
if test "x${have_timerfd}" != "xyes"; then
	AC_MSG_NOTICE([The timer module will not be included in the ratchet library.])
else
	AC_DEFINE([HAVE_TIMERFD], [1])
fi
AM_CONDITIONAL([HAVE_TIMERFD], [test "x${have_timerfd}" = "xyes"])

# clock_gettime()
AC_DEFINE([HAVE_CLOCK_GETTIME], [0], [Define to 1 if you have the clock_gettime system function.])
AC_SEARCH_LIBS([clock_gettime], [rt], [
		AC_DEFINE([HAVE_CLOCK_GETTIME], [1])], [
		AC_MSG_NOTICE([get_now() method will not be available in timer module.])])

# getaddrinfo_a()
AC_SEARCH_LIBS([getaddrinfo_a], [anl], [], [AC_MSG_ERROR([getaddrinfo_a() is not available for asynchronous DNS resolution.])])

#####################
# Configure options: BUFSIZ=nnn
AC_ARG_VAR([BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
if test -n "$BUFSIZ"; then
	AC_DEFINE_UNQUOTED([BUFSIZ], [$BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
fi

#####################
# Configure options: --enable-devel[=no].
AC_ARG_ENABLE([devel], [AS_HELP_STRING([--enable-devel],
	                               [install developer headers and libraries (default no)])],
	      [enable_devel=yes], [enable_devel=no])
AM_CONDITIONAL([ENABLE_DEVEL], [test "x${enable_devel}" != "xno"])

#####################
# Configure options: --disable-module[=yes].
AC_ARG_ENABLE([module], [AS_HELP_STRING([--enable-module@<:@=CPATH@:>@],
					[install Lua interpreter module to CPATH (use default)])],
	      [enable_module="$enableval"], [enable_module=yes])
if test "x${enable_module}" == "xyes"; then
	enable_module="`pkg-config lua --variable=INSTALL_CMOD`"
fi
AM_CONDITIONAL([ENABLE_MODULE], [test "x${enable_module}" != "xno"])
AC_SUBST([LUA_CPATH], ["$enable_module"])

#####################
# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h errno.h limits.h])
AC_CHECK_HEADERS([netdb.h sys/ioctl.h sys/socket.h sys/resource.h sys/uio.h])
AX_LUA_HEADERS
if test "x${ac_cv_header_lua_h}" != "xyes"; then
	AC_MSG_ERROR([Lua headers are required for building.])
fi

#####################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

#####################
# Checks for library functions.
AC_CHECK_FUNCS([memset socket writev])
AC_FUNC_STRERROR_R

#####################
AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile])
AC_OUTPUT
