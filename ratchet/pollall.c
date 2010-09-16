#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/epoll.h>

#include "misc.h"
#include "makeclass.h"
#include "pollall.h"
#include "epoll.h"
#include "zmq_poll.h"

/* {{{ pollall_init() */
static int pollall_init (lua_State *L)
{
	lua_settop (L, 2);

	lua_getfield (L, 1, "zmqpoll");
	lua_call (L, 0, 1);
	lua_setfield (L, 1, "zmqpoll");

	lua_getfield (L, 1, "epoll");
	lua_insert (L, 2);
	lua_call (L, 1, 1);
	lua_setfield (L, 1, "epoll");

	lua_settop (L, 1);
	lua_getfield (L, 1, "zmqpoll");
	lua_getfield (L, 1, "epoll");
	luaH_callmethod (L, 2, "register", 1);

	return 0;
}
/* }}} */

/* {{{ pollall_register() */
static int pollall_register (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets;

	luaH_callmethod (L, 2, "getfd", 0);
	if (luaH_strequal (L, -2, "zmq"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "zmqpoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "register", args);
	}
	else if (luaH_strequal (L, -2, "fd"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "epoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "register", args);
	}
	else
		return luaL_argerror (L, 2, "object not pollable");

	return rets;
}
/* }}} */

/* {{{ pollall_modify() */
static int pollall_modify (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets;

	luaH_callmethod (L, 2, "getfd", 0);
	if (luaH_strequal (L, -2, "zmq"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "zmqpoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "modify", args);
	}
	else if (luaH_strequal (L, -2, "fd"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "epoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "modify", args);
	}
	else
		return luaL_argerror (L, 2, "object not pollable");

	return rets;
}
/* }}} */

/* {{{ pollall_set_writable() */
static int pollall_set_writable (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets;

	luaH_callmethod (L, 2, "getfd", 0);
	if (luaH_strequal (L, -2, "zmq"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "zmqpoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "set_writable", args);
	}
	else if (luaH_strequal (L, -2, "fd"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "epoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "set_writable", args);
	}
	else
		return luaL_argerror (L, 2, "object not pollable");

	return rets;
}
/* }}} */

/* {{{ pollall_unset_writable() */
static int pollall_unset_writable (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets;

	luaH_callmethod (L, 2, "getfd", 0);
	if (luaH_strequal (L, -2, "zmq"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "zmqpoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "unset_writable", args);
	}
	else if (luaH_strequal (L, -2, "fd"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "epoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "unset_writable", args);
	}
	else
		return luaL_argerror (L, 2, "object not pollable");

	return rets;
}
/* }}} */

/* {{{ pollall_unregister() */
static int pollall_unregister (lua_State *L)
{
	int args = lua_gettop (L) - 1;
	int rets;

	luaH_callmethod (L, 2, "getfd", 0);
	if (luaH_strequal (L, -2, "zmq"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "zmqpoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "unregister", args);
	}
	else if (luaH_strequal (L, -2, "fd"))
	{
		lua_pop (L, 2);
		lua_getfield (L, 1, "epoll");
		lua_insert (L, 2);
		rets = luaH_callmethod (L, 2, "unregister", args);
	}
	else
		return luaL_argerror (L, 2, "object not pollable");

	return rets;
}
/* }}} */

/* {{{ pollall_wait() */
static int pollall_wait (lua_State *L)
{
	lua_settop (L, 3);
	
	lua_getfield (L, 1, "zmqpoll");
	lua_getfield (L, -1, "wait");
	lua_insert (L, -2);
	lua_pushvalue (L, 2);
	lua_call (L, 2, 3);

	lua_getfield (L, 1, "epoll");
	lua_getfield (L, -1, "wait");
	lua_insert (L, -2);
	lua_pushinteger (L, 0);	/* We don't want a timeout on epoll. */
	lua_pushvalue (L, 3);
	lua_call (L, 3, 3);

	return 6;
}
/* }}} */

/* {{{ luaopen_luah_ratchet_pollall() */
int luaopen_luah_ratchet_pollall (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", pollall_init},
		{"register", pollall_register},
		{"modify", pollall_modify},
		{"set_writable", pollall_set_writable},
		{"unset_writable", pollall_unset_writable},
		{"unregister", pollall_unregister},
		{"wait", pollall_wait},
		{NULL}
	};

	luaH_newclass (L, "luah.ratchet.pollall", meths);

	/* Grab some dependancy modules. */
	luaopen_luah_zmq_poll (L);
	lua_setfield (L, -2, "zmqpoll");
	luaopen_luah_epoll (L);
	lua_setfield (L, -2, "epoll");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
