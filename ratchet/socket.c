#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "socket.h"

/* {{{ luaopen_luah_ratchet_socket() */
int luaopen_luah_ratchet_socket (lua_State *L)
{
	luaL_Reg meths[] = {
		{NULL}
	};

	luaH_newclass (L, "luah.ratchet.socket", meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
