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

#include <zmq.h>

#include "misc.h"
#include "makeclass.h"
#include "zmq_main.h"
#include "zmq_socket.h"

#ifndef DEFAULT_ZMQ_IO_THREADS
#define DEFAULT_ZMQ_IO_THREADS 5
#endif

/* {{{ zmqctx_init() */
static int zmqctx_init (lua_State *L)
{
	int io_threads = DEFAULT_ZMQ_IO_THREADS;
	int def = 0;

	/* Grab the named parameters. */
	if (lua_istable (L, 2))
	{
		for (lua_pushnil (L); lua_next (L, 2) != 0; lua_pop (L, 1))
		{
			if (rhelp_strequal (L, -2, "io_threads"))
			{
				if (!lua_isnumber (L, -1))
					return luaL_argerror (L, 2, "named param 'io_threads' must be number");
				io_threads = lua_tointeger (L, -1);
			}
			if (rhelp_strequal (L, -2, "default"))
			{
				if (!lua_isboolean (L, -1))
					return luaL_argerror (L, 2, "named param 'default' must be boolean");
				def = lua_toboolean (L, -1);
			}
		}
	}

	void *context = zmq_init (io_threads);
	if (context == NULL)
		return rhelp_perror (L);
	
	lua_pushlightuserdata (L, context);
	lua_setfield (L, 1, "ctx");

	if (def)
	{
		lua_pushvalue (L, 1);
		lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_zmq_default_context");
	}
	
	return 0;
}
/* }}} */

/* {{{ zmqctx_del() */
static int zmqctx_del (lua_State *L)
{
	lua_getfield (L, 1, "ctx");
	void *context = lua_touserdata (L, -1);
	if (zmq_term (context) < 0)
		return rhelp_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqctx_listen() */
static int zmqctx_listen (lua_State *L)
{
#ifdef ZMQ_PULL
	int type = ZMQ_PULL;
#else
	int type = ZMQ_UPSTREAM;
#endif
	lua_settop (L, 3);

	if (lua_isnumber (L, 3))
		type = lua_tointeger (L, 3);

	lua_getfield (L, 1, "socket");

	lua_newtable (L);
	lua_pushinteger (L, type);
	lua_setfield (L, -2, "type");
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "endpoint");

	lua_pushvalue (L, 1);

	rhelp_callfunction (L, 4, 2);
	rhelp_callmethod (L, -1, "listen", 0);

	return 1;
}
/* }}} */

/* {{{ zmqctx_connect() */
static int zmqctx_connect (lua_State *L)
{
#ifdef ZMQ_PUSH
	int type = ZMQ_PUSH;
#else
	int type = ZMQ_DOWNSTREAM;
#endif
	lua_settop (L, 3);

	if (lua_isnumber (L, 3))
		type = lua_tointeger (L, 3);

	lua_getfield (L, 1, "socket");

	lua_newtable (L);
	lua_pushinteger (L, type);
	lua_setfield (L, -2, "type");
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "endpoint");

	lua_pushvalue (L, 1);

	rhelp_callfunction (L, 4, 2);
	rhelp_callmethod (L, -1, "connect", 0);

	return 1;
}
/* }}} */

/* {{{ zmqctx_version() */
static int zmqctx_version (lua_State *L)
{
	int major, minor, rev;
	luaL_Buffer b;

	zmq_version (&major, &minor, &rev);

	luaL_buffinit (L, &b);
	lua_pushinteger (L, major);
	luaL_addvalue (&b);
	luaL_addchar (&b, '.');
	lua_pushinteger (L, minor);
	luaL_addvalue (&b);
	luaL_addchar (&b, '.');
	lua_pushinteger (L, rev);
	luaL_addvalue (&b);
	luaL_pushresult (&b);

	return 1;
}
/* }}} */

/* {{{ zmqctx_parse_uri() */
static int zmqctx_parse_uri (lua_State *L)
{
	lua_settop (L, 1);
	if (rhelp_strmatch (L, "^([^%:]*)%:(.*)$"))
	{
		rhelp_callmethod (L, 2, "upper", 0);
		lua_replace (L, 2);
	}
	else
	{
		/* Use the original string as endpoint and nil as type. */
		lua_pushnil (L);
		lua_pushvalue (L, 1);
	}

	lua_newtable (L);
	lua_insert (L, 2);
	lua_setfield (L, 2, "endpoint");
	lua_setfield (L, 2, "type");

	return 1;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_zmq() */
int luaopen_ratchet_zmq (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", zmqctx_init},
		{"del", zmqctx_del},
		{"listen", zmqctx_listen},
		{"connect", zmqctx_connect},
		{"version", zmqctx_version},
		{NULL}
	};
	const luaL_Reg funcs[] = {
		{"parse_uri", zmqctx_parse_uri},
		{NULL}
	};

	rhelp_newclass (L, "ratchet.zmq", meths, funcs);

	/* Set up submodules. */
	luaopen_ratchet_zmq_socket (L);
	lua_setfield (L, -2, "socket");
	luaopen_ratchet_zmq_poll (L);
	lua_setfield (L, -2, "poll");

	/* Set up a default context in the registry. */
	lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_zmq_default_context");
	if (lua_isnil (L, -1))
	{
		lua_pushinteger (L, DEFAULT_ZMQ_IO_THREADS);
		rhelp_callfunction (L, -3, 1);
		lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_zmq_default_context");
	}
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
