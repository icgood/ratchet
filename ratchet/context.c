#include <sys/epoll.h>
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

	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "ratchet");
	lua_pushvalue (L, 3);
	lua_setfield (L, 1, "type");
	lua_pushvalue (L, 4);
	lua_setfield (L, 1, "engine");

	return luaH_callmethod (L, 1, "on_init", nargs);
}
/* }}} */

/* {{{ ctx_send() */
static int ctx_send (lua_State *L)
{
}
/* }}} */

/* {{{ ctx_recv() */
static int ctx_recv (lua_State *L)
{
}
/* }}} */

/* {{{ ctx_close() */
static int ctx_close (lua_State *L)
{
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
		{"send", ctx_send},
		{"close", ctx_close},
		{"on_init", ctx_default_on_any},
		{"on_send", ctx_default_on_any},
		{"on_recv", ctx_default_on_any},
		{"on_accept", ctx_default_on_any},
		{"on_close", ctx_default_on_any},
		{NULL}
	};

	luaH_newclass (L, NULL, meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
