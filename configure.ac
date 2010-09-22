#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([luah], [0.0], [Ian Good <icgood@gmail.com>])
AM_INIT_AUTOMAKE
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([luah/luah.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

# Checks for libraries.
AX_LUA_LIBS
if test "x${LUA_LIB}" == "x"; then
	AC_MSG_ERROR([Lua libraries required for building.])
fi

AC_DEFINE([HAVE_EPOLL], [0], [Define to 1 if you have the Linux epoll(7) interface.])
AC_DEFINE([HAVE_EPOLL_PWAIT], [0], [Define to 1 if you have the epoll_pwait function.])
AX_HAVE_EPOLL([AC_DEFINE([HAVE_EPOLL], [1])], [])
AX_HAVE_EPOLL_PWAIT([AC_DEFINE([HAVE_EPOLL_PWAIT], [1])], [])

AC_CHECK_HEADERS([expat.h], [have_expat=yes], [have_expat=no])
AC_SEARCH_LIBS([XML_ParserCreate], [expat], [], [have_expat=no])
if test "x${have_expat}" != "xyes"; then
	AC_MSG_ERROR([Expat headers and libraries are required for building.])
fi

AC_CHECK_HEADERS([zmq.h], [have_zmq=yes], [have_zmq=no])
AC_SEARCH_LIBS([zmq_init], [zmq], [], [have_zmq=no])
if test "x${have_zmq}" != "xyes"; then
	AC_MSG_ERROR([ZeroMQ headers and libraries are required for building.])
fi

# Configure options: --enable-devel[=no].
AC_ARG_ENABLE([devel], [AS_HELP_STRING([--enable-devel],
	                               [install developer headers and libraries (default no)])],
	      [enable_devel=yes], [enable_devel=no])
AM_CONDITIONAL([ENABLE_DEVEL], [test "x${enable_devel}" != "xno"])

# Configure options: --disable-module[=yes].
AC_ARG_ENABLE([module], [AS_HELP_STRING([--enable-module@<:@=CPATH@:>@],
					[install Lua interpreter module to CPATH (use default)])],
	      [enable_module="$enableval"], [enable_module="`pkg-config lua --variable=INSTALL_CMOD`"])
AM_CONDITIONAL([ENABLE_MODULE], [test "x${enable_module}" != "xno"])
AC_SUBST([LUA_CPATH], ["$enable_module"])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h strings.h errno.h limits.h])
AC_CHECK_HEADERS([netdb.h sys/ioctl.h sys/socket.h sys/resource.h sys/uio.h])
AX_LUA_HEADERS
if test "x${ac_cv_header_lua_h}" != "xyes"; then
	AC_MSG_ERROR([Lua headers are required for building.])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset socket writev])
AC_FUNC_STRERROR_R

AC_CONFIG_FILES([Makefile
		 misc/Makefile
		 makeclass/Makefile
		 rlimit/Makefile
		 zmq/Makefile
		 epoll/Makefile
		 socket/Makefile
		 dns/Makefile
		 ratchet/Makefile
		 xml/Makefile
		 luah/Makefile
		 test/Makefile])
AC_OUTPUT
