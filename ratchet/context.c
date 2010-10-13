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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "context.h"

/* {{{ ctx_init() */
static int ctx_init (lua_State *L)
{
	int nargs = lua_gettop (L) - 3;

	lua_pushboolean (L, 1);
	lua_setfield (L, 1, "is_context");

	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "poller");
	lua_pushvalue (L, 3);
	lua_setfield (L, 1, "engine");

	lua_pushvalue (L, 1);
	rhelp_callmethod (L, 2, "register", 1);

	/* Set up send_queue with table library function. */
	lua_newtable (L);
	lua_setfield (L, 1, "send_queue");
	luaopen_table (L);
	lua_setfield (L, 1, "tablelib");

	return rhelp_callmethod (L, 1, "on_init", nargs);
}
/* }}} */

/* {{{ ctx_getfd() */
static int ctx_getfd (lua_State *L)
{
	lua_getfield (L, 1, "engine");
	int rets = rhelp_callmethod (L, -1, "getfd", 0);
	return rets;
}
/* }}} */

/* {{{ ctx_send() */
static int ctx_send (lua_State *L)
{
	int n;

	lua_settop (L, 2);

	lua_getfield (L, 1, "poller");
	lua_getfield (L, -1, "set_writable");
	lua_insert (L, -2);
	lua_pushvalue (L, 1);
	lua_call (L, 2, 0);

	lua_getfield (L, 1, "send_queue");
	n = lua_objlen (L, -1);
	lua_pushvalue (L, 2);
	lua_rawseti (L, -2, n+1);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ ctx_raw_send() */
static int ctx_raw_send (lua_State *L)
{
	int i, n, rets, has_sendmany;

	lua_getfield (L, 1, "engine");
	lua_getfield (L, -1, "sendmany");
	has_sendmany = (lua_isnil (L, -1) ? 0 : 1);
	lua_pop (L, 2);

	if (!has_sendmany)
		return rhelp_callmethod (L, 1, "raw_send_one", 0);

	lua_settop (L, 3);
	lua_getfield (L, 1, "engine");
	lua_replace (L, 2);

	lua_getfield (L, 1, "send_queue");
	lua_replace (L, 3);

	lua_pushvalue (L, 3);
	rets = rhelp_callmethod (L, 2, "sendmany", 1);
	n = lua_tointeger (L, -1);
	lua_pop (L, rets);
	if (n >= (int) lua_objlen (L, 3))
	{
		lua_getfield (L, 1, "poller");
		lua_getfield (L, -1, "unset_writable");
		lua_insert (L, -2);
		lua_pushvalue (L, 1);
		lua_call (L, 2, 0);
	}
	for (i=1; i<=n; i++)
	{
		lua_rawgeti (L, 3, i);
		rhelp_callmethod (L, 1, "on_send", 1);
	}
	rhelp_tableremoven (L, 3, n);
	
	return 0;
}
/* }}} */

/* {{{ ctx_raw_send_one() */
static int ctx_raw_send_one (lua_State *L)
{
	int n;
	lua_settop (L, 2);

	lua_getfield (L, 1, "tablelib");
	lua_getfield (L, -1, "remove");
	lua_replace (L, -2);
	lua_getfield (L, 1, "send_queue");
	n = (int) lua_objlen (L, -1);
	lua_pushinteger (L, 1);
	rhelp_callfunction (L, -3, 2);
	lua_replace (L, 2);
	lua_pop (L, 1);

	if (n <= 1)
	{
		lua_getfield (L, 1, "poller");
		lua_getfield (L, -1, "unset_writable");
		lua_insert (L, -2);
		lua_pushvalue (L, 1);
		lua_call (L, 2, 0);
	}

	if (!lua_isnil (L, 2))
	{
		lua_getfield (L, 1, "engine");
		lua_insert (L, 2);
		rhelp_dupvalue (L, 3);
		rhelp_callmethod (L, 2, "send", 1);
		rhelp_callmethod (L, 1, "on_send", 1);
	}
	return 0;
}
/* }}} */

/* {{{ ctx_recv() */
static int ctx_recv (lua_State *L)
{
	lua_getfield (L, 1, "engine");
	return rhelp_callmethod (L, 2, "recv", 0);
}
/* }}} */

/* {{{ ctx_accept() */
static int ctx_accept (lua_State *L)
{
	lua_settop (L, 4);

	lua_getfield (L, 1, "poller");
	lua_replace (L, 3);

	lua_getfield (L, 1, "engine");
	rhelp_callmethod (L, -1, "accept", 0);
	lua_replace (L, 4);
	lua_pop (L, 1);

	return rhelp_callfunction (L, 2, 2);
}
/* }}} */

/* {{{ ctx_close() */
static int ctx_close (lua_State *L)
{
	lua_getfield (L, 1, "poller");
	lua_pushvalue (L, 1);
	rhelp_callmethod (L, 2, "unregister", 1);
	lua_pop (L, 1);
	
	lua_getfield (L, 1, "engine");
	return rhelp_callmethod (L, -1, "close", 0);
}
/* }}} */

/* {{{ ctx_default_on_any() */
static int ctx_default_on_any (lua_State *L)
{
	return 0;
}
/* }}} */

/* {{{ ratchet_new_context() */
int ratchet_new_context (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", ctx_init},
		{"getfd", ctx_getfd},
		{"send", ctx_send},
		{"raw_send", ctx_raw_send},
		{"raw_send_one", ctx_raw_send_one},
		{"recv", ctx_recv},
		{"accept", ctx_accept},
		{"close", ctx_close},
		{"on_init", ctx_default_on_any},
		{"on_send", ctx_default_on_any},
		{"on_recv", ctx_default_on_any},
		{"on_close", ctx_default_on_any},
		{"on_error", ctx_default_on_any},
		{NULL}
	};

	rhelp_newclass (L, NULL, meths, NULL);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
