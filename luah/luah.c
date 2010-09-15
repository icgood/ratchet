#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "epoll.h"
#include "ratchet.h"
#include "socket.h"

/* {{{ luaopen_luah() */
int luaopen_luah (lua_State *L)
{
	const luaL_Reg funcs[] = {
		{"makeclass", luaH_makeclass},
		{NULL}
	};

	luaL_register (L, "luah", funcs);

	luaopen_luah_ratchet (L);
	lua_setfield (L, -2, "ratchet");

	luaopen_luah_rlimit (L);
	lua_setfield (L, -2, "rlimit");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
