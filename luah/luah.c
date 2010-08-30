#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "epoll.h"
#include "socket.h"

/* {{{ luaopen_luah() */
int luaopen_luah (lua_State *L)
{
	lua_newtable (L);

	luaopen_luah_makeclass (L);
	lua_setfield (L, -2, "makeclass");

	luaopen_luah_netlib (L);
	lua_setfield (L, -2, "netlib");

	return 1;
}
/* }}} */

/* {{{ luaopen_luah_netlib() */
int luaopen_luah_netlib (lua_State *L)
{
	lua_newtable (L);

	luaopen_luah_netlib_epoll (L);
	lua_setfield (L, -2, "epoll");
	luaopen_luah_netlib_socket (L);
	lua_setfield (L, -2, "socket");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
