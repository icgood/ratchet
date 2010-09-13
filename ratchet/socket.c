#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "socket.h"

/* {{{ fd_set_nonblocking() */
static int fd_set_nonblocking (int fd)
{
	int flags;

#ifdef O_NONBLOCK
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIONBIO, &flags);
#endif
}
/* }}} */

/* {{{ mysocket_init() */
static int mysocket_init (lua_State *L)
{
	struct sockaddr *addr = NULL;
	int fd = -1, use_udp = 0;

	/* Grab the named parameters. */
	for (lua_pushnil (L); lua_next (L, 2) != 0; lua_pop (L, 1))
	{
		if (luaH_strequal (L, -2, "fd") && lua_isnumber (L, -1))
		{
			fd = lua_tointeger (L, -1);
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "fd");
		}
		else if (luaH_strequal (L, -2, "sockaddr"))
		{
			addr = (struct sockaddr *) lua_touserdata (L, -1);
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "sockaddr");
		}
		else if (luaH_strequal (L, -2, "use_udp"))
			use_udp = lua_toboolean (L, -1);
	}

	if (!addr)
		luaL_error (L, "Sockets must be initialized with address information!");

	if (fd < 0)
	{
		fd = socket ((int) addr->sa_family, (use_udp ? SOCK_DGRAM : SOCK_STREAM), 0);
		if (fd < 0)
			return luaH_perror (L);
		lua_pushinteger (L, fd);
		lua_setfield (L, 1, "fd");
	}

	if (fd_set_nonblocking (fd) < 0)
		return luaH_perror (L);

	return 0;
}
/* }}} */

/* {{{ mysocket_del() */
static int mysocket_del (lua_State *L)
{
	int fd;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	shutdown (fd, SHUT_RDWR);
	close (fd);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ mysocket_getfd() */
static int mysocket_getfd (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	return 1;
}
/* }}} */

/* {{{ mysocket_listen() */
static int mysocket_listen (lua_State *L)
{
	struct sockaddr *addr;
	int fd;
	size_t len;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);

	lua_getfield (L, 1, "sockaddr");
	addr = (struct sockaddr *) lua_touserdata (L, -1);
	len = lua_objlen (L, -1);

	if (bind (fd, addr, len) < 0)
		return luaH_perror (L);

	lua_pop (L, 2);

	return 0;
}
/* }}} */

/* {{{ mysocket_connect() */
static int mysocket_connect (lua_State *L)
{
	struct sockaddr *addr;
	int fd;
	size_t len;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);

	lua_getfield (L, 1, "sockaddr");
	addr = (struct sockaddr *) lua_touserdata (L, -1);
	len = lua_objlen (L, -1);

	errno = 0;
	if (connect (fd, addr, len) < 0 && errno != EINPROGRESS)
		return luaH_perror (L);

	lua_pop (L, 2);

	return 0;
}
/* }}} */

/* {{{ mysocket_accept() */
static int mysocket_accept (lua_State *L)
{
	int fd;
	struct sockaddr *addr;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);

	lua_getfield (L, 1, "sockaddr");
	addr = (struct sockaddr *) lua_touserdata (L, -1);

	lua_pop (L, 2);

	return 0;
}
/* }}} */

/* {{{ luaopen_luah_ratchet_socket() */
int luaopen_luah_ratchet_socket (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", mysocket_init},
		{"del", mysocket_del},
		{"getfd", mysocket_getfd},
		{"listen", mysocket_listen},
		{"connect", mysocket_connect},
		{"accept", mysocket_accept},
		{NULL}
	};

	luaH_newclass (L, "luah.ratchet.socket", meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
