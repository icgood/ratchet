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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include "ratchet.h"
#include "misc.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#ifndef DEFAULT_TCPUDP_PORT
#define DEFAULT_TCPUDP_PORT 80
#endif

#define socket_fd(L, i) (*((int *) luaL_checkudata (L, i, "ratchet_socket_meta")))

#if HAVE_OPENSSL
int rsock_get_encryption (lua_State *L);
int rsock_encrypt (lua_State *L);
#endif

int rsockopt_get (lua_State *L);
int rsockopt_set (lua_State *L);

/* {{{ push_inet_ntop() */
static int push_inet_ntop (lua_State *L, struct sockaddr *addr)
{
	if (addr->sa_family == AF_INET)
	{
		char buffer[INET_ADDRSTRLEN];
		struct in_addr *in = &((struct sockaddr_in *) addr)->sin_addr;
		if (!inet_ntop (AF_INET, in, buffer, INET_ADDRSTRLEN))
			return ratchet_error_errno (L, NULL, "inet_ntop");
		lua_pushstring (L, buffer);
	}

	else if (addr->sa_family == AF_INET6)
	{
		char buffer[INET6_ADDRSTRLEN];
		struct in6_addr *in = &((struct sockaddr_in6 *) addr)->sin6_addr;
		if (!inet_ntop (AF_INET6, in, buffer, INET6_ADDRSTRLEN))
			return ratchet_error_errno (L, NULL, "inet_ntop");
		lua_pushstring (L, buffer);
	}

	else
		lua_pushnil (L);

	return 1;
}
/* }}} */

/* {{{ call_tracer() */
static int call_tracer (lua_State *L, int index, const char *type, int args)
{
	lua_getuservalue (L, index);
	lua_getfield (L, -1, "tracer");
	if (!lua_toboolean (L, -1))
	{
		lua_pop (L, 2+args);
		return 0;
	}
	lua_remove (L, -2);
	lua_insert (L, -args-1);
	lua_pushstring (L, type);
	lua_insert (L, -args-1);

	lua_call (L, args+1, 0);

	return 0;
}
/* }}} */

/* {{{ throw_fd_errors() */
static int throw_fd_errors (lua_State *L, int fd)
{
	int error = 0;
	socklen_t errorlen = sizeof (int);

	if (getsockopt (fd, SOL_SOCKET, SO_ERROR, (void *) &error, &errorlen) < 0)
		return ratchet_error_errno (L, NULL, "getsockopt");

	return 0;
}
/* }}} */

/* {{{ push_query_types_table() */
static void push_query_types_table (lua_State *L, int index)
{
	if (lua_isnoneornil (L, index))
		goto other;

	else if (strequal (L, index, "AF_UNSPEC"))
		goto other;

	else if (strequal (L, index, "AF_INET"))
	{
		lua_createtable (L, 1, 0);
		lua_pushliteral (L, "a");
		lua_rawseti (L, -2, 1);
	}

	else if (strequal (L, index, "AF_INET6"))
	{
		lua_createtable (L, 1, 0);
		lua_pushliteral (L, "aaaa");
		lua_rawseti (L, -2, 1);
	}

	else
		luaL_argerror (L, index, "Unknown socket address family.");

	return;

other:
	/* nil or "AF_UNSPEC", etc. */
	lua_createtable (L, 2, 0);
	lua_pushliteral (L, "aaaa");
	lua_rawseti (L, -2, 1);
	lua_pushliteral (L, "a");
	lua_rawseti (L, -2, 2);
}
/* }}} */

/* {{{ build_tcp_info() */
static int build_tcp_info (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TTABLE);
	luaL_checkstring (L, 2);
	int port = luaL_checkint (L, 3);
	lua_settop (L, 3);

	lua_createtable (L, 0, 5);

	lua_pushinteger (L, SOCK_STREAM);
	lua_setfield (L, 4, "socktype");

	lua_pushinteger (L, 0);
	lua_setfield (L, 4, "protocol");

	lua_pushvalue (L, 2);
	lua_setfield (L, 4, "host");

	/* Check first for IPv6. */
	lua_getfield (L, 1, "aaaa");
	if (!lua_isnil (L, -1))
	{
		lua_pushinteger (L, AF_INET6);
		lua_setfield (L, 4, "family");

		lua_rawgeti (L, -1, 1);
		lua_remove (L, -2);
		if (!lua_isnil (L, -1))
		{
			struct in6_addr *iaddr = (struct in6_addr *) lua_topointer (L, -1);

			struct sockaddr_in6 *addr = (struct sockaddr_in6 *) lua_newuserdata (L, sizeof (struct sockaddr_in6));
			memset (addr, 0, sizeof (struct sockaddr_in6));
			addr->sin6_family = AF_INET6;
			addr->sin6_port = htons (port);
			memcpy (&addr->sin6_addr, iaddr, sizeof (struct in6_addr));

			lua_setfield (L, 4, "addr");
			lua_pop (L, 1);

			return 1;
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	/* Check second for IPv4. */
	lua_getfield (L, 1, "a");
	if (!lua_isnil (L, -1))
	{
		lua_pushinteger (L, AF_INET);
		lua_setfield (L, 4, "family");

		lua_rawgeti (L, -1, 1);
		lua_remove (L, -2);
		if (!lua_isnil (L, -1))
		{
			struct in_addr *iaddr = (struct in_addr *) lua_topointer (L, -1);

			struct sockaddr_in *addr = (struct sockaddr_in *) lua_newuserdata (L, sizeof (struct sockaddr_in));
			memset (addr, 0, sizeof (struct sockaddr_in));
			addr->sin_family = AF_INET;
			addr->sin_port = htons (port);
			memcpy (&addr->sin_addr, iaddr, sizeof (struct in_addr));

			lua_setfield (L, 4, "addr");
			lua_pop (L, 1);

			return 1;
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ build_udp_info() */
static int build_udp_info (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TTABLE);
	luaL_checkstring (L, 2);
	int port = luaL_checkint (L, 3);
	lua_settop (L, 3);

	lua_createtable (L, 0, 5);

	lua_pushinteger (L, SOCK_DGRAM);
	lua_setfield (L, 4, "socktype");

	lua_pushinteger (L, 0);
	lua_setfield (L, 4, "protocol");

	lua_pushvalue (L, 2);
	lua_setfield (L, 4, "host");

	/* Check first for IPv6. */
	lua_getfield (L, 1, "aaaa");
	if (!lua_isnil (L, -1))
	{
		lua_pushinteger (L, AF_INET6);
		lua_setfield (L, 4, "family");

		lua_rawgeti (L, -1, 1);
		if (!lua_isnil (L, -1))
		{
			struct in6_addr *iaddr = (struct in6_addr *) lua_topointer (L, -1);

			struct sockaddr_in6 *addr = (struct sockaddr_in6 *) lua_newuserdata (L, sizeof (struct sockaddr_in6));
			memset (addr, 0, sizeof (struct sockaddr_in6));
			addr->sin6_family = AF_INET6;
			addr->sin6_port = htons (port);
			memcpy (&addr->sin6_addr, iaddr, sizeof (struct in6_addr));

			lua_setfield (L, 4, "addr");
			lua_pop (L, 1);

			return 1;
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	/* Check second for IPv4. */
	lua_getfield (L, 1, "a");
	if (!lua_isnil (L, -1))
	{
		lua_pushinteger (L, AF_INET);
		lua_setfield (L, 4, "family");

		lua_rawgeti (L, -1, 1);
		if (!lua_isnil (L, -1))
		{
			struct in_addr *iaddr = (struct in_addr *) lua_topointer (L, -1);

			struct sockaddr_in *addr = (struct sockaddr_in *) lua_newuserdata (L, sizeof (struct sockaddr_in));
			memset (addr, 0, sizeof (struct sockaddr_in));
			addr->sin_family = AF_INET;
			addr->sin_port = htons (port);
			memcpy (&addr->sin_addr, iaddr, sizeof (struct in_addr));

			lua_setfield (L, 4, "addr");
			lua_pop (L, 1);

			return 1;
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	return 0;
}
/* }}} */

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
		return ratchet_error_errno (L, "ratchet.socket.new()", "socket");

	if (set_nonblocking (*fd) < 0)
		return ratchet_error_errno (L, "ratchet.socket.new()", NULL);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setuservalue (L, -2);

	return 1;
}
/* }}} */

/* {{{ rsock_new_pair() */
static int rsock_new_pair (lua_State *L)
{
	int family = luaL_optint (L, 1, AF_UNIX);
	int socktype = luaL_optint (L, 2, SOCK_STREAM);
	int protocol = luaL_optint (L, 3, 0);

	int *fd1 = (int *) lua_newuserdata (L, sizeof (int));
	int *fd2 = (int *) lua_newuserdata (L, sizeof (int));

	int sv[2];
	int ret = socketpair (family, socktype, protocol, sv);
	if (ret < 0)
		return ratchet_error_errno (L, "ratchet.socket.new_pair()", "socketpair");
	*fd1 = sv[0];
	*fd2 = sv[1];

	if (set_nonblocking (*fd1) < 0)
		return ratchet_error_errno (L, "ratchet.socket.new_pair()", NULL);
	if (set_nonblocking (*fd2) < 0)
		return ratchet_error_errno (L, "ratchet.socket.new_pair()", NULL);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -3);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setuservalue (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setuservalue (L, -3);

	return 2;
}
/* }}} */

/* {{{ rsock_from_fd() */
static int rsock_from_fd (lua_State *L)
{
	int *fd = (int *) lua_newuserdata (L, sizeof (int));
	*fd = luaL_checkint (L, 1);
	if (*fd < 0)
		return ratchet_error_str (L, "ratchet.socket.from_fd()", "EBADF", "Invalid file descriptor.");

	if (set_nonblocking (*fd) < 0)
		return ratchet_error_errno (L, "ratchet.socket.from_fd()", NULL);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setuservalue (L, -2);

	return 1;
}
/* }}} */

/* {{{ rsock_prepare_unix() */
static int rsock_prepare_unix (lua_State *L)
{
	const char *path = luaL_checkstring (L, 1);
	lua_settop (L, 1);

	lua_createtable (L, 0, 4);

	lua_pushinteger (L, AF_UNIX);
	lua_setfield (L, 2, "family");

	lua_pushinteger (L, SOCK_STREAM);
	lua_setfield (L, 2, "socktype");

	lua_pushinteger (L, 0);
	lua_setfield (L, 2, "protocol");

	struct sockaddr_un *addr = (struct sockaddr_un *) lua_newuserdata (L, sizeof (struct sockaddr_un));
	addr->sun_family = AF_UNIX;
	strncpy (addr->sun_path, path, UNIX_PATH_MAX-1);
	addr->sun_path[UNIX_PATH_MAX-1] = '\0';
	lua_setfield (L, 2, "addr");

	return 1;
}
/* }}} */

/* {{{ rsock_prepare_tcp() */
static int rsock_prepare_tcp (lua_State *L)
{
	int ctx = 0;
	lua_getctx (L, &ctx);

	if (ctx == 0)
	{
		lua_settop (L, 3);
		push_query_types_table (L, 3);
		lua_replace (L, 3);

		lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_dns_class");
		lua_getfield (L, -1, "query_all");
		lua_remove (L, -2);

		lua_pushvalue (L, 1);
		lua_pushvalue (L, 3);
		lua_callk (L, 2, 2, 1, rsock_prepare_tcp);
	}

	if (lua_toboolean (L, 4))
	{
		lua_settop (L, 4);

		lua_pushcfunction (L, build_tcp_info);
		lua_pushvalue (L, 4);
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_call (L, 3, 1);
		return 1;
	}
	else
		return 2;
}
/* }}} */

/* {{{ rsock_prepare_udp() */
static int rsock_prepare_udp (lua_State *L)
{
	int ctx = 0;
	lua_getctx (L, &ctx);

	if (ctx == 0)
	{
		lua_settop (L, 3);
		push_query_types_table (L, 3);
		lua_replace (L, 3);

		lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_dns_class");
		lua_getfield (L, -1, "query_all");
		lua_remove (L, -2);

		lua_pushvalue (L, 1);
		lua_pushvalue (L, 3);
		lua_callk (L, 2, 2, 1, rsock_prepare_udp);
	}

	if (lua_toboolean (L, 4))
	{
		lua_settop (L, 4);

		lua_pushcfunction (L, build_udp_info);
		lua_pushvalue (L, 4);
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_call (L, 3, 1);
		return 1;
	}
	else
		return 2;
}
/* }}} */

/* {{{ rsock_ntoh() */
static int rsock_ntoh (lua_State *L)
{
	size_t str_len = 0;
	const char *str = luaL_checklstring (L, 1, &str_len);
	int nargs = lua_gettop (L);

	if (str_len != sizeof (uint32_t))
		lua_pushnil (L);
	else
	{
		uint32_t in = 0, out = 0;
		memcpy (&in, str, sizeof (uint32_t));
		out = ntohl (in);

		lua_pushinteger (L, (lua_Number) out);
	}

	lua_replace (L, 1);

	return nargs;
}
/* }}} */

/* {{{ rsock_hton() */
static int rsock_hton (lua_State *L)
{
	char out_str[4];
	int nargs = lua_gettop (L);

	uint32_t out, in = (uint32_t) luaL_checkinteger (L, 1);
	out = htonl (in);
	memcpy (out_str, &out, sizeof (uint32_t));
	lua_pushlstring (L, out_str, 4);
	lua_replace (L, 1);

	return nargs;
}
/* }}} */

/* {{{ rsock_ntoh16() */
static int rsock_ntoh16 (lua_State *L)
{
	size_t str_len = 0;
	const char *str = luaL_checklstring (L, 1, &str_len);
	int nargs = lua_gettop (L);

	if (str_len != sizeof (uint16_t))
		lua_pushnil (L);
	else
	{
		uint16_t in = 0, out = 0;
		memcpy (&in, str, sizeof (uint16_t));
		out = ntohs (in);

		lua_pushinteger (L, (lua_Number) out);
	}

	lua_replace (L, 1);

	return nargs;
}
/* }}} */

/* {{{ rsock_hton16() */
static int rsock_hton16 (lua_State *L)
{
	char out_str[4];
	int nargs = lua_gettop (L);

	uint16_t out, in = (uint16_t) luaL_checkinteger (L, 1);
	out = htons (in);
	memcpy (out_str, &out, sizeof (uint16_t));
	lua_pushlstring (L, out_str, 2);
	lua_replace (L, 1);

	return nargs;
}
/* }}} */

/* {{{ rsock_gethostname() */
static int rsock_gethostname (lua_State *L)
{
	char buffer[256];
	int ret;

	ret = gethostname (buffer, 256);
	if (ret == -1)
		return ratchet_error_errno (L, "ratchet.socket.gethostname()", "gethostname");

	lua_pushstring (L, buffer);
	return 1;
}
/* }}} */

/* {{{ rsock_multi_recv() */
static int rsock_multi_recv (lua_State *L)
{
	int ctx = 0;
	lua_getctx (L, &ctx);

	if (ctx == 0)
	{
		lua_settop (L, 3);

		lua_pushlightuserdata (L, RATCHET_YIELD_MULTIRW);
		lua_pushvalue (L, 1);
		lua_pushnil (L);
		lua_pushvalue (L, 2);
		return lua_yieldk (L, 4, 1, rsock_multi_recv);
	}

	else if (ctx == 1)
	{
		if (!lua_toboolean (L, 4))
		{
			lua_pushnil (L);
			return 1;
		}
		
		lua_settop (L, 4);

		lua_getfield (L, 4, "recv");
		lua_pushvalue (L, 4);
		lua_pushvalue (L, 3);
		lua_callk (L, 2, 2, 2, rsock_multi_recv);
	}

	if (!lua_toboolean (L, 5))
		return 2;

	lua_pushvalue (L, 5);
	lua_pushvalue (L, 4);
	return 2;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rsock_gc() */
static int rsock_gc (lua_State *L)
{
	int *fd = &socket_fd (L, 1);
	if (*fd >= 0)
		close (*fd);
	*fd = -1;

	return 0;
}
/* }}} */

/* {{{ rsock_eq() */
static int rsock_eq (lua_State *L)
{
	int fd1 = socket_fd (L, 1);
	int fd2 = socket_fd (L, 2);

	lua_pushboolean (L, (fd1 == fd2));
	return 1;
}
/* }}} */

/* {{{ rsock_get_fd() */
static int rsock_get_fd (lua_State *L)
{
	int fd = socket_fd (L, 1);
	lua_pushinteger (L, fd);
	return 1;
}
/* }}} */

/* {{{ rsock_get_timeout() */
static int rsock_get_timeout (lua_State *L)
{
	(void) socket_fd (L, 1);

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "timeout");
	return 1;
}
/* }}} */

/* {{{ rsock_set_timeout() */
static int rsock_set_timeout (lua_State *L)
{
	(void) socket_fd (L, 1);
	luaL_checknumber (L, 2);

	lua_getuservalue (L, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "timeout");

	return 0;
}
/* }}} */

/* {{{ rsock_check_errors() */
static int rsock_check_errors (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	int error = 0;
	socklen_t errorlen = sizeof (int);

	if (getsockopt (sockfd, SOL_SOCKET, SO_ERROR, (void *) &error, &errorlen) < 0)
	{
		if (errno == EBADF)
			error = errno;
		else
			return ratchet_error_errno (L, "ratchet.socket.check_errors()", "getsockopt");
	}

	if (error)
	{
		errno = error;
		return ratchet_error_errno (L, "ratchet.socket.check_errors()", NULL);
	}
	else
		lua_pushboolean (L, 1);

	return 1;
}
/* }}} */

/* {{{ rsock_bind() */
static int rsock_bind (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	luaL_checktype (L, 2, LUA_TUSERDATA);
	struct sockaddr *addr = (struct sockaddr *) lua_touserdata (L, 2);
	socklen_t addrlen = (socklen_t) lua_rawlen (L, 2);

	if (addr->sa_family == AF_UNIX)
		unlink (((struct sockaddr_un *) addr)->sun_path);

	int ret = bind (sockfd, addr, addrlen);
	if (ret < 0)
		return ratchet_error_errno (L, "ratchet.socket.bind()", "bind");

	lua_pushvalue (L, 2);
	call_tracer (L, 1, "bind", 1);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rsock_listen() */
static int rsock_listen (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	int backlog = luaL_optint (L, 2, SOMAXCONN);

	int ret = listen (sockfd, backlog);
	if (ret < 0)
		return ratchet_error_errno (L, "ratchet.socket.listen()", "listen");

	call_tracer (L, 1, "listen", 0);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rsock_shutdown() */
static int rsock_shutdown (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	static const char *lst[] = {"read", "write", "both", NULL};
	static const int howlst[] = {SHUT_RD, SHUT_WR, SHUT_RDWR};
	int how = howlst[luaL_checkoption (L, 2, "both", lst)];

	int ret = shutdown (sockfd, how);
	if (ret == -1)
		return ratchet_error_errno (L, "ratchet.socket.shutdown()", "shutdown");

	lua_pushvalue (L, 2);
	call_tracer (L, 1, "shutdown", 1);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rsock_close() */
static int rsock_close (lua_State *L)
{
	int *fd = &socket_fd (L, 1);
	if (*fd < 0)
		return 0;

	int ret = close (*fd);
	if (ret == -1)
		return ratchet_error_errno (L, "ratchet.socket.close()", "close");
	*fd = -1;

	call_tracer (L, 1, "close", 0);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rsock_set_tracer() */
static int rsock_set_tracer (lua_State *L)
{
	(void) socket_fd (L, 1);
	lua_settop (L, 2);

	lua_getuservalue (L, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "tracer");

	return 0;
}
/* }}} */

/* {{{ rsock_connect() */
static int rsock_connect (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	luaL_checktype (L, 2, LUA_TUSERDATA);
	struct sockaddr *addr = (struct sockaddr *) lua_touserdata (L, 2);
	socklen_t addrlen = (socklen_t) lua_rawlen (L, 2);

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.socket.connect()", "ETIMEDOUT", "Timed out on connect.");
	lua_settop (L, 2);

	int ret = connect (sockfd, addr, addrlen);
	if (ret < 0)
	{
		if (errno == EALREADY || errno == EINPROGRESS)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rsock_connect);
		}
		else
			return ratchet_error_errno (L, "ratchet.socket.connect()", "connect");
	}

	throw_fd_errors (L, sockfd);

	lua_pushvalue (L, 2);
	call_tracer (L, 1, "connect", 1);

	return 0;
}
/* }}} */

/* {{{ rsock_accept() */
static int rsock_accept (lua_State *L)
{
	int sockfd = socket_fd (L, 1);

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 2))
		return ratchet_error_str (L, "ratchet.socket.accept()", "ETIMEDOUT", "Timed out on accept.");
	lua_settop (L, 1);

	socklen_t addr_len = sizeof (struct sockaddr_storage);
	struct sockaddr_storage addr;

	int clientfd = accept (sockfd, (struct sockaddr *) &addr, &addr_len);
	if (clientfd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rsock_accept);
		}

		else
			return ratchet_error_errno (L, "ratchet.socket.accept()", "accept");
	}

	/* Create the new socket object from file descriptor. */
	lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_socket_class");
	lua_getfield (L, -1, "from_fd");
	lua_pushinteger (L, clientfd);
	lua_call (L, 1, 1);
	lua_remove (L, -2);

	push_inet_ntop (L, (struct sockaddr *) &addr);

	lua_pushvalue (L, -1);
	call_tracer (L, 1, "accept", 1);

	return 2;
}
/* }}} */

/* {{{ rsock_send() */
static int rsock_send (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	size_t data_len, remaining;
	const char *data = luaL_checklstring (L, 2, &data_len);
	ssize_t ret;

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.socket.send()", "ETIMEDOUT", "Timed out on send.");
	lua_settop (L, 2);

	int flags = MSG_NOSIGNAL;
	ret = send (sockfd, data, data_len, flags);
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rsock_send);
		}
		else
			return ratchet_error_errno (L, "ratchet.socket.send()", "send");
	}

	if ((size_t) ret < data_len)
	{
		lua_pushlstring (L, data, ret);
		call_tracer (L, 1, "send", 1);

		remaining = data_len - (size_t) ret;
		lua_pushlstring (L, data+ret, remaining);
		return 1;
	}
	else
	{
		lua_pushvalue (L, 2);
		call_tracer (L, 1, "send", 1);

		return 0;
	}
}
/* }}} */

/* {{{ rsock_recv() */
static int rsock_recv (lua_State *L)
{
	int sockfd = socket_fd (L, 1);
	luaL_Buffer buffer;
	ssize_t ret;

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.socket.recv()", "ETIMEDOUT", "Timed out on recv.");
	lua_settop (L, 2);

	luaL_buffinit (L, &buffer);
	char *prepped = luaL_prepbuffer (&buffer);

	size_t len = (size_t) luaL_optunsigned (L, 2, (lua_Unsigned) LUAL_BUFFERSIZE);
	if (len > LUAL_BUFFERSIZE)
		return luaL_error (L, "Cannot recv more than %u bytes, %u requested", (unsigned) LUAL_BUFFERSIZE, (unsigned) len);

	ret = recv (sockfd, prepped, len, 0);
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rsock_recv);
		}
		else
			return ratchet_error_errno (L, "ratchet.socket.recv()", "recv");
	}

	luaL_addsize (&buffer, (size_t) ret);
	luaL_pushresult (&buffer);

	lua_pushvalue (L, -1);
	call_tracer (L, 1, "recv", 1);

	return 1;
}
/* }}} */

#if HAVE_OPENSSL
/* {{{ rsock_try_encrypted_send() */
static int rsock_try_encrypted_send (lua_State *L)
{
	(void) socket_fd (L, 1);
	int ctx = 0;
	if (LUA_YIELD == lua_getctx (L, &ctx))
		return 2;

	lua_settop (L, 2);

	lua_getfield (L, 1, "get_encryption");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);

	if (lua_toboolean (L, -1))
	{
		lua_getfield (L, -1, "write");
		lua_pushvalue (L, -2);
		lua_pushvalue (L, 2);
		lua_callk (L, 2, 2, 1, rsock_try_encrypted_send);
		return 2;
	}
	else
	{
		lua_settop (L, 2);
		return rsock_send (L);
	}
}
/* }}} */

/* {{{ rsock_try_encrypted_recv() */
static int rsock_try_encrypted_recv (lua_State *L)
{
	(void) socket_fd (L, 1);
	int ctx = 0;
	if (LUA_YIELD == lua_getctx (L, &ctx))
		return 2;

	lua_settop (L, 2);

	lua_getfield (L, 1, "get_encryption");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);

	if (lua_toboolean (L, -1))
	{
		lua_getfield (L, -1, "read");
		lua_pushvalue (L, -2);
		lua_pushvalue (L, 2);
		lua_callk (L, 2, 2, 1, rsock_try_encrypted_recv);
		return 2;
	}
	else
	{
		lua_settop (L, 2);
		return rsock_recv (L);
	}
}
/* }}} */
#endif

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_socket() */
int luaopen_ratchet_socket (lua_State *L)
{
	/* Static functions in the ratchet.socket namespace. */
	static const luaL_Reg funcs[] = {
		/* Documented methods. */
		{"new", rsock_new},
		{"new_pair", rsock_new_pair},
		{"from_fd", rsock_from_fd},
		{"ntoh", rsock_ntoh},
		{"hton", rsock_hton},
		{"ntoh16", rsock_ntoh16},
		{"hton16", rsock_hton16},
		{"gethostname", rsock_gethostname},
		{"multi_recv", rsock_multi_recv},
		{"prepare_unix", rsock_prepare_unix},
		{"prepare_tcp", rsock_prepare_tcp},
		{"prepare_udp", rsock_prepare_udp},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Meta-methods for ratchet.socket object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rsock_gc},
		{"__eq", rsock_eq},
		{NULL}
	};

	/* Methods in the ratchet.socket class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_fd", rsock_get_fd},
		{"get_timeout", rsock_get_timeout},
		{"set_timeout", rsock_set_timeout},
#if HAVE_OPENSSL
		{"get_encryption", rsock_get_encryption},
		{"encrypt", rsock_encrypt},
		{"send", rsock_try_encrypted_send},
		{"recv", rsock_try_encrypted_recv},
#else
		{"send", rsock_send},
		{"recv", rsock_recv},
#endif
		{"bind", rsock_bind},
		{"listen", rsock_listen},
		{"check_errors", rsock_check_errors},
		{"connect", rsock_connect},
		{"accept", rsock_accept},
		{"shutdown", rsock_shutdown},
		{"close", rsock_close},
		{"set_tracer", rsock_set_tracer},
		{"getsockopt", rsockopt_get},
		{"setsockopt", rsockopt_set},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Set up the ratchet.socket namespace functions. */
	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_socket_class");

	/* Set up the ratchet.socket class and metatables. */
	luaL_newmetatable (L, "ratchet_socket_meta");
	luaL_setfuncs (L, metameths, 0);
	luaL_newlib (L, meths);
	lua_setfield (L, -2, "__index");
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
