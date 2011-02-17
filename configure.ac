#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([ratchet], [0.4.1], [Ian Good <icgood@gmail.com>])
AM_INIT_AUTOMAKE([color-tests silent-rules])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ratchet.c])
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
# Configure options: --disable-zeromq[=no]
AC_ARG_ENABLE([zeromq], [AS_HELP_STRING([--disable-zeromq],
	                                [disable usage of ZeroMQ libraries (default no)])],
	      [use_zeromq="$enableval"], [use_zeromq=yes])

#####################
# Checks for libraries.

# Lua
AX_WITH_LUA
if test "x${LUA}" == "x"; then
	AC_MSG_NOTICE([Lua interpreter not found, disabling build tests.])
else
	AX_LUA_VERSION([501])
fi
AX_LUA_HEADERS
AX_LUA_HEADERS_VERSION([501])
AX_LUA_LIBS
if test "x${LUA_LIB}" == "x"; then
	AC_MSG_ERROR([Lua development libraries required for building.])
fi
AC_SUBST([LUA], ["$LUA"])
AM_CONDITIONAL([HAVE_LUA_INTERPRETER], [test "x${LUA}" != "x"])

# Libevent
AC_DEFINE([HAVE_LIBEVENT], [0], [Define to 1 if you have the libevent library.])
AC_CHECK_HEADERS([event.h], [have_libevent=yes], [have_libevent=no])
AC_SEARCH_LIBS([event_set], [event], [], [have_libevent=no])
if test "x${have_libevent}" != "xyes"; then
	AC_MSG_ERROR([Libevent libraries required for building.])
fi
AM_CONDITIONAL([HAVE_LIBEVENT], [test "x${have_libevent}" = "xyes"])

# ZeroMQ 2.1.0+
have_zmq=no
AC_DEFINE([HAVE_ZMQ], [0], [Define to 1 if you have the ZeroMQ library.])
if test "x${use_zeromq}" != "xno"; then
	AC_CHECK_HEADERS([zmq.h], [have_zmq=yes], [have_zmq=no])
	AC_SEARCH_LIBS([zmq_init], [zmq], [], [have_zmq=no])
	AC_MSG_CHECKING([for ZeroMQ version 2.1.0 or higher])
	AC_COMPUTE_INT([have_zmq_version], [ZMQ_VERSION], [@%:@include <zmq.h>], [have_zmq_version=0])
	AS_IF([test $have_zmq_version -ge 20100], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no]); have_zmq=no])
	if test "x${have_zmq}" != "xyes"; then
		AC_MSG_ERROR([ZeroMQ libraries required for building (or --disable-zeromq).])
	else
		AC_DEFINE([HAVE_ZMQ], [1])
	fi
else
	AC_MSG_NOTICE([ZeroMQ will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_ZMQ], [test "x${have_zmq}" = "xyes"])

# signalfd
AC_CHECK_HEADERS([sys/signalfd.h], [have_signalfd=yes], [have_signalfd=no])
AC_CHECK_FUNC([signalfd], [], [have_signalfd=no])
if test "x${have_signalfd}" != "xyes"; then
	AC_MSG_ERROR([The signalfd libraries required for building.])
fi

# timerfd
AC_CHECK_HEADERS([sys/timerfd.h], [have_timerfd=yes], [have_timerfd=no])
AC_CHECK_FUNC([timerfd_create], [], [have_timerfd=no])
if test "x${have_timerfd}" != "xyes"; then
	AC_MSG_ERROR([The timerfd libraries required for building.])
fi

# getaddrinfo_a()
AC_SEARCH_LIBS([getaddrinfo_a], [anl], [], [AC_MSG_ERROR([getaddrinfo_a() is not available for asynchronous DNS resolution.])])

#####################
# Configure options: BUFSIZ=nnn
AC_ARG_VAR([BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
if test -n "$BUFSIZ"; then
	AC_DEFINE_UNQUOTED([BUFSIZ], [$BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
fi

#####################
# Configure options: --disable-module[=yes].
AC_ARG_ENABLE([module], [AS_HELP_STRING([--enable-module@<:@=CPATH@:>@],
					[install Lua interpreter module to CPATH (use default)])],
	      [enable_module="${enableval%/}"], [enable_module=yes])
if test "x${enable_module}" == "xyes"; then
	enable_module="`pkg-config lua --variable=INSTALL_CMOD`"
fi
if test "x${enable_module}" == "x"; then
	AC_MSG_ERROR([Please provide --enable-module=CPATH with Lua module directory.])
fi
AM_CONDITIONAL([ENABLE_MODULE], [test "x${enable_module}" != "xno"])
AC_SUBST([LUA_CPATH], ["$enable_module"])

#####################
# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h errno.h limits.h])
AC_CHECK_HEADERS([netdb.h sys/ioctl.h sys/socket.h sys/resource.h sys/uio.h])
AC_CHECK_HEADERS([net/if.h fcntl.h sys/time.h])
AX_LUA_HEADERS
if test "x${ac_cv_header_lua_h}" != "xyes"; then
	AC_MSG_ERROR([Lua headers are required for building.])
fi

#####################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

#####################
# Checks for library functions.
AC_CHECK_FUNCS([memset socket writev modf])
AC_FUNC_STRERROR_R

#####################
AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile])
AC_OUTPUT
