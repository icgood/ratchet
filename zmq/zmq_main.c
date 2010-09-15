#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <zmq.h>

#include "misc.h"
#include "makeclass.h"
#include "zmq_main.h"
#include "zmq_socket.h"

/* {{{ zmqctx_init() */
static int zmqctx_init (lua_State *L)
{
	int io_threads = 1;
	if (lua_isnumber (L, 2))
		io_threads = lua_tointeger (L, 2);

	void *context = zmq_init (io_threads);
	if (context == NULL)
		return luaH_perror (L);
	
	lua_pushlightuserdata (L, context);
	lua_setfield (L, 1, "ctx");
	
	return 0;
}
/* }}} */

/* {{{ zmqctx_del() */
static int zmqctx_del (lua_State *L)
{
	lua_getfield (L, 1, "ctx");
	void *context = lua_touserdata (L, -1);
	if (zmq_term (context) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqctx_listen() */
static int zmqctx_listen (lua_State *L)
{
#ifdef ZMQ_PULL
	int type = ZMQ_PULL;
#else
	int type = ZMQ_UPSTREAM;
#endif
	lua_settop (L, 3);

	if (lua_isnumber (L, 3))
		type = lua_tointeger (L, 3);

	lua_getfield (L, 1, "socket");
	lua_getfield (L, 1, "ctx");
	lua_pushinteger (L, type);
	luaH_callfunction (L, -3, 2);
	lua_replace (L, 4);

	lua_pushvalue (L, 2);
	luaH_callmethod (L, 4, "bind", 1);
	lua_settop (L, 4);

	return 1;
}
/* }}} */

/* {{{ zmqctx_connect() */
static int zmqctx_connect (lua_State *L)
{
#ifdef ZMQ_PUSH
	int type = ZMQ_PUSH;
#else
	int type = ZMQ_DOWNSTREAM;
#endif
	lua_settop (L, 3);

	if (lua_isnumber (L, 3))
		type = lua_tointeger (L, 3);

	lua_getfield (L, 1, "socket");
	lua_getfield (L, 1, "ctx");
	lua_pushinteger (L, type);
	luaH_callfunction (L, -3, 2);
	lua_replace (L, 4);

	lua_pushvalue (L, 2);
	luaH_callmethod (L, 4, "connect", 1);
	lua_settop (L, 4);

	return 1;
}
/* }}} */

/* {{{ zmqctx_version() */
static int zmqctx_version (lua_State *L)
{
	int major, minor, rev;
	luaL_Buffer b;

	zmq_version (&major, &minor, &rev);

	luaL_buffinit (L, &b);
	lua_pushinteger (L, major);
	luaL_addvalue (&b);
	luaL_addchar (&b, '.');
	lua_pushinteger (L, minor);
	luaL_addvalue (&b);
	luaL_addchar (&b, '.');
	lua_pushinteger (L, rev);
	luaL_addvalue (&b);
	luaL_pushresult (&b);

	return 1;
}
/* }}} */

/* {{{ luaopen_luah_zmq() */
int luaopen_luah_zmq (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", zmqctx_init},
		{"del", zmqctx_del},
		{"listen", zmqctx_listen},
		{"connect", zmqctx_connect},
		{"version", zmqctx_version},
		{NULL}
	};

	luaH_newclass (L, "luah.zmq", meths);

	/* Set up submodules. */
	luaopen_luah_zmq_socket (L);
	luaH_setclassfield (L, -2, "socket");
	luaopen_luah_zmq_poll (L);
	luaH_setclassfield (L, -2, "poll");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
