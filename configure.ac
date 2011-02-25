#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([ratchet], [0.5.2], [Ian Good <icgood@gmail.com>])
AM_INIT_AUTOMAKE([color-tests silent-rules])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ratchet.c])
AC_CONFIG_HEADERS([config.h])

#####################
# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

#####################
# Configure options: --disable-socket[=no]
AC_ARG_ENABLE([socket], [AS_HELP_STRING([--disable-socket],
	                                [disable usage of BSD socket libraries (default no)])],
	      [use_socket="$enableval"], [use_socket=yes])

#####################
# Configure options: --disable-zeromq[=no]
AC_ARG_ENABLE([zeromq], [AS_HELP_STRING([--disable-zeromq],
	                                [disable usage of ZeroMQ libraries (default no)])],
	      [use_zeromq="$enableval"], [use_zeromq=yes])

#####################
# Configure options: --disable-udns[=no]
AC_ARG_ENABLE([udns], [AS_HELP_STRING([--disable-udns],
	                              [disable usage of UDNS libraries (default no)])],
	      [use_udns="$enableval"], [use_udns=yes])

#####################
# Configure options: --disable-timerfd[=no]
AC_ARG_ENABLE([timerfd], [AS_HELP_STRING([--disable-timerfd],
	      				 [disable usage of timerfd calls (default no)])],
	      [use_timerfd="$enableval"], [use_timerfd=yes])

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
else
	AC_DEFINE([HAVE_LIBEVENT], [1])
fi
AM_CONDITIONAL([HAVE_LIBEVENT], [test "x${have_libevent}" = "xyes"])

# UDNS
AC_DEFINE([HAVE_UDNS], [0], [Define to 1 if you have the udns library.])
if test "x${use_udns}" != "xno"; then
	AC_CHECK_HEADER([udns.h], [have_udns=yes], [have_udns=no])
	AC_CHECK_HEADER([arpa/inet.h], [have_udns=yes], [have_udns=no])
	AC_SEARCH_LIBS([dns_init], [udns], [], [have_udns=no])
	if test "x${have_udns}" != "xyes"; then
		AC_MSG_ERROR([UDNS libraries required for building (or --disable-udns).])
	else
		AC_DEFINE([HAVE_UDNS], [1])
	fi
else
	AC_MSG_NOTICE([UDNS will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_UDNS], [test "x${have_udns}" = "xyes"])

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

# timerfd
AC_DEFINE([HAVE_TIMERFD], [0], [Define to 1 if you have the timerfd system call suite.])
if test "x${use_timerfd}" != "xno"; then
	AC_CHECK_HEADERS([sys/timerfd.h], [have_timerfd=yes], [have_timerfd=no])
	AC_CHECK_FUNC([timerfd_create], [], [have_timerfd=no])
	if test "x${have_timerfd}" != "xyes"; then
		AC_MSG_ERROR([The timerfd libraries required for building.])
	else
		AC_DEFINE([HAVE_TIMERFD], [1])
	fi
else
	AC_MSG_NOTICE([timerfd will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_TIMERFD], [test "x${have_timerfd}" = "xyes"])

# socket
AC_DEFINE([HAVE_SOCKET], [0], [Define to 1 if you have the BSD socket system call suite.])
if test "x${use_socket}" != "xno"; then
	AC_CHECK_HEADERS([sys/socket.h], [have_socket=yes], [have_socket=no])
	AC_CHECK_FUNCS([socket getsockopt setsockopt], [], [have_socket=no])
	if test "x${have_socket}" != "xyes"; then
		AC_MSG_ERROR([The BSD socket libraries required for building.])
	else
		AC_DEFINE([HAVE_SOCKET], [1])
	fi
else
	AC_MSG_NOTICE([socket will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_SOCKET], [test "x${have_socket}" = "xyes"])

#####################
# Configure options: BUFSIZ=nnn
AC_ARG_VAR([BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
if test -n "$BUFSIZ"; then
	AC_DEFINE_UNQUOTED([BUFSIZ], [$BUFSIZ], [The size of the intermediate buffers used when building large Lua strings.])
fi

#####################
# Configure options: --disable-devel.
AC_ARG_ENABLE([devel], [AS_HELP_STRING([--enable-devel@<:@=yes@:>@],
				       [install development headers and libraries.])],
	      [enable_devel="${enableval}"], [enable_devel=yes])
AM_CONDITIONAL([ENABLE_DEVEL], [test "x${enable_devel}" != "xno"])

#####################
# Configure options: --disable-module.
AC_ARG_ENABLE([module], [AS_HELP_STRING([--enable-module@<:@=CPATH@:>@],
					[install Lua interpreter module to CPATH, which uses pkg-config for default.])],
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
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

#####################
# Checks for library functions.
AC_CHECK_FUNCS([memset writev modf])
AC_FUNC_STRERROR_R

#####################
AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile])
AC_OUTPUT
