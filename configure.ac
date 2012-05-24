#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([ratchet], [0.8.2], [Ian Good <icgood@gmail.com>])
AM_INIT_AUTOMAKE([color-tests silent-rules])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/c/ratchet.c])
AC_CONFIG_HEADERS([config.h])

#####################
# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
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
# Configure options: --disable-dns[=no]
AC_ARG_ENABLE([dns], [AS_HELP_STRING([--disable-dns],
	                             [disable usage of dns libraries (default no)])],
	      [use_dns="$enableval"], [use_dns=yes])

#####################
# Configure options: --disable-timerfd[=no]
AC_ARG_ENABLE([timerfd], [AS_HELP_STRING([--disable-timerfd],
	                                 [disable usage of timerfd calls (default no)])],
	      [use_timerfd="$enableval"], [use_timerfd=yes])

#####################
# Configure options: --disable-openssl[=no]
AC_ARG_ENABLE([openssl], [AS_HELP_STRING([--disable-openssl],
	                                 [disable usage of openssl libraries (default no)])],
	      [use_openssl="$enableval"], [use_openssl=yes])

#####################
# Configure options: --with-lua-cpath=CPATH
AC_MSG_CHECKING([Lua C module path])
AC_ARG_WITH([lua-cpath], [AS_HELP_STRING([--with-lua-cpath=CPATH],
	                                 [Install Lua C modules to CPATH])])
if test "x${with_lua_cpath}" == "x"; then
	with_lua_cpath="`pkg-config lua --variable=INSTALL_CMOD 2>&1`"
	if test $? -ne 0 -o "x${with_lua_cpath}" == "x"; then
		with_lua_cpath="/usr/lib/lua/5.2"
	fi
fi
AC_MSG_RESULT([$with_lua_cpath])
AC_SUBST([LUA_CPATH], ["$with_lua_cpath"])

#####################
# Configure options: --with-lua-lpath=LPATH
AC_MSG_CHECKING([Lua module path])
AC_ARG_WITH([lua-lpath], [AS_HELP_STRING([--with-lua-lpath=LPATH],
	                                 [Install Lua modules to LPATH])])
if test "x${with_lua_lpath}" == "x"; then
	with_lua_lpath="`pkg-config lua --variable=INSTALL_LMOD 2>&1`"
	if test $? -ne 0 -o "x${with_lua_lpath}" == "x"; then
		with_lua_lpath="/usr/share/lua/5.2"
	fi
fi
AC_MSG_RESULT([$with_lua_lpath])
AC_SUBST([LUA_LPATH], ["$with_lua_lpath"])

#####################
# Checks for libraries.

# Lua
AX_WITH_LUA
if test "x${LUA}" == "x"; then
	AC_MSG_NOTICE([Lua interpreter not found, disabling build tests.])
else
	AX_WITH_LUA([502])
fi
AX_LUA_HEADERS
AX_LUA_HEADERS_VERSION([502])
AX_LUA_LIBS
if test "x${LUA_LIB}" == "x"; then
	AC_MSG_ERROR([Lua development libraries required for building.])
fi
AC_SUBST([LUA], ["$LUA"])
AM_CONDITIONAL([HAVE_LUA_INTERPRETER], [test "x${LUA}" != "x"])

# Libevent
AC_DEFINE([HAVE_LIBEVENT], [0], [Define to 1 if you have the libevent library.])
AC_CHECK_HEADERS([event2/event.h], [have_libevent=yes], [have_libevent=no])
AC_SEARCH_LIBS([event_assign], [event], [], [have_libevent=no])
if test "x${have_libevent}" != "xyes"; then
	AC_MSG_ERROR([Libevent libraries required for building.])
else
	AC_DEFINE([HAVE_LIBEVENT], [1])
fi
AM_CONDITIONAL([HAVE_LIBEVENT], [test "x${have_libevent}" = "xyes"])

# OpenSSL
AC_DEFINE([HAVE_OPENSSL], [0], [Define to 1 if you have the openssl library.])
if test "x${use_openssl}" != "xno"; then
	AX_CHECK_OPENSSL([have_openssl=yes], [have_openssl=no])
	if test "x${have_openssl}" != "xyes"; then
		AC_MSG_ERROR([OpenSSL libraries required for building (or --disable-openssl).])
	else
		AC_DEFINE([HAVE_OPENSSL], [1])
	fi
else
	AC_MSG_NOTICE([OpenSSL will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_OPENSSL], [test "x${have_openssl}" = "xyes"])

# DNS
AC_DEFINE([HAVE_DNS], [0], [Define to 1 if you have the dns library.])
if test "x${use_dns}" != "xno"; then
	AC_DEFINE([HAVE_DNS], [1])
else
	AC_MSG_NOTICE([dns will not be included in the ratchet library.])
fi
AM_CONDITIONAL([HAVE_DNS], [test "x${use_dns}" = "xyes"])

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
# Configure options: --disable-devel
AC_ARG_ENABLE([devel], [AS_HELP_STRING([--disable-devel],
	                               [do not install development headers and libraries.])],
	      [enable_devel="${enableval}"], [enable_devel=yes])
AM_CONDITIONAL([ENABLE_DEVEL], [test "x${enable_devel}" != "xno"])

#####################
# Configure options: --disable-module
AC_ARG_ENABLE([module], [AS_HELP_STRING([--enable-module@<:@=PATH@:>@],
	                                [install Lua interpreter module to Lua C module installation path, or PATH if given.])],
	      [enable_module="${enableval%/}"], [enable_module=yes])
if test "x${enable_module}" != "xno"; then
	if test "x${enable_module}" == "xyes"; then
		enable_module="$with_lua_cpath"
	fi
	if test "x${enable_module}" == "x"; then
		AC_MSG_ERROR([Please provide PATH or --with-lua-cpath=CPATH.])
	fi
fi
AM_CONDITIONAL([ENABLE_MODULE], [test "x${enable_module}" != "xno"])
AC_SUBST([MODULE_PATH], ["$enable_module"])

#####################
# Configure options: --disable-http
AC_ARG_ENABLE([http], [AS_HELP_STRING([--disable-http],
	                              [Disable installation of ratchet.http modules.])],
	      [enable_http="${enableval%/}"], [enable_http=yes])
AM_CONDITIONAL([ENABLE_HTTP], [test "x${enable_http}" != "xno"])

#####################
# Configure options: --disable-socketpad
AC_ARG_ENABLE([socketpad], [AS_HELP_STRING([--disable-socketpad],
	                                   [Disable installation of ratchet.socketpad modules.])],
	      [enable_socketpad="${enableval%/}"], [enable_socketpad=yes])
if test "x${enable_socketpad}" != "xno"; then
	if test "x${have_socket}" = "xno"; then
		enable_socketpad=no
	fi
fi
AM_CONDITIONAL([ENABLE_SOCKETPAD], [test "x${enable_socketpad}" != "xno"])

#####################
# Configure options: --disable-bus
AC_ARG_ENABLE([bus], [AS_HELP_STRING([--disable-bus],
	                             [Disable installation of ratchet.bus modules.])],
	      [enable_bus="${enableval%/}"], [enable_bus=yes])
AM_CONDITIONAL([ENABLE_BUS], [test "x${enable_bus}" != "xno"])

#####################
# Configure options: --disable-smtp
AC_ARG_ENABLE([smtp], [AS_HELP_STRING([--disable-smtp],
	                              [Disable installation of ratchet.smtp modules.])],
	      [enable_smtp="${enableval%/}"], [enable_smtp=yes])
AM_CONDITIONAL([ENABLE_SMTP], [test "x${enable_smtp}" != "xno"])

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
AC_CHECK_FUNCS([memset writev modf sigaction])
AC_FUNC_STRERROR_R

#####################
AC_CONFIG_FILES([Makefile
	         src/c/Makefile
	         src/c/dns/Makefile
	         src/c/dns/libdns/Makefile
		 src/lua/Makefile
	         test/Makefile])
AC_OUTPUT
