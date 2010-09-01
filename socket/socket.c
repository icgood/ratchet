#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "socket.h"
#include "urls.h"

/* {{{ luaopen_luah_netlib_socket() */
int luaopen_luah_netlib_socket (lua_State *L)
{
	luaL_Reg meths[] = {
		{NULL}
	};

	luaH_makecclass (L, meths);

	luaopen_luah_netlib_socket_parseurl (L);
	lua_setfield (L, -2, "parseurl");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
