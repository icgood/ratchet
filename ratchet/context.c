#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "context.h"

/* {{{ ctx_init() */
static int ctx_init (lua_State *L)
{
	int nargs = lua_gettop (L) - 4;

	lua_pushboolean (L, 1);
	lua_setfield (L, 1, "is_context");

	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "poller");
	lua_pushvalue (L, 3);
	lua_setfield (L, 1, "type");
	lua_pushvalue (L, 4);
	lua_setfield (L, 1, "engine");

	lua_pushvalue (L, 1);
	luaH_callmethod (L, 2, "register", 1);

	/* Set up send_queue with table library function. */
	lua_newtable (L);
	lua_setfield (L, 1, "send_queue");
	luaopen_table (L);
	lua_setfield (L, 1, "tablelib");

	return luaH_callmethod (L, 1, "on_init", nargs);
}
/* }}} */

/* {{{ ctx_getfd() */
static int ctx_getfd (lua_State *L)
{
	lua_getfield (L, 1, "engine");
	int rets = luaH_callmethod (L, -1, "getfd", 0);
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
	luaH_callfunction (L, -3, 2);
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
		luaH_dupvalue (L, 3);
		luaH_callmethod (L, 2, "send", 1);
		luaH_callmethod (L, 1, "on_send", 1);

		return 0;
	}
}
/* }}} */

/* {{{ ctx_recv() */
static int ctx_recv (lua_State *L)
{
	lua_getfield (L, 1, "engine");
	return luaH_callmethod (L, 2, "recv", 0);
}
/* }}} */

/* {{{ ctx_accept() */
static int ctx_accept (lua_State *L)
{
	lua_settop (L, 5);

	lua_getfield (L, 1, "poller");
	lua_replace (L, 3);

	lua_getfield (L, 1, "type");
	lua_replace (L, 4);

	lua_getfield (L, 1, "engine");
	luaH_callmethod (L, -1, "accept", 0);
	lua_replace (L, 5);
	lua_pop (L, 1);

	return luaH_callfunction (L, 2, 3);
}
/* }}} */

/* {{{ ctx_close() */
static int ctx_close (lua_State *L)
{
	lua_getfield (L, 1, "poller");
	lua_pushvalue (L, 1);
	luaH_callmethod (L, 2, "unregister", 1);
	lua_pop (L, 1);
	
	lua_getfield (L, 1, "engine");
	return luaH_callmethod (L, -1, "close", 0);
}
/* }}} */

/* {{{ ctx_default_on_any() */
static int ctx_default_on_any (lua_State *L)
{
	return 0;
}
/* }}} */

/* {{{ luaH_ratchet_new_context() */
int luaH_ratchet_new_context (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", ctx_init},
		{"getfd", ctx_getfd},
		{"send", ctx_send},
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

	luaH_newclass (L, NULL, meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
