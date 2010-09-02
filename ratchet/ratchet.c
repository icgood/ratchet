#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "ratchet.h"
#include "epoll.h"
#include "parseuri.h"

/* {{{ luaH_ratchet_add_types() */
static void luaH_ratchet_add_types (lua_State *L)
{
	lua_getfield (L, -2, "socket");
	lua_setfield (L, -2, "tcp");

	lua_getfield (L, -2, "socket");
	lua_setfield (L, -2, "unix");
}
/* }}} */

/* {{{ luaH_ratchet_factory() */
static int luaH_ratchet_factory (lua_State *L)
{
	int rets;

	/* Process the URI based on known schemas. */
	lua_pushvalue (L, 2);
	lua_getfield (L, 1, "uri_schemas");
	rets = luaH_callmethod (L, 1, "parseuri", 2);
	if (rets != 2 || lua_isnil (L, -1))
	{
		lua_pop (L, rets);
		lua_pushnil (L);
		return 1;
	}

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
	lua_remove (L, -3);
	lua_remove (L, -2);

	return rets;
}
/* }}} */

/* {{{ luaopen_luah_ratchet() */
int luaopen_luah_ratchet (lua_State *L)
{
	const luaL_Reg funcs[] = {
//		{"poll_all", luaH_ratchet_poll_all},
		{"factory", luaH_ratchet_factory},
		{"parseuri", luaH_parseuri},
		{NULL}
	};

	luaL_register (L, "luah.ratchet", funcs);

	/* Set up submodule. */
	luaopen_luah_ratchet_epoll (L);
	lua_setfield (L, -2, "epoll");
	luaopen_luah_ratchet_socket (L);
	lua_setfield (L, -2, "socket");

	/* Set up URI schema table. */
	lua_newtable (L);
	luaH_parseuri_add_builtin (L);
	lua_setfield (L, -2, "uri_schemas");

	/* Set up the ratchet type table. */
	lua_newtable (L);
	luaH_ratchet_add_types (L);
	lua_setfield (L, -2, "ratchet_types");

	/* Set up metatable for factory method. */
	lua_newtable (L);
	luaH_setmethod (L, "__call", luaH_ratchet_factory);
	lua_setmetatable (L, -2);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
