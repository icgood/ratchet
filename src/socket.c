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

#include "luaopens.h"
#include "misc.h"
#include "sockopt.h"

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

/* {{{ push_inet_ntop() */
static int push_inet_ntop (lua_State *L, struct sockaddr *addr)
{
	if (addr->sa_family == AF_INET)
	{
		char buffer[INET_ADDRSTRLEN];
		struct in_addr *in = &((struct sockaddr_in *) addr)->sin_addr;
		if (!inet_ntop (AF_INET, in, buffer, INET_ADDRSTRLEN))
			return handle_perror (L);
		lua_pushstring (L, buffer);
	}

	else if (addr->sa_family == AF_INET6)
	{
		char buffer[INET6_ADDRSTRLEN];
		struct in6_addr *in = &((struct sockaddr_in6 *) addr)->sin6_addr;
		if (!inet_ntop (AF_INET6, in, buffer, INET6_ADDRSTRLEN))
			return handle_perror (L);
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
	lua_getfenv (L, index);
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
		return handle_perror (L);

	if (set_nonblocking (*fd) < 0)
		return handle_perror (L);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setfenv (L, -2);

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
		return handle_perror (L);
	*fd1 = sv[0];
	*fd2 = sv[1];

	if (set_nonblocking (*fd1) < 0)
		return handle_perror (L);
	if (set_nonblocking (*fd2) < 0)
		return handle_perror (L);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -3);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setfenv (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setfenv (L, -3);

	return 2;
}
/* }}} */

/* {{{ rsock_from_fd() */
static int rsock_from_fd (lua_State *L)
{
	int *fd = (int *) lua_newuserdata (L, sizeof (int));
	*fd = luaL_checkint (L, 1);
	if (*fd < 0)
		return handle_perror (L);

	if (set_nonblocking (*fd) < 0)
		return handle_perror (L);

	luaL_getmetatable (L, "ratchet_socket_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushnumber (L, (lua_Number) -1.0);
	lua_setfield (L, -2, "timeout");
	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* {{{ rsock_type_and_info_from_uri() */
static int rsock_type_and_info_from_uri (lua_State *L)
{
	luaL_checkstring (L, 1);
	lua_settop (L, 1);

	/* Check for form: schema://[127.0.0.1]:25
	 * or: schema://[01:02:03:04:05:06:07:08]:25 */
	if (strmatch (L, 1, "^([tu][cd]p)%:%/+%[(.-)%]%:(%d+)$"))
		return 3;

	/* Check for form: schema://127.0.0.1:25 */
	else if (strmatch (L, 1, "^([tu][cd]p)%:%/+([^%:]*)%:(%d+)$"))
		return 3;

	/* Check for form: tcp://127.0.0.1
	 * or: tcp://01:02:03:04:05:06:07:08
	 * Always uses default port. */
	else if (strmatch (L, 1, "^([tu][cd]p)%:%/+(.*)$"))
	{
		lua_pushinteger (L, DEFAULT_TCPUDP_PORT);
		return 3;
	}

	else if (strmatch (L, 1, "^(unix)%:(.*)$"))
		return 2;

	return 0;
}
/* }}} */

/* {{{ rsock_build_tcp_info() */
static int rsock_build_tcp_info (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TTABLE);
	luaL_checkstring (L, 2);
	int port = luaL_checkint (L, 3);
	lua_settop (L, 3);

	lua_createtable (L, 0, 4);

	lua_pushinteger (L, SOCK_STREAM);
	lua_setfield (L, 4, "socktype");

	lua_pushinteger (L, 0);
	lua_setfield (L, 4, "protocol");

	lua_pushvalue (L, 2);
	lua_setfield (L, 4, "host");

	/* Check first for IPv6. */
	lua_getfield (L, 1, "ipv6");
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
	lua_getfield (L, 1, "ipv4");
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

/* {{{ rsock_build_udp_info() */
static int rsock_build_udp_info (lua_State *L)
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
	lua_getfield (L, 1, "ipv6");
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
	lua_getfield (L, 1, "ipv4");
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

/* {{{ rsock_ntoh() */
static int rsock_ntoh (lua_State *L)
{
	size_t str_len = 0;
	const char *str = luaL_checklstring (L, 1, &str_len);

	if (str_len == sizeof (uint32_t))
	{
		uint32_t in = 0, out = 0;
		memcpy (&in, str, sizeof (uint32_t));
		out = ntohl (in);

		lua_pushinteger (L, (lua_Number) out);
	}
	else if (str_len == sizeof (uint16_t))
	{
		uint16_t in = 0, out = 0;
		memcpy (&in, str, sizeof (uint16_t));
		out = ntohs (in);

		lua_pushinteger (L, (lua_Number) out);
	}
	else
		return luaL_argerror (L, 1, "Input can only be 2 or 4 bytes.");

	return 1;
}
/* }}} */

/* {{{ rsock_hton() */
static int rsock_hton (lua_State *L)
{
	char out_str[4];
	int use_16 = lua_toboolean (L, 2);

	if (use_16)
	{
		uint16_t out, in = (uint16_t) luaL_checkinteger (L, 1);
		out = htons (in);
		memcpy (out_str, &out, sizeof (uint16_t));
		lua_pushlstring (L, out_str, 2);
	}
	else
	{
		uint32_t out, in = (uint32_t) luaL_checkinteger (L, 1);
		out = htonl (in);
		memcpy (out_str, &out, sizeof (uint32_t));
		lua_pushlstring (L, out_str, 4);
	}

	return 1;
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

/* {{{ rsock_index() */
static int rsock_index (lua_State *L)
{
	lua_getmetatable (L, 1);
	lua_getfield (L, -1, "methods");
	lua_pushvalue (L, 2);
	lua_gettable (L, -2);
	if (!lua_isnil (L, -1))
		return 1;
	else
		lua_pop (L, 3);

	if (lua_isstring (L, 2))
	{
		const char *key = lua_tostring (L, 2);
		int fd = socket_fd (L, 1);
		return rsockopt_get (L, key, fd);
	}
	else
	{
		lua_pushnil (L);
		return 1;
	}
}
/* }}} */

/* {{{ rsock_newindex() */
static int rsock_newindex (lua_State *L)
{
	if (lua_isstring (L, 2))
	{
		const char *key = lua_tostring (L, 2);
		int fd = socket_fd (L, 1);
		if (rsockopt_set (L, key, fd, 3) == -1)
			return luaL_error (L, "cannot set arbitrary key on socket");
		
	}
	return 0;
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

	lua_getfenv (L, 1);
	lua_getfield (L, -1, "timeout");
	return 1;
}
/* }}} */

/* {{{ rsock_set_timeout() */
static int rsock_set_timeout (lua_State *L)
{
	(void) socket_fd (L, 1);
	luaL_checknumber (L, 2);

	lua_getfenv (L, 1);
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
			return handle_perror (L);
	}

	if (error)
		return handle_perror (L);
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
	socklen_t addrlen = (socklen_t) lua_objlen (L, 2);

	if (addr->sa_family == AF_UNIX)
		unlink (((struct sockaddr_un *) addr)->sun_path);

	int ret = bind (sockfd, addr, addrlen);
	if (ret < 0)
		return handle_perror (L);

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
		return handle_perror (L);

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
		return handle_perror (L);

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
		return handle_perror (L);
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

	lua_getfenv (L, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "tracer");

	return 0;
}
/* }}} */

/* {{{ rsock_rawconnect() */
static int rsock_rawconnect (lua_State *L)
{
	lua_settop (L, 2);
	int sockfd = socket_fd (L, 1);
	luaL_checktype (L, 2, LUA_TUSERDATA);
	struct sockaddr *addr = (struct sockaddr *) lua_touserdata (L, 2);
	socklen_t addrlen = (socklen_t) lua_objlen (L, 2);

	int ret = connect (sockfd, addr, addrlen);
	if (ret < 0)
	{
		if (errno == EALREADY || errno == EINPROGRESS || errno == EISCONN)
			lua_pushboolean (L, 0);
		else
			return handle_perror (L);
	}
	else
		lua_pushboolean (L, 1);

	lua_pushvalue (L, 2);
	call_tracer (L, 1, "connect", 1);

	return 1;
}
/* }}} */

/* {{{ rsock_rawaccept() */
static int rsock_rawaccept (lua_State *L)
{
	int sockfd = socket_fd (L, 1);

	socklen_t addr_len = sizeof (struct sockaddr_storage);
	struct sockaddr_storage addr;

	int clientfd = accept (sockfd, (struct sockaddr *) &addr, &addr_len);
	if (clientfd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_getfield (L, 1, "accept");
			lua_pushvalue (L, 1);
			lua_call (L, 1, 2);
			return 2;
		}

		else
			return handle_perror (L);
	}

	/* Create the new socket object from file descriptor. */
	lua_getfield (L, lua_upvalueindex (1), "from_fd");
	lua_pushinteger (L, clientfd);
	lua_call (L, 1, 1);

	push_inet_ntop (L, (struct sockaddr *) &addr);

	lua_pushvalue (L, -1);
	call_tracer (L, 1, "accept", 1);

	return 2;
}
/* }}} */

/* {{{ rsock_rawsend() */
static int rsock_rawsend (lua_State *L)
{
	lua_settop (L, 2);
	int sockfd = socket_fd (L, 1);
	size_t data_len;
	const char *data = luaL_checklstring (L, 2, &data_len);
	ssize_t ret;

	int flags = MSG_NOSIGNAL;
	ret = send (sockfd, data, data_len, flags);
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_getfield (L, 1, "send");
			lua_pushvalue (L, 1);
			lua_pushvalue (L, 2);
			lua_call (L, 2, 2);
			return 2;
		}

		else
			return handle_perror (L);
	}

	lua_pushvalue (L, 2);
	call_tracer (L, 1, "send", 1);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rsock_rawrecv() */
static int rsock_rawrecv (lua_State *L)
{
	lua_settop (L, 1);
	int sockfd = socket_fd (L, 1);
	luaL_Buffer buffer;
	ssize_t ret;

	luaL_buffinit (L, &buffer);
	char *prepped = luaL_prepbuffer (&buffer);

	ret = recv (sockfd, prepped, LUAL_BUFFERSIZE, 0);
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_getfield (L, 1, "recv");
			lua_pushvalue (L, 1);
			lua_call (L, 1, 2);
			return 2;
		}

		else
			return handle_perror (L);
	}

	luaL_addsize (&buffer, (size_t) ret);
	luaL_pushresult (&buffer);

	lua_pushvalue (L, -1);
	call_tracer (L, 1, "recv", 1);

	return 1;
}
/* }}} */

/* ---- Lua-implemented Functions ------------------------------------------- */

/* {{{ send() */
#if HAVE_OPENSSL
#define rsock_send "return function (self, ...)\n" \
	"	local enc = self:get_encryption()\n" \
	"	if enc then\n" \
	"		return enc:write(...)\n" \
	"	elseif coroutine.yield('write', self) then\n" \
	"		return self:rawsend(...)\n" \
	"	end\n" \
	"end\n"
#else
#define rsock_send "return function (self, ...)\n" \
	"	if coroutine.yield('write', self) then\n" \
	"		return self:rawsend(...)\n" \
	"	end\n" \
	"end\n"
#endif
/* }}} */

/* {{{ recv() */
#if HAVE_OPENSSL
#define rsock_recv "return function (self, ...)\n" \
	"	local enc = self:get_encryption()\n" \
	"	if enc then\n" \
	"		return enc:read(...)\n" \
	"	elseif coroutine.yield('read', self) then\n" \
	"		return self:rawrecv(...)\n" \
	"	end\n" \
	"end\n"
#else
#define rsock_recv "return function (self, ...)\n" \
	"	if coroutine.yield('read', self) then\n" \
	"		return self:rawrecv(...)\n" \
	"	end\n" \
	"end\n"
#endif
/* }}} */

/* {{{ accept() */
#define rsock_accept "return function (self, ...)\n" \
	"	if coroutine.yield('read', self) then\n" \
	"		return self:rawaccept(...)\n" \
	"	else\n" \
	"		return nil, 'Timed out.'\n" \
	"	end\n" \
	"end\n"
/* }}} */

/* {{{ connect() */
#define rsock_connect "return function (self, ...)\n" \
	"	local ret, err = self:rawconnect(...)\n" \
	"	if err then\n" \
	"		return nil, err\n" \
	"	elseif not ret then\n" \
	"		if not coroutine.yield('write', self) then\n" \
	"			return nil, 'Timed out.'\n" \
	"		end\n" \
	"	end\n" \
	"	return self:check_errors()\n" \
	"end\n"
/* }}} */

/* {{{ prepare_uri() */
#define rsock_prepare_uri "return function (uri, query_types)\n" \
	"	local class = ratchet.socket\n" \
	"	local schema, dest, port = class.type_and_info_from_uri(uri)\n" \
	"	if schema == 'tcp' then\n" \
	"		local dnsrec = ratchet.dns.query_all(dest, query_types)\n" \
	"		return class.build_tcp_info(dnsrec, dest, port)\n" \
	"	elseif schema == 'udp' then\n" \
	"		local dnsrec = ratchet.dns.query_all(dest, query_types)\n" \
	"		return class.build_udp_info(dnsrec, dest, port)\n" \
	"	\n" \
	"	elseif schema == 'unix' then\n" \
	"		return class.prepare_unix(dest)\n" \
	"	else\n" \
	"		error('unrecognized URI string: ' .. uri)\n" \
	"	end\n" \
	"end\n"
/* }}} */

/* {{{ prepare_tcp() */
#define rsock_prepare_tcp "return function (host, port)\n" \
	"	local dnsrec = ratchet.dns.query_all(host)\n" \
	"	return ratchet.socket.build_tcp_info(dnsrec, host, port)\n" \
	"end\n"
/* }}} */

/* {{{ prepare_udp() */
#define rsock_prepare_udp "return function (host, port)\n" \
	"	local dnsrec = ratchet.dns.query_all(host)\n" \
	"	return ratchet.socket.build_udp_info(dnsrec, host, port)\n" \
	"end\n"
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_socket() */
int luaopen_ratchet_socket (lua_State *L)
{
	static const luaL_Reg empty[] = {{NULL}};

	/* Static functions in the ratchet.socket namespace. */
	static const luaL_Reg funcs[] = {
		/* Documented methods. */
		{"new", rsock_new},
		{"new_pair", rsock_new_pair},
		{"from_fd", rsock_from_fd},
		{"ntoh", rsock_ntoh},
		{"hton", rsock_hton},
		/* Undocumented, helper methods. */
		{"type_and_info_from_uri", rsock_type_and_info_from_uri},
		{"build_tcp_info", rsock_build_tcp_info},
		{"build_udp_info", rsock_build_udp_info},
		{"prepare_unix", rsock_prepare_unix},
		{NULL}
	};

	/* Static Lua functions in the ratchet.socket namespace. */
	static const struct luafunc luafuncs[] = {
		/* Documented methods. */
		{"prepare_uri", rsock_prepare_uri},
		{"prepare_tcp", rsock_prepare_tcp},
		{"prepare_udp", rsock_prepare_udp},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Meta-methods for ratchet.socket object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rsock_gc},
		{"__index", rsock_index},
		{"__newindex", rsock_newindex},
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
#endif
		{"bind", rsock_bind},
		{"listen", rsock_listen},
		{"check_errors", rsock_check_errors},
		{"shutdown", rsock_shutdown},
		{"close", rsock_close},
		{"set_tracer", rsock_set_tracer},
		/* Undocumented, helper methods. */
		{"rawconnect", rsock_rawconnect},
		{"rawaccept", rsock_rawaccept},
		{"rawsend", rsock_rawsend},
		{"rawrecv", rsock_rawrecv},
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

	/* Set up the ratchet.socket namespace functions. */
	luaI_openlib (L, "ratchet.socket", empty, 0);
	lua_pushvalue (L, -1);
	luaI_openlib (L, NULL, funcs, 1);
	register_luafuncs (L, -1, luafuncs);

	/* Set up the ratchet.socket class and metatables. */
	luaL_newmetatable (L, "ratchet_socket_meta");
	lua_newtable (L);
	lua_pushvalue (L, -3);
	luaI_openlib (L, NULL, meths, 1);
	register_luafuncs (L, -1, luameths);
	lua_setfield (L, -2, "methods");
	luaI_openlib (L, NULL, metameths, 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
