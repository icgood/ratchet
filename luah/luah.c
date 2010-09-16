#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "epoll.h"
#include "zmq_main.h"
#include "dns.h"
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
	luaopen_luah_zmq (L);
	lua_setfield (L, -2, "zmq");
	luaopen_luah_epoll (L);
	lua_setfield (L, -2, "epoll");
	luaopen_luah_dns (L);
	lua_setfield (L, -2, "dns");
	luaopen_luah_socket (L);
	lua_setfield (L, -2, "socket");
	luaopen_luah_xml (L);
	lua_setfield (L, -2, "xml");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
