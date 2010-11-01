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

#include "misc.h"
#include "makeclass.h"
#include "context.h"

/* {{{ ratchet_init() */
static int ratchet_init (lua_State *L)
{
	luaL_argcheck (L, !lua_isnoneornil (L, 2), 2, "poller argument required");
	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "poller");

	/* Set up the ratchet type table. */
	lua_newtable (L);
	lua_setfield (L, 1, "ratchet_factories");

	return 0;
}
/* }}} */

/* {{{ ratchet_getfd() */
static int ratchet_getfd (lua_State *L)
{
	lua_getfield (L, 1, "poller");
	return rhelp_callmethod (L, -1, "getfd", 0);
}
/* }}} */

/* {{{ ratchet_filter_uri() */
static int ratchet_filter_uri (lua_State *L)
{
	int args, uvi;

	/* Pull the filter up-value and call it with all args. */
	for (uvi = 2; ; uvi++)
	{
		args = lua_gettop (L);
		lua_pushvalue (L, lua_upvalueindex (uvi));
		if (lua_isnil (L, -1))
		{
			lua_pop (L, 1);
			break;
		}
		lua_insert (L, 1);
		lua_call (L, args, LUA_MULTRET);
	}

	/* Now run the factory with all results from the filter. */
	args = lua_gettop (L);
	lua_pushvalue (L, lua_upvalueindex (1));
	lua_insert (L, 1);
	lua_call (L, args, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_parseuri() */
static int ratchet_parseuri (lua_State *L)
{
	const char *uri = luaL_checkstring (L, 1);
	luaL_checktype (L, 2, LUA_TTABLE);

	lua_pushvalue (L, 1);
	if (!rhelp_strmatch (L, "^([%w%+%.%-]+):(.*)$"))
	{
		lua_pushliteral (L, "tcp");
		lua_pushfstring (L, "//%s", uri);
	}
	lua_remove (L, -3);

	lua_pushvalue (L, -2);
	lua_gettable (L, 2);
	if (lua_isnil (L, -1))
		return luaL_error (L, "Unknown URI scheme: <%s>", uri);
	lua_insert (L, -2);

	lua_call (L, 1, 1);
	return 2;
}
/* }}} */

/* {{{ ratchet_urifactory() */
static int ratchet_urifactory (lua_State *L)
{
	/* Process the URI based on known schemas. */
	lua_getfield (L, 1, "parseuri");
	lua_pushvalue (L, 2);
	lua_getfield (L, 1, "ratchet_factories");
	lua_call (L, 2, 2);

	return 2;
}
/* }}} */

/* {{{ ratchet_instantiate_context() */
static int ratchet_instantiate_context (lua_State *L)
{
	int args = lua_gettop (L);
	int extra_args = (args > 3 ? args-3 : 0);

	if (args < 3)
	{
		rhelp_callmethod (L, 1, "new_context", 0);
		lua_insert (L, 3);
	}

	lua_pushvalue (L, 2);
	if (lua_isstring (L, -1)) /* Use URI to construct engine. */
	{
		int rets = rhelp_callmethod (L, 1, "urifactory", 1);
		if (rets != 2 || lua_isnil (L, -1))
			return 0;
		lua_remove (L, -2);
	}
	lua_insert (L, 4);
	lua_getfield (L, 1, "poller");
	lua_insert (L, 4);

	return rhelp_callfunction (L, 3, 2+extra_args);
}
/* }}} */

/* {{{ ratchet_register_uri() */
static int ratchet_register_uri (lua_State *L)
{
	int args = lua_gettop (L);

	if (args > 3)
		lua_pushcclosure (L, ratchet_filter_uri, args-2);
	lua_getfield (L, 1, "ratchet_factories");
	lua_insert (L, 2);
	lua_rawset (L, 2);

	return 0;
}
/* }}} */

/* {{{ ratchet_attach() */
static int ratchet_attach (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = rhelp_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;

	return 1;
}
/* }}} */

/* {{{ ratchet_connect() */
static int ratchet_connect (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = rhelp_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;
	lua_getfield (L, -1, "engine");
	rhelp_callmethod (L, -1, "connect", 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_listen() */
static int ratchet_listen (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = rhelp_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;
	lua_getfield (L, -1, "engine");
	rhelp_callmethod (L, -1, "listen", 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_handle_events() */
static int ratchet_handle_events (lua_State *L)
{
	int has_errfunc = lua_isfunction (L, 2);

	/* This essentially mimics the "generic for" ability of lua, calling ratchet's
	 * "handle_one" method with every argument not involved in iteration. */
	while (1)
	{
		rhelp_dupvalue (L, 4);
		int rets = rhelp_callfunction (L, 3, 2);
		lua_settop (L, 4+rets);
		if (!rets || lua_isnil (L, -rets))
			break;
		if (has_errfunc)
			rhelp_pcallmethod (L, 1, "handle_one", rets-1, 2);
		else
			rhelp_callmethod (L, 1, "handle_one", rets-1);
		lua_settop (L, 5);
	}
	return 0;
}
/* }}} */

/* {{{ ratchet_handle_one() */
static int ratchet_handle_one (lua_State *L)
{
	lua_getfield (L, 3, "is_context");
	if (!lua_isboolean (L, -1) || !lua_toboolean (L, -1))
		return 0;
	lua_pop (L, 1);

	if (rhelp_callboolmethod (L, 2, "error", 0))
		rhelp_callmethod (L, 3, "on_error", 0);
	lua_settop (L, 3);
	
	if (rhelp_callboolmethod (L, 2, "hangup", 0))
		rhelp_callmethod (L, 3, "on_close", 0);
	lua_settop (L, 3);
	
	if (rhelp_callboolmethod (L, 2, "readable", 0))
		rhelp_callmethod (L, 3, "on_recv", 0);
	lua_settop (L, 3);
	
	if (rhelp_callboolmethod (L, 2, "writable", 0))
		rhelp_callmethod (L, 3, "raw_send", 0);
	lua_settop (L, 3);
	
	return 0;
}
/* }}} */

/* {{{ ratchet_run_once() */
static int ratchet_run_once (lua_State *L)
{
	lua_settop (L, 2);
	if (!lua_istable (L, 2))
	{
		if (!lua_isnoneornil (L, 2))
			luaL_checktype (L, 2, LUA_TTABLE);
		lua_newtable (L);
		lua_replace (L, 2);
	}

	lua_getfield (L, 1, "poller");
	lua_getfield (L, 2, "timeout");
	lua_getfield (L, 2, "maxevents");

	rhelp_callmethod (L, 3, "wait", 2);
	lua_settop (L, 6);	/* The three method rets now reside in indices 4-6. */

	lua_getfield (L, 1, "handle_events");
	lua_replace (L, 3);
	lua_pushvalue (L, 1);
	lua_insert (L, 4);
	lua_getfield (L, 2, "panicf");
	lua_insert (L, 5);
	lua_call (L, 5, 0);
	
	return 0;
}
/* }}} */

/* {{{ ratchet_run() */
static int ratchet_run (lua_State *L)
{
	int iterations = -1;
	int i;

	if (lua_istable (L, 2))
	{
		lua_getfield (L, 2, "iterations");
		if (lua_isnumber (L, -1))
			iterations = lua_tointeger (L, -1);
		lua_pop (L, 1);
	}
	else if (!lua_isnoneornil (L, 2))
		luaL_checktype (L, 2, LUA_TTABLE);

	for (i=0; i<iterations || iterations<0; i++)
	{
		lua_pushvalue (L, 2);
		rhelp_callmethod (L, 1, "run_once", 1);
		lua_settop (L, 2);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_run_until() */
static int ratchet_run_until (lua_State *L)
{
	int iterations = -1;
	int i;

	luaL_checktype (L, 3, LUA_TFUNCTION);

	while (1)
	{
		lua_pushvalue (L, 3);
		lua_call (L, 0, 1);
		if (lua_toboolean (L, -1))
			break;
		lua_pop (L, 1);

		lua_pushvalue (L, 2);
		rhelp_callmethod (L, 1, "run_once", 1);
		lua_settop (L, 3);
	}

	return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet() */
int luaopen_ratchet (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", ratchet_init},
		{"getfd", ratchet_getfd},
		{"parseuri", ratchet_parseuri},
		{"urifactory", ratchet_urifactory},
		{"instantiate_context", ratchet_instantiate_context},
		{"register_uri", ratchet_register_uri},
		{"attach", ratchet_attach},
		{"connect", ratchet_connect},
		{"listen", ratchet_listen},
		{"new_context", ratchet_new_context},
		{"handle_events", ratchet_handle_events},
		{"handle_one", ratchet_handle_one},
		{"run_once", ratchet_run_once},
		{"run", ratchet_run},
		{"run_until", ratchet_run_until},
		{NULL}
	};

	const luaL_Reg funcs[] = {
		{"makeclass", rhelp_makeclass},
		{NULL}
	};

	rhelp_newclass (L, "ratchet", meths, funcs);

#if HAVE_ZMQ
	luaopen_ratchet_zmq (L);
	lua_setfield (L, -2, "zmq");
#endif
#if HAVE_EPOLL
	luaopen_ratchet_epoll (L);
	lua_setfield (L, -2, "epoll");
#endif
#if HAVE_LIBEVENT
	luaopen_ratchet_libevent (L);
	lua_setfield (L, -2, "libevent");
#endif
	luaopen_ratchet_dns (L);
	lua_setfield (L, -2, "dns");
	luaopen_ratchet_socket (L);
	lua_setfield (L, -2, "socket");

	return 1;
}
/* }}} */

/* {{{ ratchet_version() */
const char *ratchet_version (void)
{
	return PACKAGE_VERSION;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
