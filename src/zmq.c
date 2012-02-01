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

#include <math.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <zmq.h>

#include "ratchet.h"
#include "misc.h"
#include "yield-types.h"

#ifndef RATCHET_ZMQ_IO_THREADS
#define RATCHET_ZMQ_IO_THREADS 10
#endif

#define socket_ptr(L, i) (((struct socket_data *) luaL_checkudata (L, i, "ratchet_zmqsocket_meta"))->socket)
#define socket_timeout(L, i) (((struct socket_data *) luaL_checkudata (L, i, "ratchet_zmqsocket_meta"))->timeout)

#define raise_zmq_error(L, f) raise_zmq_error_ln (L, f, __FILE__, __LINE__)

struct socket_data
{
	void *socket;
	double timeout;
};

/* {{{ raise_zmq_error_ln() */
static int raise_zmq_error_ln (lua_State *L, const char *func, const char *file, int line)
{
	lua_settop (L, 0);
	int e = zmq_errno ();

	ratchet_error_push_constructor (L);
	lua_pushstring (L, zmq_strerror (e));
	ratchet_error_push_code (L, e);
	if (func)
		lua_pushstring (L, func);
	else
		lua_pushnil (L);
	lua_pushstring (L, file);
	lua_pushinteger (L, line);
	lua_pushnil (L);
	lua_pushinteger (L, e);
	lua_call (L, 7, 1);

	return lua_error (L);
}
/* }}} */

/* {{{ gc_zmq_context() */
static int gc_zmq_context (lua_State *L)
{
	void **ctx = (void **) lua_touserdata (L, 1);
	zmq_term (*ctx);

	return 0;
}
/* }}} */

/* {{{ push_new_zmq_context() */
static int push_new_zmq_context (lua_State *L)
{
	void *ctx = zmq_init (RATCHET_ZMQ_IO_THREADS);
	if (ctx)
	{
		void **ptr = (void **) lua_newuserdata (L, sizeof (void *));
		*ptr = ctx;
		lua_createtable (L, 0, 1);
		lua_pushcfunction (L, gc_zmq_context);
		lua_setfield (L, -2, "__gc");
		lua_setmetatable (L, -2);
		return 1;
	}
	else
		return raise_zmq_error (L, NULL);
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rzmq_new() */
static int rzmq_new (lua_State *L)
{
#ifdef ZMQ_UPSTREAM
	static const char *lst[] = {"PAIR", "PUB", "SUB", "REQ", "REP", "XREQ", "XREP", "PULL", "PUSH", "UPSTREAM", "DOWNSTREAM", NULL};
	static const int typelst[] = {ZMQ_PAIR, ZMQ_PUB, ZMQ_SUB, ZMQ_REQ, ZMQ_REP, ZMQ_XREQ, ZMQ_XREP, ZMQ_PULL, ZMQ_PUSH, ZMQ_UPSTREAM, ZMQ_DOWNSTREAM};
#else
	static const char *lst[] = {"PAIR", "PUB", "SUB", "REQ", "REP", "XREQ", "XREP", "PULL", "PUSH", NULL};
	static const int typelst[] = {ZMQ_PAIR, ZMQ_PUB, ZMQ_SUB, ZMQ_REQ, ZMQ_REP, ZMQ_XREQ, ZMQ_XREP, ZMQ_PULL, ZMQ_PUSH};
#endif

	void *context = *(void **) lua_touserdata (L, lua_upvalueindex (1));
	int type = typelst[luaL_checkoption (L, 1, "PAIR", lst)];

	void *socket = zmq_socket (context, type);
	if (socket)
	{
		struct socket_data *sd = (struct socket_data *) lua_newuserdata (L, sizeof (struct socket_data));
		sd->socket = socket;
		sd->timeout = -1.0;

		luaL_getmetatable (L, "ratchet_zmqsocket_meta");
		lua_setmetatable (L, -2);

		return 1;
	}
	else
		return raise_zmq_error (L, "ratchet.zmqsocket.new()");
}
/* }}} */

/* {{{ rzmq_prepare_uri() */
static int rzmq_prepare_uri (lua_State *L)
{
	luaL_checkstring (L, 1);
	lua_settop (L, 1);

	lua_createtable (L, 0, 2);

	/* Check for form: schema:TYPE:endpoint
	 * example: zmq:PAIR:tcp://localhost:10025 */
	if (strmatch (L, 1, "^(.-)%:(.*)$"))
	{
		lua_getfield (L, 3, "upper");
		lua_pushvalue (L, 3);
		lua_call (L, 1, 1);
		lua_pushvalue (L, 4);
	}

	else
	{
		lua_pushnil (L);
		lua_pushvalue (L, 1);
	}

	lua_setfield (L, 2, "endpoint");
	lua_setfield (L, 2, "type");
	lua_settop (L, 2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rzmq_gc() */
static int rzmq_gc (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	if (socket)
		zmq_close (socket);

	return 0;
}
/* }}} */

/* {{{ rzmq_get_fd() */
static int rzmq_get_fd (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	int fd;
	size_t fd_len = sizeof (int);

	int ret = zmq_getsockopt (socket, ZMQ_FD, &fd, &fd_len);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.get_fd()");

	lua_pushinteger (L, fd);
	return 1;
}
/* }}} */

/* {{{ rzmq_get_timeout() */
static int rzmq_get_timeout (lua_State *L)
{
	double timeout = socket_timeout (L, 1);
	lua_pushnumber (L, timeout);
	return 1;
}
/* }}} */

/* {{{ rzmq_set_timeout() */
static int rzmq_set_timeout (lua_State *L)
{
	double new_timeout = luaL_checknumber (L, 2);
	double *timeout = &socket_timeout (L, 1);
	*timeout = new_timeout;
	return 0;
}
/* }}} */

/* {{{ rzmq_is_readable() */
static int rzmq_is_readable (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	int events;
	size_t events_len = sizeof (int);

	int ret = zmq_getsockopt (socket, ZMQ_EVENTS, &events, &events_len);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.is_readable()");

	lua_pushboolean (L, (events & ZMQ_POLLIN));

	return 1;
}
/* }}} */

/* {{{ rzmq_is_writable() */
static int rzmq_is_writable (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	int events;
	size_t events_len = sizeof (int);

	int ret = zmq_getsockopt (socket, ZMQ_EVENTS, &events, &events_len);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.is_writable()");

	lua_pushboolean (L, (events & ZMQ_POLLOUT));

	return 1;
}
/* }}} */

/* {{{ rzmq_is_rcvmore() */
static int rzmq_is_rcvmore (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	int64_t rcvmore;
	size_t rcvmore_len = sizeof (rcvmore);

	int ret = zmq_getsockopt (socket, ZMQ_RCVMORE, &rcvmore, &rcvmore_len);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.is_rcvmore()");

	lua_pushboolean (L, rcvmore);
	return 1;
}
/* }}} */

/* {{{ rzmq_bind() */
static int rzmq_bind (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	const char *endpoint = luaL_checkstring (L, 2);

	int ret = zmq_bind (socket, endpoint);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.bind()");

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rzmq_connect() */
static int rzmq_connect (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	const char *endpoint = luaL_checkstring (L, 2);

	int ret = zmq_connect (socket, endpoint);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.connect()");

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rzmq_rawsend() */
static int rzmq_rawsend (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	size_t data_len;
	const char *data = luaL_checklstring (L, 2, &data_len);
	int flags = ZMQ_NOBLOCK | (lua_toboolean (L, 3) ? ZMQ_SNDMORE : 0);

	/* Set up the zmq_msg_t object to send. */
	zmq_msg_t msg;
	zmq_msg_init_size (&msg, data_len);
	memcpy (zmq_msg_data (&msg), data, data_len);

	int ret = zmq_send (socket, &msg, flags);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.send()");
	zmq_msg_close (&msg);

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rzmq_rawrecv() */
static int rzmq_rawrecv (lua_State *L)
{
	void *socket = socket_ptr (L, 1);
	int flags = ZMQ_NOBLOCK;

	/* Set up the zmq_msg_t object to recv. */
	zmq_msg_t msg;
	zmq_msg_init (&msg);

	int ret = zmq_recv (socket, &msg, flags);
	if (ret == -1)
		return raise_zmq_error (L, "ratchet.zmqsocket.recv()");

	/* Build Lua string from zmq_msg_t. */
	lua_pushlstring (L, (const char *) zmq_msg_data (&msg), zmq_msg_size (&msg));
	zmq_msg_close (&msg);

	lua_getfield (L, 1, "is_rcvmore");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);

	return 2;
}
/* }}} */

/* {{{ rzmq_send() */
static int rzmq_send (lua_State *L)
{
	int nargs = lua_gettop (L);
	if (LUA_YIELD == lua_getctx (L, &nargs))
	{
		if (!lua_toboolean (L, nargs+1))
			return 0;
		lua_settop (L, nargs);
	}

	lua_getfield (L, 1, "is_writable");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);
	int is_writable = lua_toboolean (L, -1);
	lua_pop (L, 1);

	if (is_writable)
	{
		int nargs = lua_gettop (L);
		lua_getfield (L, 1, "rawsend");
		lua_insert (L, 1);
		lua_call (L, nargs, LUA_MULTRET);
		return lua_gettop (L);
	}

	lua_pushlightuserdata (L, YIELD_WRITE);
	lua_pushvalue (L, 1);
	return lua_yieldk (L, 2, nargs, rzmq_send);
}
/* }}} */

/* {{{ rzmq_recv() */
static int rzmq_recv (lua_State *L)
{
	int nargs = lua_gettop (L);
	if (LUA_YIELD == lua_getctx (L, &nargs))
	{
		if (!lua_toboolean (L, nargs+1))
			return 0;
		lua_settop (L, nargs);
	}

	lua_getfield (L, 1, "is_readable");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);
	int is_readable = lua_toboolean (L, -1);
	lua_pop (L, 1);

	if (is_readable)
	{
		int nargs = lua_gettop (L);
		lua_getfield (L, 1, "rawrecv");
		lua_insert (L, 1);
		lua_call (L, nargs, LUA_MULTRET);
		return lua_gettop (L);
	}

	lua_pushlightuserdata (L, YIELD_READ);
	lua_pushvalue (L, 1);
	return lua_yieldk (L, 2, nargs, rzmq_recv);
}
/* }}} */

/* {{{ rzmq_recv_all() */
static int rzmq_recv_all (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_settop (L, 1);
		lua_newtable (L);
	}
	else
	{
		int more, i;
received:
		more = lua_toboolean (L, -1);
		lua_pop (L, 1);
		i = lua_rawlen (L, 2)+1;
		lua_rawseti (L, 2, i);

		if (!more)
		{
			int j;
			luaL_Buffer buff;
			luaL_buffinit (L, &buff);
			for (j=1; j<=i; j++)
			{
				lua_rawgeti (L, 2, j);
				luaL_addvalue (&buff);
			}
			luaL_pushresult (&buff);
			return 1;
		}
	}

	lua_getfield (L, 1, "recv");
	lua_pushvalue (L, 1);
	lua_callk (L, 1, 2, ctx, rzmq_recv_all);

	goto received;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_zmqsocket() */
int luaopen_ratchet_zmqsocket (lua_State *L)
{
	/* Static functions in the ratchet.zmqsocket namespace. */
	static const luaL_Reg funcs[] = {
		{"new", rzmq_new},
		{"prepare_uri", rzmq_prepare_uri},
		{NULL}
	};

	/* Meta-methods for ratchet.zmqsocket object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rzmq_gc},
		{NULL}
	};

	/* Methods in the ratchet.zmqsocket class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_fd", rzmq_get_fd},
		{"get_timeout", rzmq_get_timeout},
		{"set_timeout", rzmq_set_timeout},
		{"bind", rzmq_bind},
		{"connect", rzmq_connect},
		{"send", rzmq_send},
		{"recv", rzmq_recv},
		{"recv_all", rzmq_recv_all},
		/* Undocumented, helper methods. */
		{"is_readable", rzmq_is_readable},
		{"is_writable", rzmq_is_writable},
		{"is_rcvmore", rzmq_is_rcvmore},
		{"rawsend", rzmq_rawsend},
		{"rawrecv", rzmq_rawrecv},
		{NULL}
	};

	/* Set up the ratchet.zmqsocket namespace functions. */
	luaL_newlibtable (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_zmq_class");
	push_new_zmq_context (L);
	luaL_setfuncs (L, funcs, 1);

	/* Set up the ratchet.zmqsocket class and metatables. */
	luaL_newmetatable (L, "ratchet_zmqsocket_meta");
	lua_newtable (L);
	lua_pushvalue (L, -3);
	luaL_setfuncs (L, meths, 1);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
