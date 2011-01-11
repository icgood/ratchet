/* Copyright (c) 2010 Ian C. Good
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <event.h>

#include "misc.h"

#define get_event_base(L, index) *(struct event_base **) luaL_checkudata (L, index, "ratchet_meta")
#define get_thread(L, index, s) luaL_checktype (L, index, LUA_TTHREAD); lua_State *s = lua_tothread (L, index)

/* {{{ setup_persistance_tables() */
static int setup_persistance_tables (lua_State *L)
{
	lua_newtable (L);

	lua_newtable (L);
	lua_pushliteral (L, "kv");
	lua_setfield (L, -2, "__mode");

	lua_newtable (L);
	lua_pushvalue (L, -2);
	lua_setmetatable (L, -2);
	lua_setfield (L, -3, "waited_for");

	lua_newtable (L);
	lua_pushvalue (L, -2);
	lua_setmetatable (L, -2);
	lua_setfield (L, -3, "not_started");

	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* {{{ set_thread_persist() */
static void set_thread_persist (lua_State *L, int index, int not_started, int waiting_index)
{
	lua_getfenv (L, 1);

	lua_pushvalue (L, index);
	lua_pushboolean (L, 1);
	lua_settable (L, -3);

	if (not_started)
	{
		lua_getfield (L, -1, "not_started");
		lua_pushvalue (L, index);
		lua_pushboolean (L, 1);
		lua_settable (L, -3);
		lua_pop (L, 1);
	}

	if (waiting_index)
	{
		lua_getfield (L, -1, "waited_for");
		lua_pushvalue (L, index);
		lua_pushvalue (L, waiting_index);
		lua_settable (L, -3);
		lua_pop (L, 1);
	}

	lua_pop (L, 1);
}
/* }}} */

/* {{{ end_thread_persist() */
static void end_thread_persist (lua_State *L, int index)
{
	lua_getfenv (L, 1);

	lua_getfield (L, -1, "waited_for");
	lua_pushvalue (L, index);
	lua_gettable (L, -2);
	lua_remove (L, -2);

	lua_pushvalue (L, index);
	lua_pushnil (L);
	lua_settable (L, -4);

	lua_remove (L, -2);
}
/* }}} */

/* {{{ event_triggered() */
static void event_triggered (int fd, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	lua_State *L = lua_tothread (L1, 1);

	/* Call the run_thread() helper method. */
	lua_getfield (L, 1, "run_thread");
	lua_pushvalue (L, 1);
	lua_settop (L1, 0);
	lua_pushthread (L1);
	lua_xmove (L1, L, 1);
	lua_call (L, 2, 0);
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ ratchet_new() */
static int ratchet_new (lua_State *L)
{
	struct event_base **new = (struct event_base **) lua_newuserdata (L, sizeof (struct event_base *));
	*new = event_base_new ();
	if (!*new)
		return luaL_error (L, "failed to create event_base structure.");

	luaL_getmetatable (L, "ratchet_meta");
	lua_setmetatable (L, -2);

	/* Set up persistance table. */
	setup_persistance_tables (L);
	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ ratchet_gc() */
static int ratchet_gc (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	event_base_free (e_b);

	return 0;
}
/* }}} */

/* {{{ ratchet_get_method() */
static int ratchet_get_method (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	lua_pushstring (L, event_base_get_method (e_b));
	return 1;
}
/* }}} */

/* {{{ ratchet_dispatch() */
static int ratchet_dispatch (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	if (event_base_loop (e_b, 0) < 0)
		return luaL_error (L, "libevent internal error.");
	return 0;
}
/* }}} */

/* {{{ ratchet_stop() */
static int ratchet_stop (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	if (event_base_loopbreak (e_b) < 0)
		return luaL_error (L, "libevent internal error.");
	return 0;
}
/* }}} */

/* {{{ ratchet_stop_after() */
static int ratchet_stop_after (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	struct timeval tv;
	gettimeval (L, 2, &tv);
	if (event_base_loopexit (e_b, &tv) < 0)
		return luaL_error (L, "libevent internal error.");
	return 0;
}
/* }}} */

/* {{{ ratchet_attach() */
static int ratchet_attach (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	luaL_checktype (L, 2, LUA_TFUNCTION);
	int nargs = lua_gettop (L) - 2;

	/* Set up new coroutine. */
	lua_State *L1 = lua_newthread (L);
	lua_insert (L, 2);
	lua_xmove (L, L1, nargs+1);

	set_thread_persist (L, 2 /* index of thread */, 1 /* not yet started */, 0 /* no thread waiting */);
	event_base_loopbreak (e_b);	/* So that new threads get started. */

	return 0;
}
/* }}} */

/* {{{ ratchet_attach_wait() */
static int ratchet_attach_wait (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	luaL_checktype (L, 2, LUA_TFUNCTION);
	if (lua_pushthread (L))
		return luaL_error (L, "attach_wait cannot be called from main thread");
	lua_insert (L, 2);
	int nargs = lua_gettop (L) - 3;

	/* Set up new coroutine. */
	lua_State *L1 = lua_newthread (L);
	lua_insert (L, 3);
	lua_xmove (L, L1, nargs+1);

	set_thread_persist (L, 3 /* index of thread */, 1 /* not yet started */, 2 /* index of waiting thread */);
	event_base_loopbreak (e_b);	/* So that new threads get started. */

	return lua_yield (L, 0);
}
/* }}} */

/* {{{ ratchet_loop() */
static int ratchet_loop (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);

	while (1)
	{
		/* Execute self:start_all_new(). */
		lua_getfield (L, 1, "start_all_new");
		lua_pushvalue (L, 1);
		lua_call (L, 1, 0);

		/* Call event loop, break if we're out of events. */
		int ret = event_base_loop (e_b, 0);
		if (ret < 0)
			return luaL_error (L, "libevent internal error");
		else if (ret > 0)
			break;
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_run_thread() */
static int ratchet_run_thread (lua_State *L)
{
	get_event_base (L, 1);
	get_thread (L, 2, L1);

	int nargs = lua_gettop (L1);
	if (lua_toboolean (L, 3))	/* not_started, first stack slot is the function */
		nargs--;
	int ret = lua_resume (L1, nargs);

	if (ret == 0)
	{
		/* Call self:finish_thread(). */
		lua_getfield (L, 1, "finish_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_call (L, 2, 0);
	}

	else if (ret == LUA_YIELD)
	{
		/* Call self:yield_thread(). */
		lua_getfield (L, 1, "yield_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_call (L, 2, 0);
	}

	else
		return lua_error (L1);

	return 0;
}
/* }}} */

/* {{{ ratchet_finish_thread() */
static int ratchet_finish_thread (lua_State *L)
{
	get_event_base (L, 1);
	get_thread (L, 2, L1);
	lua_settop (L, 2);

	int nrets = lua_gettop (L1);

	/* Remove the entry from the persistance table, get potential waiting thread. */
	end_thread_persist (L, 2);
	lua_State *L2 = lua_tothread (L, -1);

	/* Resume waiting thread, if necessary. */
	if (L2)
	{
		/* Set up the returns as args to the waiting thread. */
		lua_xmove (L1, L2, nrets);

		/* Execute the next waiting chunk of the waiting thread. */
		lua_getfield (L, 1, "run_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 3);
		lua_call (L, 2, 0);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_yield_thread() */
static int ratchet_yield_thread (lua_State *L)
{
	get_event_base (L, 1);
	get_thread (L, 2, L1);

	int nrets = lua_gettop (L1);

	if (nrets)
	{
		/* Get a wait_for_xxxxx method corresponding to first yield arg. */
		const char *yieldtype = lua_tostring (L1, 1);
		if (0 == strcmp (yieldtype, "write"))
			lua_getfield (L, 1, "wait_for_write");
		else if (0 == strcmp (yieldtype, "read"))
			lua_getfield (L, 1, "wait_for_read");
		else if (0 == strcmp (yieldtype, "signal"))
			lua_getfield (L, 1, "wait_for_signal");
		else if (0 == strcmp (yieldtype, "timeout"))
			lua_getfield (L, 1, "wait_for_timeout");
		else
			luaL_error (L, "unknown wait request [%s]", yieldtype);

		/* Call wait_for_xxxxx method with self, thread, arg1, arg2... */
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_xmove (L1, L, nrets-1);
		lua_call (L, nrets+1, 0);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_write() */
static int ratchet_wait_for_write (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int fd = luaL_checkint (L, 3);

	/* Set the main thread at index 1. */
	lua_settop (L1, 0);
	lua_pushthread (L);
	lua_xmove (L, L1, 1);

	/* Build event and queue it up. */
	struct event *ev = (struct event *) lua_newuserdata (L1, sizeof (struct event));
	event_set (ev, fd, EV_WRITE, event_triggered, L1);
	event_base_set (e_b, ev);
	event_add (ev, NULL);

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_read() */
static int ratchet_wait_for_read (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int fd = luaL_checkint (L, 3);

	/* Set the main thread at index 1. */
	lua_settop (L1, 0);
	lua_pushthread (L);
	lua_xmove (L, L1, 1);

	/* Build event and queue it up. */
	struct event *ev = (struct event *) lua_newuserdata (L1, sizeof (struct event));
	event_set (ev, fd, EV_READ, event_triggered, L1);
	event_base_set (e_b, ev);
	event_add (ev, NULL);

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_signal() */
static int ratchet_wait_for_signal (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int signal = luaL_checkint (L, 3);

	/* Set the main thread at index 1. */
	lua_settop (L1, 0);
	lua_pushthread (L);
	lua_xmove (L, L1, 1);

	/* Build event and queue it up. */
	struct event *ev = (struct event *) lua_newuserdata (L1, sizeof (struct event));
	signal_set (ev, signal, event_triggered, L1);
	event_base_set (e_b, ev);
	event_add (ev, NULL);

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_timeout() */
static int ratchet_wait_for_timeout (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	struct timeval tv;
	gettimeval (L, 3, &tv);

	/* Set the main thread at index 1. */
	lua_settop (L1, 0);
	lua_pushthread (L);
	lua_xmove (L, L1, 1);

	/* Build event and queue it up. */
	struct event *ev = (struct event *) lua_newuserdata (L1, sizeof (struct event));
	timeout_set (ev, event_triggered, L1);
	event_base_set (e_b, ev);
	event_add (ev, &tv);

	return 0;
}
/* }}} */

/* {{{ ratchet_start_all_new() */
static int ratchet_start_all_new (lua_State *L)
{
	get_event_base (L, 1);
	lua_settop (L, 1);
	
	lua_getfenv (L, 1);
	lua_getfield (L, 2, "not_started");
	for (lua_pushnil (L); lua_next (L, 3); lua_pop (L, 1))
	{
		/* Call self:run_thread(t). */
		lua_getfield (L, 1, "run_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 4);	/* The "key" is the thread to start. */
		lua_pushboolean (L, 1);	/* not_started flag. */
		lua_call (L, 3, 0);
	}

	/* Recreate the not_started table, to clear it. */
	lua_newtable (L);
	lua_getmetatable (L, 3);
	lua_setmetatable (L, -2);
	lua_setfield (L, 2, "not_started");
	lua_settop (L, 1);

	return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet() */
int luaopen_ratchet (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{"new", ratchet_new},
		{NULL}
	};

	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_method", ratchet_get_method},
		{"stop", ratchet_stop},
		{"stop_after", ratchet_stop_after},
		{"attach", ratchet_attach},
		{"attach_wait", ratchet_attach_wait},
		{"loop", ratchet_loop},
		/* Undocumented, helper methods. */
		{"run_thread", ratchet_run_thread},
		{"finish_thread", ratchet_finish_thread},
		{"yield_thread", ratchet_yield_thread},
		{"wait_for_write", ratchet_wait_for_write},
		{"wait_for_read", ratchet_wait_for_read},
		{"wait_for_signal", ratchet_wait_for_signal},
		{"wait_for_timeout", ratchet_wait_for_timeout},
		{"start_all_new", ratchet_start_all_new},
		{NULL}
	};

	static const luaL_Reg metameths[] = {
		{"__gc", ratchet_gc},
		{NULL}
	};

	luaL_newmetatable (L, "ratchet_meta");
	lua_newtable (L);
	luaI_openlib (L, NULL, meths, 0);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, metameths, 0);
	lua_pop (L, 1);

	luaI_openlib (L, "ratchet", funcs, 0);

	luaopen_ratchet_socket (L);
	lua_setfield (L, -2, "socket");
	luaopen_ratchet_uri (L);
	lua_setfield (L, -2, "uri");

	return 1;
}
/* }}} */

/* {{{ ratchet_version() */
const char *ratchet_version (void)
{
	return PACKAGE_VERSION;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
