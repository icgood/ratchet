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
#include <string.h>
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#include <sys/time.h>

#include "ratchet.h"
#include "misc.h"

#define CHECK_OPT_GET(opt, type, ...) if (0 == strcmp (#opt, key)) return rsockopt_get_##type (L, fd, opt, ##__VA_ARGS__)
#define CHECK_OPT_SET(opt, type) if (0 == strcmp (#opt, key)) return rsockopt_set_##type (L, fd, opt, 3)

int rsockopt_get (lua_State *L);
int rsockopt_set (lua_State *L);

/* {{{ rsockopt_get_boolean() */
static int rsockopt_get_boolean (lua_State *L, int fd, int opt)
{
	int val;
	socklen_t val_len = sizeof (val);
	int ret = getsockopt (fd, SOL_SOCKET, opt, &val, &val_len);
	if (ret == 0)
	{
		lua_pushboolean (L, val);
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */

/* {{{ rsockopt_get_int() */
static int rsockopt_get_int (lua_State *L, int fd, int opt)
{
	int val;
	socklen_t val_len = sizeof (val);
	int ret = getsockopt (fd, SOL_SOCKET, opt, &val, &val_len);
	if (ret == 0)
	{
		lua_pushinteger (L, val);
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */

/* {{{ rsockopt_get_string() */
static int rsockopt_get_string (lua_State *L, int fd, int opt, int maxlen)
{
	char val[maxlen+1];
	socklen_t val_len = maxlen+1;
	int ret = getsockopt (fd, SOL_SOCKET, opt, val, &val_len);
	if (ret == 0)
	{
		lua_pushlstring (L, val, (size_t) val_len);
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */

/* {{{ rsockopt_get_linger() */
static int rsockopt_get_linger (lua_State *L, int fd, int opt)
{
	struct linger val;
	socklen_t val_len = sizeof (val);
	int ret = getsockopt (fd, SOL_SOCKET, opt, &val, &val_len);
	if (ret == 0)
	{
		lua_createtable (L, 0, 2);
		lua_pushboolean (L, val.l_onoff);
		lua_setfield (L, -2, "l_onoff");
		lua_pushinteger (L, val.l_linger);
		lua_setfield (L, -2, "l_linger");
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */

#ifdef _GNU_SOURCE
/* {{{ rsockopt_get_peercred() */
static int rsockopt_get_peercred (lua_State *L, int fd, int opt)
{
	struct ucred val;
	socklen_t val_len = sizeof (val);
	int ret = getsockopt (fd, SOL_SOCKET, opt, &val, &val_len);
	if (ret == 0)
	{
		lua_createtable (L, 0, 3);
		lua_pushinteger (L, (int) val.pid);
		lua_setfield (L, -2, "pid");
		lua_pushinteger (L, (int) val.uid);
		lua_setfield (L, -2, "uid");
		lua_pushinteger (L, (int) val.gid);
		lua_setfield (L, -2, "gid");
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */
#endif

/* {{{ rsockopt_get_timeval() */
static int rsockopt_get_timeval (lua_State *L, int fd, int opt)
{
	struct timeval val;
	socklen_t val_len = sizeof (val);
	int ret = getsockopt (fd, SOL_SOCKET, opt, &val, &val_len);
	if (ret == 0)
	{
		lua_createtable (L, 0, 2);
		lua_pushinteger (L, val.tv_sec);
		lua_setfield (L, -2, "tv_sec");
		lua_pushinteger (L, val.tv_usec);
		lua_setfield (L, -2, "tv_usec");
		return 1;
	}
	else
		return ratchet_error_errno (L, NULL, "getsockopt");
}
/* }}} */

/* {{{ rsockopt_set_boolean() */
static int rsockopt_set_boolean (lua_State *L, int fd, int opt, int valindex)
{
	int val = lua_toboolean (L, valindex);
	int ret = setsockopt (fd, SOL_SOCKET, opt, &val, sizeof (val));
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */

/* {{{ rsockopt_set_int() */
static int rsockopt_set_int (lua_State *L, int fd, int opt, int valindex)
{
	int val = luaL_checkint (L, valindex);
	int ret = setsockopt (fd, SOL_SOCKET, opt, &val, sizeof (val));
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */

/* {{{ rsockopt_set_string() */
static int rsockopt_set_string (lua_State *L, int fd, int opt, int valindex)
{
	size_t val_len;
	const char * val = luaL_checklstring (L, valindex, &val_len);
	int ret = setsockopt (fd, SOL_SOCKET, opt, val, (socklen_t) val_len);
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */

/* {{{ rsockopt_set_linger() */
static int rsockopt_set_linger (lua_State *L, int fd, int opt, int valindex)
{
	luaL_checktype (L, valindex, LUA_TTABLE);
	struct linger val;
	lua_getfield (L, valindex, "l_onoff");
	val.l_onoff = lua_toboolean (L, -1);
	lua_pop (L, 1);
	lua_getfield (L, valindex, "l_linger");
	val.l_linger = lua_tointeger (L, -1);
	lua_pop (L, 1);

	int ret = setsockopt (fd, SOL_SOCKET, opt, &val, sizeof (val));
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */

#ifdef _GNU_SOURCE
/* {{{ rsockopt_set_peercred() */
static int rsockopt_set_peercred (lua_State *L, int fd, int opt, int valindex)
{
	luaL_checktype (L, valindex, LUA_TTABLE);
	struct ucred val;
	lua_getfield (L, valindex, "pid");
	val.pid = lua_tointeger (L, -1);
	lua_pop (L, 1);
	lua_getfield (L, valindex, "uid");
	val.uid = lua_tointeger (L, -1);
	lua_pop (L, 1);
	lua_getfield (L, valindex, "gid");
	val.gid = lua_tointeger (L, -1);
	lua_pop (L, 1);

	socklen_t val_len = sizeof (val);
	int ret = setsockopt (fd, SOL_SOCKET, opt, &val, sizeof (val));
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */
#endif

/* {{{ rsockopt_set_timeval() */
static int rsockopt_set_timeval (lua_State *L, int fd, int opt, int valindex)
{
	luaL_checktype (L, valindex, LUA_TTABLE);
	struct timeval val;
	lua_getfield (L, valindex, "tv_sec");
	val.tv_sec = lua_tointeger (L, -1);
	lua_pop (L, 1);
	lua_getfield (L, valindex, "tv_usec");
	val.tv_usec = lua_tointeger (L, -1);
	lua_pop (L, 1);

	int ret = setsockopt (fd, SOL_SOCKET, opt, &val, sizeof (val));
	if (ret == 0)
		return 0;
	else
		return ratchet_error_errno (L, NULL, "setsockopt");
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ rsockopt_get() */
int rsockopt_get (lua_State *L)
{
	int fd = (*((int *) luaL_checkudata (L, 1, "ratchet_socket_meta")));
	const char *key = luaL_checkstring (L, 2);

	CHECK_OPT_GET (SO_ACCEPTCONN, boolean);
#ifdef IFNAMSIZ
	CHECK_OPT_GET (SO_BINDTODEVICE, string, IFNAMSIZ);
#endif
	CHECK_OPT_GET (SO_BROADCAST, int);
	CHECK_OPT_GET (SO_BSDCOMPAT, boolean);
	CHECK_OPT_GET (SO_DEBUG, boolean);
#ifdef SO_DOMAIN
	CHECK_OPT_GET (SO_DOMAIN, int);
#endif
	CHECK_OPT_GET (SO_ERROR, int);
	CHECK_OPT_GET (SO_DONTROUTE, boolean);
	CHECK_OPT_GET (SO_KEEPALIVE, boolean);
	CHECK_OPT_GET (SO_LINGER, linger);
	CHECK_OPT_GET (SO_OOBINLINE, boolean);
	CHECK_OPT_GET (SO_PASSCRED, boolean);
#ifdef _GNU_SOURCE
	CHECK_OPT_GET (SO_PEERCRED, peercred);
#endif
	CHECK_OPT_GET (SO_PRIORITY, int);
#ifdef SO_PROTOCOL
	CHECK_OPT_GET (SO_PROTOCOL, int);
#endif
	CHECK_OPT_GET (SO_RCVBUF, int);
#ifdef SO_RCVBUFFORCE
	CHECK_OPT_GET (SO_RCVBUFFORCE, int);
#endif
	CHECK_OPT_GET (SO_RCVLOWAT, int);
	CHECK_OPT_GET (SO_SNDLOWAT, int);
	CHECK_OPT_GET (SO_RCVTIMEO, timeval);
	CHECK_OPT_GET (SO_SNDTIMEO, timeval);
	CHECK_OPT_GET (SO_REUSEADDR, boolean);
	CHECK_OPT_GET (SO_SNDBUF, int);
#ifdef SO_SNDBUFFORCE
	CHECK_OPT_GET (SO_SNDBUFFORCE, int);
#endif
	CHECK_OPT_GET (SO_TIMESTAMP, boolean);
	CHECK_OPT_GET (SO_TYPE, int);

	lua_pushnil (L);
	return 1;
}
/* }}} */

/* {{{ rsockopt_set() */
int rsockopt_set (lua_State *L)
{
	int fd = (*((int *) luaL_checkudata (L, 1, "ratchet_socket_meta")));
	const char *key = luaL_checkstring (L, 2);

	CHECK_OPT_SET (SO_ACCEPTCONN, boolean);
#ifdef IFNAMSIZ
	CHECK_OPT_SET (SO_BINDTODEVICE, string);
#endif
	CHECK_OPT_SET (SO_BROADCAST, int);
	CHECK_OPT_SET (SO_BSDCOMPAT, boolean);
	CHECK_OPT_SET (SO_DEBUG, boolean);
#ifdef SO_DOMAIN
	CHECK_OPT_SET (SO_DOMAIN, int);
#endif
	CHECK_OPT_SET (SO_ERROR, int);
	CHECK_OPT_SET (SO_DONTROUTE, boolean);
	CHECK_OPT_SET (SO_KEEPALIVE, boolean);
	CHECK_OPT_SET (SO_LINGER, linger);
	CHECK_OPT_SET (SO_OOBINLINE, boolean);
	CHECK_OPT_SET (SO_PASSCRED, boolean);
#ifdef _GNU_SOURCE
	CHECK_OPT_SET (SO_PEERCRED, peercred);
#endif
	CHECK_OPT_SET (SO_PRIORITY, int);
#ifdef SO_PROTOCOL
	CHECK_OPT_SET (SO_PROTOCOL, int);
#endif
	CHECK_OPT_SET (SO_RCVBUF, int);
#ifdef SO_RCVBUFFORCE
	CHECK_OPT_SET (SO_RCVBUFFORCE, int);
#endif
	CHECK_OPT_SET (SO_RCVLOWAT, int);
	CHECK_OPT_SET (SO_SNDLOWAT, int);
	CHECK_OPT_SET (SO_RCVTIMEO, timeval);
	CHECK_OPT_SET (SO_SNDTIMEO, timeval);
	CHECK_OPT_SET (SO_REUSEADDR, boolean);
	CHECK_OPT_SET (SO_SNDBUF, int);
#ifdef SO_SNDBUFFORCE
	CHECK_OPT_SET (SO_SNDBUFFORCE, int);
#endif
	CHECK_OPT_SET (SO_TIMESTAMP, boolean);
	CHECK_OPT_SET (SO_TYPE, int);

	return -1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
