/* Copyright (c) 2010 Ian C. Good
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>
#include <netdb.h>
#include <errno.h>

#include "misc.h"

#define get_fd(L, i) *(int *) luaL_checkudata (L, i, "ratchet_socket_meta")

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rsock_new() */
static int rsock_new (lua_State *L)
{
	int family = luaL_optint (L, 1, AF_INET);
	int socktype = luaL_optint (L, 2, SOCK_STREAM);
	int protocol = luaL_optint (L, 3, 0);

	int *fd = (int *) lua_newuserdata (L, sizeof (int));
	*fd = socket (family, socktype, protocol);
	if (*fd < 0)
		return raise_perror (L);

	if (set_nonblocking (*fd) < 0)
		return raise_perror (L);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	return 1;
}
/* }}} */

/* {{{ rsock_parse_unix_uri() */
static int rsock_parse_unix_uri (lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);

	struct sockaddr_un *addr = (struct sockaddr_un *) lua_newuserdata (L, sizeof (struct sockaddr_un));
	memset (addr, 0, sizeof (struct sockaddr_un));

	addr->sun_family = AF_UNIX;
	strncpy (addr->sun_path, path, sizeof (addr->sun_path) - 1);

	return 1;
}
/* }}} */

/* {{{ rsock_parse_tcp_uri() */
static int rsock_parse_tcp_uri (lua_State *L)
{
	luaL_checkstring (L, 1);
	lua_settop (L, 1);

	/* Check for form: schema://[127.0.0.1]:25
	 * or: schema://[01:02:03:04:05:06:07:08]:25 */
	if (strmatch (L, 1, "^%/+%[(.-)%](%:?)(%d*)$"))
	{
		if (!strequal (L, -2, ":") || strequal (L, -1, ""))
		{
			lua_pop (L, 1);
			lua_pushnil (L);
		}
		lua_remove (L, -2);
	}

	/* Check for form: schema://127.0.0.1:25 */
	else if (strmatch (L, 1, "^%/+([^%:]*)(%:?)(%d*)$"))
	{
		if (!strequal (L, -2, ":") || strequal (L, -1, ""))
		{
			/* Send nil as the port/service if not given. */
			lua_pop (L, 1);
			lua_pushnil (L);
		}
		lua_remove (L, -2);
	}

	/* Check for form: schema://127.0.0.1
	 * or: schema://01:02:03:04:05:06:07:08 */
	else if (strmatch (L, 1, "^%/+(.*)$"))
		lua_pushnil (L);	/* Send nil as the port/service no matter what. */

	/* Unrecognized tcp URI, return nothing. */
	else
		return 0;

	/* Turn the ip/host and port/service into something getaddrinfo_a() can use. */
	lua_settop (L, 3);


	return 2;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rsock_gc() */
static int rsock_gc (lua_State *L)
{
	int fd = get_fd (L, 1);
	if (fd >= 0)
		close (fd);

	return 0;
}
/* }}} */

/* {{{ rsock_getfd() */
static int rsock_getfd (lua_State *L)
{
	int fd = get_fd (L, 1);
	lua_pushinteger (L, fd);
	return 1;
}
/* }}} */

/* {{{ rsock_check_error() */
static int rsock_check_error (lua_State *L)
{
	int sockfd = get_fd (L, 1);
	int error;
	socklen_t errorlen = sizeof (int);

	if (getsockopt (sockfd, SOL_SOCKET, SO_ERROR, (void *) &error, &errorlen) < 0)
		return raise_perror (L);

	if (error)
	{
		errno = error;
		return raise_perror (L);
	}

	return 0;
}
/* }}} */

/* {{{ rsock_bind() */
static int rsock_bind (lua_State *L)
{
	int sockfd = get_fd (L, 1);
	luaL_checktype (L, 2, LUA_TUSERDATA);
	struct sockaddr *addr = (struct sockaddr *) lua_touserdata (L, 2);
	socklen_t addrlen = (socklen_t) lua_objlen (L, 2);

	int ret = bind (sockfd, addr, addrlen);
	if (ret < 0)
		return raise_perror (L);

	return 0;
}
/* }}} */

/* {{{ rsock_listen() */
static int rsock_listen (lua_State *L)
{
	int sockfd = get_fd (L, 1);
	int backlog = luaL_optint (L, 2, SOMAXCONN);

	int ret = listen (sockfd, backlog);
	if (ret < 0)
		return raise_perror (L);

	return 0;
}
/* }}} */

/* {{{ rsock_rawconnect() */
static int rsock_rawconnect (lua_State *L)
{
	int sockfd = get_fd (L, 1);
	luaL_checktype (L, 2, LUA_TUSERDATA);
	struct sockaddr *addr = (struct sockaddr *) lua_touserdata (L, 2);
	socklen_t addrlen = (socklen_t) lua_objlen (L, 2);

	int ret = connect (sockfd, addr, addrlen);
	if (ret < 0)
	{
		if (errno == EALREADY || errno == EINPROGRESS || errno == EISCONN)
			lua_pushboolean (L, 0);
		else
			return raise_perror (L);
	}
	else
		lua_pushboolean (L, 1);

	return 1;
}
/* }}} */

/* ---- Lua-implemented Functions ------------------------------------------- */

/* {{{ send() */
#define rsock_send "return function (self, ...)\ncoroutine.yield('write', self:getfd())\nself:rawsend(...)\nend\n"
/* }}} */

/* {{{ recv() */
#define rsock_recv "return function (self, ...)\ncoroutine.yield('read', self:getfd())\nreturn self:rawrecv(...)\nend\n"
/* }}} */

/* {{{ accept() */
#define rsock_accept "return function (self, ...)\ncoroutine.yield('read', self:getfd())\nreturn self:rawaccept(...)\nend\n"
/* }}} */

/* {{{ connect() */
#define rsock_connect "return function (self, ...)\n" \
		"if not self:rawconnect(...) then\n" \
			"coroutine.yield('write', self:getfd())\n" \
			"self:check_error()\n" \
		"end\n" \
	"end\n"
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_socket() */
int luaopen_ratchet_socket (lua_State *L)
{
	/* Static functions in the ratchet.socket namespace. */
	static const luaL_Reg funcs[] = {
		{"new", rsock_new},
		//{"connect", rsock_new_connect},
		//{"listen", rsock_new_listen},
		{"parse_unix_uri", rsock_parse_unix_uri},
		{"parse_tcp_uri", rsock_parse_tcp_uri},
		{NULL}
	};

	/* Meta-methods for ratchet.socket object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rsock_gc},
		{NULL}
	};

	/* Methods in the ratchet.socket class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"getfd", rsock_getfd},
		{"bind", rsock_bind},
		{"listen", rsock_listen},
		{"check_error", rsock_check_error},
		/* Undocumented, helper methods. */
		{"rawconnect", rsock_rawconnect},
		{NULL}
	};

	/* Methods in the ratchet.socket class implemented in Lua. */
	static const struct luafunc luameths[] = {
		/* Documented methods. */
		{"send", rsock_send},
		{"recv", rsock_recv},
		{"accept", rsock_accept},
		{"connect", rsock_connect},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Set up the ratchet.socket class and metatables. */
	luaL_newmetatable (L, "ratchet_socket_meta");
	lua_newtable (L);
	luaI_openlib (L, NULL, meths, 0);
	register_luafuncs (L, -1, luameths);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, metameths, 0);
	lua_pop (L, 1);

	/* Set up the ratchet.socket namespace functions. */
	luaI_openlib (L, "ratchet.socket", funcs, 0);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
