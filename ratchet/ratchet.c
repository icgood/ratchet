#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "ratchet.h"
#include "context.h"
#include "parseuri.h"
#include "zmq_socket.h"

/* {{{ luaH_ratchet_add_types() */
static void luaH_ratchet_add_types (lua_State *L)
{
	luaopen_luah_socket (L);
	lua_setfield (L, -2, "tcp");

	luaopen_luah_socket (L);
	lua_setfield (L, -2, "rawfd");

	luaopen_luah_socket (L);
	lua_setfield (L, -2, "unix");

	luaopen_luah_zmq_socket (L);
	lua_setfield (L, -2, "zmq");
}
/* }}} */

/* {{{ ratchet_init() */
static int ratchet_init (lua_State *L)
{
	luaL_argcheck (L, !lua_isnoneornil (L, 2), 2, "poller argument required");
	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "poller");

	return 0;
}
/* }}} */

/* {{{ ratchet_urifactory() */
static int ratchet_urifactory (lua_State *L)
{
	int rets;

	/* Process the URI based on known schemas. */
	lua_getfield (L, 1, "parseuri");
	lua_pushvalue (L, 2);
	lua_getfield (L, 1, "uri_schemas");
	rets = luaH_callfunction (L, -3, 2);
	if (rets != 2 || lua_isnil (L, -1))
	{
		lua_pop (L, rets+1);
		lua_pushnil (L);
		return 1;
	}
	lua_remove (L, -3);

	/* Get the ratchet type handler for the URI schema. */
	lua_getfield (L, 1, "ratchet_types");
	lua_pushvalue (L, -3);
	lua_gettable (L, -2);
	if (lua_isnil (L, -1))
	{
		lua_pop (L, 4);
		lua_pushnil (L);
		return 1;
	}
	lua_remove (L, -2);

	/* Get the ratchet object from the handler, given the data. */
	lua_pushvalue (L, -2);
	lua_call (L, 1, 1);
	lua_remove (L, -2);

	return 2;
}
/* }}} */

/* {{{ ratchet_instantiate_context() */
static int ratchet_instantiate_context (lua_State *L)
{
	int args = lua_gettop (L);
	int extra_args = (args > 3 ? args-3 : 0);
	luaL_checkstring (L, 2);

	if (args < 3)
	{
		luaH_callmethod (L, 1, "new_context", 0);
		lua_insert (L, 3);
	}

	lua_pushvalue (L, 2);
	int rets = luaH_callmethod (L, 1, "urifactory", 1);
	if (rets != 2 || lua_isnil (L, -1))
		return 0;
	lua_insert (L, 4);
	lua_insert (L, 4);
	lua_getfield (L, 1, "poller");
	lua_insert (L, 4);

	return luaH_callfunction (L, 3, 3+extra_args);
}
/* }}} */

/* {{{ ratchet_attach() */
static int ratchet_attach (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = luaH_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;

	return 1;
}
/* }}} */

/* {{{ ratchet_connect() */
static int ratchet_connect (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = luaH_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;
	lua_getfield (L, -1, "engine");
	luaH_callmethod (L, -1, "connect", 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_listen() */
static int ratchet_listen (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets = luaH_callmethod (L, 1, "instantiate_context", args);
	if (rets != 1)
		return 0;
	lua_getfield (L, -1, "engine");
	luaH_callmethod (L, -1, "listen", 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_new_context() */
static int ratchet_new_context (lua_State *L)
{
	lua_pushcfunction (L, luaH_ratchet_new_context);
	lua_call (L, 0, 1);
	return 1;
}
/* }}} */

/* {{{ ratchet_handle_events() */
static int ratchet_handle_events (lua_State *L)
{
	/* This essentially mimics the "generic for" ability of lua, calling ratchet's
	 * "handle_one" method with every argument not involved in iteration. */
	while (1)
	{
		luaH_dupvalue (L, 3);
		int rets = luaH_callfunction (L, 2, 2);
		lua_settop (L, 3+rets);
		if (!rets || lua_isnil (L, -rets))
			break;
		luaH_callmethod (L, 1, "handle_one", rets-1);
		lua_settop (L, 4);
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

	if (luaH_callboolmethod (L, 2, "error", 0))
		luaH_callmethod (L, 3, "on_error", 0);
	lua_settop (L, 3);
	
	if (luaH_callboolmethod (L, 2, "hangup", 0))
		luaH_callmethod (L, 3, "on_close", 0);
	lua_settop (L, 3);
	
	if (luaH_callboolmethod (L, 2, "readable", 0))
		luaH_callmethod (L, 3, "on_recv", 0);
	lua_settop (L, 3);
	
	if (luaH_callboolmethod (L, 2, "writable", 0))
		luaH_callmethod (L, 3, "raw_send_one", 0);
	lua_settop (L, 3);
	
	return 0;
}
/* }}} */

/* {{{ ratchet_run_once() */
static int ratchet_run_once (lua_State *L)
{
	lua_settop (L, 5);
	if (lua_istable (L, 2))
	{
		lua_getfield (L, 2, "timeout");
		lua_replace (L, 4);
		lua_getfield (L, 2, "maxevents");
		lua_replace (L, 5);
	}
	else if (!lua_isnoneornil (L, 2))
		luaL_checktype (L, 2, LUA_TTABLE);

	lua_getfield (L, 1, "poller");
	lua_replace (L, 3);

	luaH_callmethod (L, 3, "wait", 2);
	lua_settop (L, 6);	/* The three method rets now reside in indices 4-6. */

	int rets = luaH_callmethod (L, 1, "handle_events", 3);
	lua_pop (L, rets);
	
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
		luaH_callmethod (L, 1, "run_once", 1);
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
		luaH_callmethod (L, 1, "run_once", 1);
		lua_settop (L, 3);
	}

	return 0;
}
/* }}} */

/* {{{ luaopen_luah_ratchet() */
int luaopen_luah_ratchet (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", ratchet_init},
		{"parseuri", luaH_parseuri},
		{"urifactory", ratchet_urifactory},
		{"instantiate_context", ratchet_instantiate_context},
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

	luaH_newclass (L, "luah.ratchet", meths);

	/* Set up URI schema table. */
	lua_newtable (L);
	luaH_parseuri_add_builtin (L);
	lua_setfield (L, -2, "uri_schemas");

	/* Set up the ratchet type table. */
	lua_newtable (L);
	luaH_ratchet_add_types (L);
	lua_setfield (L, -2, "ratchet_types");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
