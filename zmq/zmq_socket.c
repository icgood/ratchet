#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <zmq.h>
#include <string.h>

#include "misc.h"
#include "makeclass.h"
#include "zmq_socket.h"

#define GETSOCKOPT_MAXLEN 256

/* {{{ zmqsock_init() */
static int zmqsock_init (lua_State *L)
{
	void *context = lua_touserdata (L, 2);
	int type = luaL_checkinteger (L, 3);
	int defflags = 0;

	if (lua_isnumber (L, 4))
		defflags = lua_tointeger (L, 4);

	void *socket = zmq_socket (context, type);
	if (socket == NULL)
		return luaH_perror (L);
	
	lua_pushlightuserdata (L, socket);
	lua_setfield (L, 1, "socket");
	lua_pushinteger (L, defflags);
	lua_setfield (L, 1, "defflags");
	
	return 0;
}
/* }}} */

/* {{{ zmqsock_del() */
static int zmqsock_del (lua_State *L)
{
	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_close (socket) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqsock_getfd() */
static int zmqsock_getfd (lua_State *L)
{
	lua_getfield (L, 1, "socket");
	return 1;
}
/* }}} */

/* {{{ zmqsock_bind() */
static int zmqsock_bind (lua_State *L)
{
	const char *endpoint = luaL_checkstring (L, 2);

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_bind (socket, endpoint) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqsock_connect() */
static int zmqsock_connect (lua_State *L)
{
	const char *endpoint = luaL_checkstring (L, 2);

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_connect (socket, endpoint) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqsock_getsockopt() */
static int zmqsock_getsockopt (lua_State *L)
{
	int option = luaL_checkinteger (L, 2);
	char buffer[GETSOCKOPT_MAXLEN];
	size_t len;
	memset (buffer, 0, sizeof (buffer));

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_getsockopt (socket, option, buffer, &len) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	void *ret = lua_newuserdata (L, len);
	memcpy (ret, buffer, len);

	return 1;
}
/* }}} */

/* {{{ zmqsock_setsockopt() */
static int zmqsock_setsockopt (lua_State *L)
{
	int option = luaL_checkinteger (L, 2);
	void *setto = lua_touserdata (L, 3);
	size_t settolen = lua_objlen (L, 3);

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_setsockopt (socket, option, setto, settolen) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqsock_recv() */
static int zmqsock_recv (lua_State *L)
{
	int flags = 0;
	zmq_msg_t msg;

	lua_settop (L, 2);

	lua_getfield (L, 1, "defflags");
	flags |= lua_tointeger (L, -1);
	lua_pop (L, 1);

	if (lua_isnumber (L, 2))
		flags |= lua_tointeger (L, 2);

	if (zmq_msg_init (&msg) < 0)
		return luaH_perror (L);

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_recv (socket, &msg, flags) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	lua_pushlstring (L, (const char *) zmq_msg_data (&msg), zmq_msg_size (&msg));

	if (zmq_msg_close (&msg) < 0)
		return luaH_perror (L);

	return 1;
}
/* }}} */

/* {{{ zmqsock_send() */
static int zmqsock_send (lua_State *L)
{
	int flags = 0;
	const char *data;
	size_t datalen;
	zmq_msg_t msg;

	lua_settop (L, 3);

	lua_getfield (L, 1, "defflags");
	flags |= lua_tointeger (L, -1);
	lua_pop (L, 1);

	if (lua_isnumber (L, 3))
		flags |= lua_tointeger (L, 3);

	int t = lua_type (L, 2);
	if (t == LUA_TUSERDATA)
	{
		datalen = lua_objlen (L, 2);
		data = (const char *) lua_touserdata (L, 2);
	}
	else if (t == LUA_TSTRING)
		data = lua_tolstring (L, 2, &datalen);
	else
		return luaL_argerror (L, 2, "string or heavy userdata required");

	if (zmq_msg_init_size (&msg, datalen) < 0)
		return luaH_perror (L);
	memcpy (zmq_msg_data (&msg), data, datalen);

	lua_getfield (L, 1, "socket");
	void *socket = lua_touserdata (L, -1);
	if (zmq_send (socket, &msg, flags) < 0)
		return luaH_perror (L);
	lua_pop (L, 1);

	if (zmq_msg_close (&msg) < 0)
		return luaH_perror (L);

	return 0;
}
/* }}} */

/* {{{ zmqsock_sendsome() */
static int zmqsock_sendsome (lua_State *L)
{
	int flags = ZMQ_SNDMORE;

	lua_settop (L, 3);
	if (lua_isnumber (L, 3))
		flags |= lua_tointeger (L, 3);
	lua_pushinteger (L, flags);
	lua_replace (L, 3);

	return luaH_callmethod (L, 1, "send", 2);
}
/* }}} */

/* {{{ luaopen_luah_zmq_socket() */
int luaopen_luah_zmq_socket (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", zmqsock_init},
		{"del", zmqsock_del},
		{"getfd", zmqsock_getfd},
		{"bind", zmqsock_bind},
		{"connect", zmqsock_connect},
		{"getsockopt", zmqsock_getsockopt},
		{"setsockopt", zmqsock_setsockopt},
		{"send", zmqsock_send},
		{"sendsome", zmqsock_sendsome},
		{"recv", zmqsock_recv},
		{NULL}
	};

	luaH_newclass (L, "luah.zmq.socket", meths);

	/* Socket types. */
	lua_pushinteger (L, ZMQ_PAIR);
	lua_setfield (L, -2, "PAIR");
	lua_pushinteger (L, ZMQ_PUB);
	lua_setfield (L, -2, "PUB");
	lua_pushinteger (L, ZMQ_SUB);
	lua_setfield (L, -2, "SUB");
	lua_pushinteger (L, ZMQ_REQ);
	lua_setfield (L, -2, "REQ");
	lua_pushinteger (L, ZMQ_REP);
	lua_setfield (L, -2, "REP");
	lua_pushinteger (L, ZMQ_XREQ);
	lua_setfield (L, -2, "XREQ");
	lua_pushinteger (L, ZMQ_XREP);
	lua_setfield (L, -2, "XREP");
#ifdef ZMQ_PULL
	lua_pushinteger (L, ZMQ_PULL);
#else
	lua_pushinteger (L, ZMQ_UPSTREAM);
#endif
	lua_setfield (L, -2, "PULL");
#ifdef ZMQ_PUSH
	lua_pushinteger (L, ZMQ_PUSH);
#else
	lua_pushinteger (L, ZMQ_DOWNSTREAM);
#endif
	lua_setfield (L, -2, "PUSH");

	/* Socket options. */
	lua_pushinteger (L, ZMQ_HWM);
	lua_setfield (L, -2, "HWM");
	lua_pushinteger (L, ZMQ_SWAP);
	lua_setfield (L, -2, "SWAP");
	lua_pushinteger (L, ZMQ_AFFINITY);
	lua_setfield (L, -2, "AFFINITY");
	lua_pushinteger (L, ZMQ_IDENTITY);
	lua_setfield (L, -2, "IDENTITY");
	lua_pushinteger (L, ZMQ_SUBSCRIBE);
	lua_setfield (L, -2, "SUBSCRIBE");
	lua_pushinteger (L, ZMQ_UNSUBSCRIBE);
	lua_setfield (L, -2, "UNSUBSCRIBE");
	lua_pushinteger (L, ZMQ_RATE);
	lua_setfield (L, -2, "RATE");
	lua_pushinteger (L, ZMQ_RECOVERY_IVL);
	lua_setfield (L, -2, "RECOVERY_IVL");
	lua_pushinteger (L, ZMQ_MCAST_LOOP);
	lua_setfield (L, -2, "MCAST_LOOP");
	lua_pushinteger (L, ZMQ_SNDBUF);
	lua_setfield (L, -2, "SNDBUF");
	lua_pushinteger (L, ZMQ_RCVBUF);
	lua_setfield (L, -2, "RCVBUF");
	lua_pushinteger (L, ZMQ_RCVMORE);
	lua_setfield (L, -2, "RCVMORE");

	/* Send/recv options. */
	lua_pushinteger (L, ZMQ_NOBLOCK);
	lua_setfield (L, -2, "NOBLOCK");
	lua_pushinteger (L, ZMQ_SNDMORE);
	lua_setfield (L, -2, "SNDMORE");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4: