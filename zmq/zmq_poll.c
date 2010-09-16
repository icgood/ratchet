#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <zmq.h>
#include <string.h>

#include "misc.h"
#include "makeclass.h"
#include "zmq_poll.h"

const size_t sizeof_zmq_pollitem_t = sizeof (zmq_pollitem_t);

/* {{{ zmqpoll_wait_iter() */
static int zmqpoll_wait_iter (lua_State *L)
{
	zmq_pollitem_t *pollitems, *curr = NULL;
	int i, j, found, nitems;

	pollitems = (zmq_pollitem_t *) lua_touserdata (L, 1);
	i = lua_tointeger (L, 2);
	nitems = (int) (lua_objlen (L, 1) / sizeof_zmq_pollitem_t);

	/* Check if we're done iterating, update i and get curr if not. */
	found = 0;
	for (j=0; j<nitems; j++)
	{
		if (pollitems[j].revents && ++found == i)
		{
			curr = &pollitems[j];
			break;
		}
	}
	if (!curr)
		return 0;
	lua_pushinteger (L, i+1);

	/* Push the status of the triggered item. */
	lua_getfield (L, lua_upvalueindex (1), "status");
	lua_pushinteger (L, curr->revents);
	lua_call (L, 1, 1);

	/* Get the associated object, keyed on curr. */
	lua_getfield (L, lua_upvalueindex (1), "item_ref");
	if (curr->socket)
		lua_pushlightuserdata (L, curr->socket);
	else
		lua_pushinteger (L, curr->fd);
	lua_rawget (L, -2);
	lua_replace (L, -2);

	return 3;
}
/* }}} */

/* {{{ zmqpoll_init() */
static int zmqpoll_init (lua_State *L)
{
	lua_newuserdata (L, 0);
	lua_setfield (L, 1, "pollitems");

	lua_pushvalue (L, 1);
	lua_pushcclosure (L, zmqpoll_wait_iter, 1);
	lua_setfield (L, 1, "wait_iter");

	lua_newtable (L);
	lua_setfield (L, 1, "item_ref");

	return 0;
}
/* }}} */

/* {{{ zmqpoll_wait() */
static int zmqpoll_wait (lua_State *L)
{
	long timeout = -1;
	zmq_pollitem_t *pollitems;
	int nitems;

	lua_settop (L, 2);
	
	if (lua_isnumber (L, 2))
		timeout = (long) ((double) lua_tonumber (L, 2) * 1000000.0);
	lua_getfield (L, 1, "pollitems");
	pollitems = (zmq_pollitem_t *) lua_touserdata (L, -1);
	nitems = (int) (lua_objlen (L, -1) / sizeof_zmq_pollitem_t);
	if (zmq_poll (pollitems, nitems, timeout) < 0)
		return luaH_perror (L);

	/* Organize the iterator function, invariant data, and control variable. */
	lua_getfield (L, 1, "wait_iter");
	lua_insert (L, -2);
	lua_pushinteger (L, 1);

	return 3;
}
/* }}} */

/* {{{ zmqpoll_getfd() */
static int zmqpoll_getfd (lua_State *L)
{
	lua_pushliteral (L, "zmqpoll");
	lua_pushnil (L);
	return 2;
}
/* }}} */

/* {{{ zmqpoll_register() */
static int zmqpoll_register (lua_State *L)
{
	luaH_callmethod (L, 2, "getfd", 0);
	lua_remove (L, -2);
	lua_getfield (L, 1, "item_ref");
	lua_pushvalue (L, -2);
	lua_pushvalue (L, 2);
	lua_rawset (L, -3);
	lua_pop (L, 1);
	lua_insert (L, 3);

	/* Get pollitems and expand it by one. */
	lua_getfield (L, 1, "pollitems");
	zmq_pollitem_t *oldpollitems = (zmq_pollitem_t *) lua_touserdata (L, -1);
	int nitems = (int) (lua_objlen (L, -1) / sizeof_zmq_pollitem_t);
	zmq_pollitem_t *newpollitems = (zmq_pollitem_t *) lua_newuserdata (L, (nitems+1) * sizeof_zmq_pollitem_t);
	memcpy (newpollitems, oldpollitems, nitems * sizeof_zmq_pollitem_t);
	zmq_pollitem_t *new = &newpollitems[nitems];
	lua_setfield (L, 1, "pollitems");
	lua_pop (L, 1);

	/* Fill out the new zmq_pollitem_t structure. */
	memset (new, 0, sizeof_zmq_pollitem_t);
	int t = lua_type (L, 3);
	if (t == LUA_TLIGHTUSERDATA)
		new->socket = lua_touserdata (L, 3);
	else if (t == LUA_TNUMBER)
		new->fd = lua_tointeger (L, 3);
	int flag_i;
	if (lua_gettop (L) >= 4)
	{
		new->events = 0;
		for (flag_i=4; flag_i<=lua_gettop (L); flag_i++)
			new->events |= luaL_checkint (L, flag_i);
	}
	else
		new->events = ZMQ_POLLIN;

	return 0;
}
/* }}} */

/* {{{ zmqpoll_modify() */
static int zmqpoll_modify (lua_State *L)
{
	int i;
	void *socket = NULL;
	int fd = 0;
	int flags, flag_i;

	luaH_callmethod (L, 2, "getfd", 0);
	lua_replace (L, -2);
	lua_insert (L, 3);

	/* Get whatever flags we now want. */
	if (lua_gettop (L) < 4)
		flags = ZMQ_POLLIN;
	else
	{
		flags = 0;
		for (flag_i=4; flag_i<=lua_gettop (L); flag_i++)
			flags |= luaL_checkint (L, flag_i);
	}

	/* Get the backing for the item to remove.. */
	int t = lua_type (L, 3);
	if (t == LUA_TLIGHTUSERDATA)
		socket = lua_touserdata (L, 3);
	else if (t == LUA_TNUMBER)
		fd = lua_tointeger (L, 3);

	/* Get pollitems and remove the requested item. */
	lua_getfield (L, 1, "pollitems");
	zmq_pollitem_t *pollitems = (zmq_pollitem_t *) lua_touserdata (L, -1);
	int nitems = (int) (lua_objlen (L, -1) / sizeof_zmq_pollitem_t);
	for (i=0; i<nitems; i++)
	{
		/* Skip over the old pollitem we want to remove. */
		if (socket)
		{
			if (socket == pollitems[i].socket)
				pollitems[i].events = flags;
		}
		else if (fd == pollitems[i].fd)
			pollitems[i].events = flags;
	}
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ zmqpoll_set_writable() */
static int zmqpoll_set_writable (lua_State *L)
{
	lua_settop (L, 2);
	lua_pushinteger (L, ZMQ_POLLOUT | ZMQ_POLLIN);

	return luaH_callmethod (L, 1, "modify", 2);
}
/* }}} */

/* {{{ zmqpoll_unset_writable() */
static int zmqpoll_unset_writable (lua_State *L)
{
	lua_settop (L, 2);
	lua_pushinteger (L, ZMQ_POLLIN);

	return luaH_callmethod (L, 1, "modify", 2);
}
/* }}} */

/* {{{ zmqpoll_unregister() */
static int zmqpoll_unregister (lua_State *L)
{
	zmq_pollitem_t *iter;
	int i;
	void *socket = NULL;
	int fd = 0;

	luaH_callmethod (L, 2, "getfd", 0);
	lua_remove (L, -2);
	lua_getfield (L, 1, "item_ref");
	lua_pushvalue (L, -2);
	lua_pushnil (L);
	lua_rawset (L, -3);
	lua_pop (L, 1);
	lua_insert (L, 3);

	/* Get the backing for the item to remove.. */
	int t = lua_type (L, 3);
	if (t == LUA_TLIGHTUSERDATA)
		socket = lua_touserdata (L, 3);
	else if (t == LUA_TNUMBER)
		fd = lua_tointeger (L, 3);

	/* Get pollitems and remove the requested item. */
	lua_getfield (L, 1, "pollitems");
	zmq_pollitem_t *oldpollitems = iter = (zmq_pollitem_t *) lua_touserdata (L, -1);
	int nitems = (int) (lua_objlen (L, -1) / sizeof_zmq_pollitem_t);
	zmq_pollitem_t *newpollitems = (zmq_pollitem_t *) lua_newuserdata (L, (nitems-1) * sizeof_zmq_pollitem_t);
	for (i=0; i<nitems-1; i++)
	{
		/* Skip over the old pollitem we want to remove. */
		if (socket)
		{
			if (socket == iter->socket)
				iter++;
		}
		else if (fd == iter->fd)
			iter++;

		memcpy (&newpollitems[i], iter, sizeof_zmq_pollitem_t);
		iter++;
	}
	lua_setfield (L, 1, "pollitems");
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ status_init() */
static int status_init (lua_State *L)
{
	luaL_checkint (L, 2);
	lua_pushvalue (L, 2);
	lua_setfield (L, 1, "n");

	return 0;
}
/* }}} */

/* {{{ status_readable() */
static int status_readable (lua_State *L)
{
	lua_getfield (L, 1, "n");
	int n = lua_tointeger (L, -1);
	lua_pop (L, 1);

	lua_pushboolean (L, n & ZMQ_POLLIN);
	return 1;
}
/* }}} */

/* {{{ status_writable() */
static int status_writable (lua_State *L)
{
	lua_getfield (L, 1, "n");
	int n = lua_tointeger (L, -1);
	lua_pop (L, 1);

	lua_pushboolean (L, n & ZMQ_POLLOUT);
	return 1;
}
/* }}} */

/* {{{ status_hangup() */
static int status_hangup (lua_State *L)
{
	lua_pushboolean (L, 0);
	return 1;
}
/* }}} */

/* {{{ status_error() */
static int status_error (lua_State *L)
{
	lua_getfield (L, 1, "n");
	int n = lua_tointeger (L, -1);
	lua_pop (L, 1);

	lua_pushboolean (L, n & ZMQ_POLLERR);
	return 1;
}
/* }}} */

/* {{{ luaopen_luah_zmq_poll() */
int luaopen_luah_zmq_poll (lua_State *L)
{
	const luaL_Reg meths[] = {
		{"init", zmqpoll_init},
		{"getfd", zmqpoll_getfd},
		{"register", zmqpoll_register},
		{"modify", zmqpoll_modify},
		{"set_writable", zmqpoll_set_writable},
		{"unset_writable", zmqpoll_unset_writable},
		{"unregister", zmqpoll_unregister},
		{"wait", zmqpoll_wait},
		{NULL}
	};

	luaH_newclass (L, "luah.zmq.poll", meths);

	luaL_Reg status_meths[] = {
		{"init", status_init},
		{"readable", status_readable},
		{"writable", status_writable},
		{"hangup", status_hangup},
		{"error", status_error},
		{NULL}
	};
	luaH_newclass (L, NULL, status_meths);
	lua_setfield (L, -2, "status");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
