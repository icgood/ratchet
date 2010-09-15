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

/* {{{ fd_set_blocking_flag() */
static int fd_set_blocking_flag (int fd, int blocking)
{
	int flags;

#ifdef O_NONBLOCK
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	if (!blocking)
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	else
		return fcntl(fd, F_SETFL, flags & (~O_NONBLOCK));
#else
	flags = (blocking ? 0 : 1);
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

	return 0;
}
/* }}} */

/* {{{ mysocket_close() */
static int mysocket_close (lua_State *L)
{
	int fd;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	shutdown (fd, SHUT_RDWR);
	close (fd);
	lua_pop (L, 1);

	lua_pushinteger (L, -1);
	lua_setfield (L, 1, "fd");

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

/* {{{ mysocket_set_blocking() */
static int mysocket_set_blocking (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	fd_set_blocking_flag (lua_tointeger (L, -1), 1);
	return 1;
}
/* }}} */

/* {{{ mysocket_set_nonblocking() */
static int mysocket_set_nonblocking (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	fd_set_blocking_flag (lua_tointeger (L, -1), 0);
	return 1;
}
/* }}} */

/* {{{ mysocket_listen() */
static int mysocket_listen (lua_State *L)
{
	struct sockaddr *addr;
	int fd, backlog = 30;
	size_t len;
	int one = 1;

	if (lua_isnumber (L, 2))
		backlog = lua_tointeger (L, 2);

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);

	lua_getfield (L, 1, "sockaddr");
	addr = (struct sockaddr *) lua_touserdata (L, -1);
	len = lua_objlen (L, -1);

	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));

	if (bind (fd, addr, len) < 0)
		return luaH_perror (L);

	if (listen (fd, backlog) < 0)
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
	int fd, newfd;
	struct sockaddr *addr;
	socklen_t addrlen;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	lua_getfield (L, 1, "sockaddr");
	addrlen = (socklen_t) lua_objlen (L, -1);
	lua_pop (L, 2);

	lua_getfield (L, 1, "class");
	lua_newtable (L);

	addr = (struct sockaddr *) lua_newuserdata (L, (size_t) addrlen);
	if ((newfd = accept (fd, addr, &addrlen)) < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		return luaH_perror (L);
	lua_setfield (L, -2, "sockaddr");

	lua_pushinteger (L, newfd);
	lua_setfield (L, -2, "fd");

	return luaH_callfunction (L, -2, 1);
}
/* }}} */

/* {{{ mysocket_send() */
static int mysocket_send (lua_State *L)
{
	int fd;
	const char *str;
	size_t strlen;
	ssize_t ret;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	str = lua_tolstring (L, 2, &strlen);

	ret = send (fd, str, strlen, 0);
	if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		return luaH_perror (L);

	return 0;
}
/* }}} */

/* {{{ mysocket_recv() */
static int mysocket_recv (lua_State *L)
{
	int fd;
	char *prepped;
	luaL_Buffer buffer;
	ssize_t ret = 1;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	luaL_buffinit (L, &buffer);
	
	for (prepped = luaL_prepbuffer (&buffer); ret > 0; luaL_addsize (&buffer, (size_t) ret))
	{
		prepped = luaL_prepbuffer (&buffer);
		ret = recv (fd, prepped, LUAL_BUFFERSIZE, 0);
		if (ret == 0)
			continue;
		if (ret < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				ret = 0;
				continue;
			}
			else
				return luaH_perror (L);
		}
	}

	luaL_pushresult (&buffer);

	return 1;
}
/* }}} */

/* {{{ luaopen_luah_socket() */
int luaopen_luah_socket (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", mysocket_init},
		{"getfd", mysocket_getfd},
		{"listen", mysocket_listen},
		{"set_blocking", mysocket_set_blocking},
		{"set_nonblocking", mysocket_set_nonblocking},
		{"connect", mysocket_connect},
		{"send", mysocket_send},
		{"recv", mysocket_recv},
		{"accept", mysocket_accept},
		{"close", mysocket_close},
		{NULL}
	};

	luaH_newclass (L, "luah.socket", meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
