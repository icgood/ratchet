#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <zmq.h>

#include "misc.h"
#include "makeclass.h"
#include "zmq_main.h"
#include "zmq_socket.h"

#ifndef DEFAULT_ZMQ_IO_THREADS
#define DEFAULT_ZMQ_IO_THREADS 5
#endif

/* {{{ zmqctx_init() */
static int zmqctx_init (lua_State *L)
{
	int io_threads = DEFAULT_ZMQ_IO_THREADS;
	int def = 0;

	/* Grab the named parameters. */
	if (lua_istable (L, 2))
	{
		for (lua_pushnil (L); lua_next (L, 2) != 0; lua_pop (L, 1))
		{
			if (luaH_strequal (L, -2, "io_threads"))
			{
				if (!lua_isnumber (L, -1))
					return luaL_argerror (L, 2, "named param 'io_threads' must be number");
				io_threads = lua_tointeger (L, -1);
			}
			if (luaH_strequal (L, -2, "default"))
			{
				if (!lua_isboolean (L, -1))
					return luaL_argerror (L, 2, "named param 'default' must be boolean");
				def = lua_toboolean (L, -1);
			}
		}
	}

	void *context = zmq_init (io_threads);
	if (context == NULL)
		return luaH_perror (L);
	
	lua_pushlightuserdata (L, context);
	lua_setfield (L, 1, "ctx");

	if (def)
	{
		lua_pushvalue (L, 1);
		lua_setfield (L, LUA_REGISTRYINDEX, "luah_zmq_default_context");
	}
	
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

	lua_newtable (L);
	lua_pushinteger (L, type);
	lua_setfield (L, -2, "type");
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "endpoint");

	lua_pushvalue (L, 1);

	luaH_callfunction (L, 4, 2);
	luaH_callmethod (L, -1, "listen", 0);

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

	lua_newtable (L);
	lua_pushinteger (L, type);
	lua_setfield (L, -2, "type");
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "endpoint");

	lua_pushvalue (L, 1);

	luaH_callfunction (L, 4, 2);
	luaH_callmethod (L, -1, "connect", 0);

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

	/* Set up a default context in the registry. */
	lua_getfield (L, LUA_REGISTRYINDEX, "luah_zmq_default_context");
	if (lua_isnil (L, -1))
	{
		lua_pushinteger (L, DEFAULT_ZMQ_IO_THREADS);
		luaH_callfunction (L, -3, 1);
		lua_setfield (L, LUA_REGISTRYINDEX, "luah_zmq_default_context");
	}
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
