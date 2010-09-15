#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/epoll.h>
#include <string.h>

#include "misc.h"
#include "makeclass.h"
#include "epoll.h"

/* {{{ myepoll_event_iter() */
static int myepoll_event_iter (lua_State *L)
{
	struct epoll_event *events;
	int i, n_events;

	events = (struct epoll_event *) lua_touserdata (L, 1);
	i = lua_tointeger (L, 2);
	n_events = (int) (lua_objlen (L, 1) / sizeof (struct epoll_event));

	/* Check if we're done iterating, and update i if not. */
	if (i > n_events)
		return 0;
	lua_pushinteger (L, i+1);

	/* Push the triggered events. */
	lua_pushinteger (L, events[i-1].events);

	/* Get the associated object, keyed on current fd. */
	lua_getfield (L, lua_upvalueindex (1), "fd_ref");
	lua_rawgeti (L, -1, events[i-1].data.fd);
	lua_replace (L, -2);

	return 3;
}
/* }}} */

/* {{{ myepoll_init() */
static int myepoll_init (lua_State *L)
{
	int fd_est, epfd;

	/* Grab the "fd_est" attribute from parameter. */
	if (lua_gettop (L) > 1)
		fd_est = luaL_checkint (L, 2);
	else
		fd_est = 100;

	/* Build and save the fd_ref table attribute. */
	lua_newtable (L);
	lua_setfield (L, 1, "fd_ref");

	/* Create the epoll object. */
	epfd = epoll_create (fd_est);
	if (epfd == -1)
		return luaH_perror (L);

	/* Save the epfd attribute. */
	lua_pushinteger (L, epfd);
	lua_setfield (L, 1, "epfd");

	/* Build the iteration closure. */
	lua_pushvalue (L, 1);
	lua_pushcclosure (L, myepoll_event_iter, 1);
	lua_setfield (L, 1, "wait_iter");

	return 0;
}
/* }}} */

/* {{{ myepoll_getepfd() */
static int myepoll_getepfd (lua_State *L, int index)
{
	int epfd = -1;

	lua_getfield (L, index, "epfd");
	if (lua_isnumber (L, -1))
		epfd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	return epfd;
}
/* }}} */

/* {{{ myepoll_getmyfd() */
static int myepoll_getmyfd (lua_State *L)
{
	lua_pushinteger (L, myepoll_getepfd (L, 1));
	return 1;
}
/* }}} */

/* {{{ myepoll_del() */
static int myepoll_del (lua_State *L)
{
	int epfd = myepoll_getepfd (L, 1);

	if (close (epfd) == -1)
		return luaH_perror (L);

	return 0;
}
/* }}} */

/* {{{ myepoll_getfd() */
static int myepoll_getfd (lua_State *L, int index)
{
	int ret;

	if (lua_isnumber (L, index))
		ret = luaL_checkint (L, index);

	else if (lua_istable (L, index) || lua_isuserdata (L, index))
	{
		lua_getfield (L, index, "getfd");
		if (!lua_isfunction (L, -1))
			return luaL_argerror (L, index, "table must have getfd()");
		lua_pushvalue (L, index);
		lua_call (L, 1, 1);
		ret = luaL_checkint (L, -1);
		lua_pop (L, 1);
	}
	
	else
		return luaL_argerror (L, index, "numeric fd or object with getfd() required");

	return ret;
}
/* }}} */

/* {{{ myepoll_ctl() */
static int myepoll_ctl (lua_State *L, int op, int fd)
{
	int epfd = myepoll_getepfd (L, 1);
	int flags, flag_i;
	struct epoll_event event;

	/* Grab the optional parameter with the event flags. */
	if (lua_gettop (L) < 3)
		flags = EPOLLIN;
	else
	{
		flags = 0;
		for (flag_i=3; flag_i<=lua_gettop (L); flag_i++)
			flags |= luaL_checkint (L, flag_i);
	}

	/* Register with epoll instance. */
	event.data.fd = fd;
	event.events = flags;
	if (epoll_ctl (epfd, op, fd, &event) == -1)
		return luaH_perror (L);

	return 0;
}
/* }}} */

/* {{{ myepoll_register() */
static int myepoll_register (lua_State *L)
{
	int fd = myepoll_getfd (L, 2);
	lua_getfield (L, 1, "fd_ref");
	lua_pushvalue (L, 2);
	lua_rawseti (L, -2, fd);
	lua_pop (L, 1);

	return myepoll_ctl (L, EPOLL_CTL_ADD, fd);
}
/* }}} */

/* {{{ myepoll_modify() */
static int myepoll_modify (lua_State *L)
{
	int fd = myepoll_getfd (L, 2);
	lua_getfield (L, 1, "fd_ref");
	lua_pushvalue (L, 2);
	lua_rawseti (L, -2, fd);
	lua_pop (L, 1);

	return myepoll_ctl (L, EPOLL_CTL_MOD, fd);
}
/* }}} */

/* {{{ myepoll_unregister() */
static int myepoll_unregister (lua_State *L)
{
	int fd = myepoll_getfd (L, 2);
	lua_getfield (L, 1, "fd_ref");
	lua_pushnil (L);
	lua_rawseti (L, -2, fd);
	lua_pop (L, 1);

	return myepoll_ctl (L, EPOLL_CTL_DEL, fd);
}
/* }}} */

/* {{{ myepoll_wait() */
static int myepoll_wait (lua_State *L)
{
	int epfd = myepoll_getepfd (L, 1);
	int timeout, n_events, maxevents;
	struct epoll_event *events;

	/* Grab the optional parameter with the timeout. */
	if (lua_isnoneornil (L, 2))
		timeout = -1;
	else if (lua_isnumber (L, 2))
		timeout = (int) ((double) lua_tonumber (L, 2) * 1000.0);
	else
		return luaL_argerror (L, 2, "optional numeric timeout argument");

	/* Grab the optional parameter with maxevents. */
	if (lua_isnoneornil (L, 3))
		maxevents = 1;
	else if (lua_isnumber (L, 3))
		maxevents = lua_tointeger (L, 3);
	else
		return luaL_argerror (L, 3, "optional numeric timeout argument");

	/* Wait for event. */
	struct epoll_event allevents[maxevents];
	n_events = epoll_wait (epfd, allevents, maxevents, timeout);
	if (n_events == -1)
		return luaH_perror (L);

	/* Gather the iterator function, invariant data, and control variable. */
	lua_getfield (L, 1, "wait_iter");
	events = (struct epoll_event *) lua_newuserdata (L, n_events * sizeof (struct epoll_event));
	memcpy (events, allevents, n_events * sizeof (struct epoll_event));
	lua_pushinteger (L, 1);

	return 3;
}
/* }}} */

/* {{{ myepoll_happened() */
static int myepoll_happened (lua_State *L)
{
	int events = luaL_checkint (L, 1);
	int specific = luaL_checkint (L, 2);

	lua_pushboolean (L, events & specific);
	return 1;
}
/* }}} */

/* {{{ luaopen_luah_ratchet_epoll() */
int luaopen_luah_ratchet_epoll (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", myepoll_init},
		{"del", myepoll_del},
		{"getfd", myepoll_getmyfd},
		{"register", myepoll_register},
		{"modify", myepoll_modify},
		{"unregister", myepoll_unregister},
		{"wait", myepoll_wait},
		{"happened", myepoll_happened},
		{NULL}
	};

	luaH_newclass (L, "luah.ratchet.epoll", meths);

	luaH_setfieldint (L, -1, "poll_err", EPOLLERR);
	luaH_setfieldint (L, -1, "poll_edge", EPOLLET);
	luaH_setfieldint (L, -1, "poll_hup", EPOLLHUP);
	luaH_setfieldint (L, -1, "poll_rdhup", EPOLLRDHUP);
	luaH_setfieldint (L, -1, "poll_read", EPOLLIN);
	luaH_setfieldint (L, -1, "poll_write", EPOLLOUT);
	luaH_setfieldint (L, -1, "poll_oneshot", EPOLLONESHOT);
	luaH_setfieldint (L, -1, "poll_pri", EPOLLPRI);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
