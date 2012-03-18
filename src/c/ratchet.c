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

#define _GNU_SOURCE
#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <event2/event.h>
#include <netdb.h>
#include <string.h>

#include "ratchet.h"
#include "misc.h"

#define get_event_base(L, index) (*(struct event_base **) luaL_checkudata (L, index, "ratchet_meta"))
#define get_thread(L, index, s) luaL_checktype (L, index, LUA_TTHREAD); lua_State *s = lua_tothread (L, index)

const char *ratchet_version (void);

/* {{{ setup_persistance_tables() */
static int setup_persistance_tables (lua_State *L)
{
	lua_newtable (L);

	/* Storage space for all threads, for reference and to prevent garbage
	 * collection. */
	lua_newtable (L);
	lua_setfield (L, -2, "threads");

	/* Error handler from constructor. */
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "error_handler");

	/* Set up a weak-ref table to track what threads are not yet started. */
	lua_newtable (L);
	lua_newtable (L);
	lua_pushliteral (L, "kv");
	lua_setfield (L, -2, "__mode");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "ready");

	/* Set up a weak-key table to track what threads a thread is waiting on. */
	lua_newtable (L);
	lua_newtable (L);
	lua_pushliteral (L, "k");
	lua_setfield (L, -2, "__mode");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "waiting_on");

	/* Set up a weak-key table to hold alarm events for threads. */
	lua_newtable (L);
	lua_newtable (L);
	lua_pushliteral (L, "k");
	lua_setfield (L, -2, "__mode");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "alarm_events");

	/* Set up a weak-key table to hold alarm callbacks for threads. */
	lua_newtable (L);
	lua_newtable (L);
	lua_pushliteral (L, "k");
	lua_setfield (L, -2, "__mode");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "alarm_callbacks");

	/* Set up scratch-space for threads to store thread-scope data. */
	lua_newtable (L);
	lua_newtable (L);
	lua_pushliteral (L, "k");
	lua_setfield (L, -2, "__mode");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "thread_space");

	return 1;
}
/* }}} */

/* {{{ set_thread_persist() */
static void set_thread_persist (lua_State *L, int index)
{
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "threads");

	lua_pushvalue (L, index);
	lua_pushboolean (L, 1);
	lua_settable (L, -3);

	lua_pop (L, 2);
}
/* }}} */

/* {{{ set_thread_ready() */
static void set_thread_ready (lua_State *L, int index)
{
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "ready");

	lua_pushvalue (L, index);
	lua_pushboolean (L, 1);
	lua_settable (L, -3);

	lua_pop (L, 2);
}
/* }}} */

/* {{{ end_thread_persist() */
static void end_thread_persist (lua_State *L, int index)
{
	lua_getuservalue (L, 1);

	lua_State *L1 = lua_tothread (L, index);
	lua_settop (L1, 0);

	lua_getfield (L, -1, "threads");
	lua_pushvalue (L, index);
	lua_pushnil (L);
	lua_settable (L, -3);
	lua_pop (L, 1);

	lua_getfield (L, -1, "alarm_events");
	lua_pushvalue (L, index);
	lua_rawget (L, -2);
	if (!lua_isnil (L, -1))
	{
		struct event *ev = (struct event *) lua_touserdata (L, -1);
		event_del (ev);
	}
	lua_pop (L, 2);

	lua_getfield (L, -1, "waiting_on");
	for (lua_pushnil (L); lua_next (L, -2) != 0; lua_pop (L, 1))
	{
		lua_pushvalue (L, index);
		lua_pushnil (L);
		lua_rawset (L, -3);
	}
	lua_pop (L, 2);
}
/* }}} */

/* {{{ end_all_waiting_thread_events() */
static void end_all_waiting_thread_events (lua_State *L)
{
	lua_getfield (L, 2, "event");
	if (lua_isuserdata (L, -1))
	{
		struct event *ev = lua_touserdata (L, -1);
		event_del (ev);
	}
	lua_pop (L, 1);

	lua_getfield (L, 2, "event_list");
	if (lua_istable (L, -1))
	{
		int i;
		lua_pushnil (L);
		for (i=1; ; i++)
		{
			lua_pop (L, 1);
			lua_rawgeti (L, -1, i);
			if (lua_isuserdata (L, -1))
			{
				struct event *ev = lua_touserdata (L, -1);
				event_del (ev);
			}
			else if (lua_isnil (L, -1))
				break;
		}
		lua_pop (L, 1);
	}
	lua_pop (L, 1);
}
/* }}} */

/* {{{ get_fd_from_object() */
static int get_fd_from_object (lua_State *L, int index)
{
	lua_pushvalue (L, index);
	lua_getfield (L, -1, "get_fd");
	if (!lua_isfunction (L, -1))
		luaL_argerror (L, index, "object has no get_fd() method");
	lua_insert (L, -2);
	lua_call (L, 1, 1);

	int fd = lua_tointeger (L, -1);
	lua_pop (L, 1);
	return fd;
}
/* }}} */

/* {{{ get_timeout_from_object() */
static double get_timeout_from_object (lua_State *L, int index)
{
	lua_pushvalue (L, index);
	lua_getfield (L, -1, "get_timeout");
	if (!lua_isfunction (L, -1))
	{
		lua_pop (L, 2);
		return -1.0;
	}
	lua_insert (L, -2);
	lua_call (L, 1, 1);

	double timeout = (double) lua_tonumber (L, -1);
	lua_pop (L, 1);
	return timeout;
}
/* }}} */

/* {{{ event_triggered() */
static void event_triggered (int fd, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	if (!lua_isthread (L1, 1))
		luaL_error (L1, "ratchet internal error.");
	lua_State *L = lua_tothread (L1, 1);

	/* Call the run_thread() helper method. */
	lua_getfield (L, 1, "run_thread");
	lua_pushvalue (L, 1);
	lua_settop (L1, 0);
	lua_pushthread (L1);
	lua_xmove (L1, L, 1);
	lua_pushboolean (L1, !(event & EV_TIMEOUT));
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ signal_triggered() */
static void signal_triggered (int sig, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	if (!lua_isthread (L1, 1))
		luaL_error (L1, "ratchet internal error.");
	lua_State *L = lua_tothread (L1, 1);

	end_all_waiting_thread_events (L1);

	/* Call the run_thread() helper method. */
	lua_getfield (L, 1, "run_thread");
	lua_pushvalue (L, 1);
	lua_settop (L1, 0);
	lua_pushthread (L1);
	lua_xmove (L1, L, 1);
	lua_pushboolean (L1, !(event & EV_TIMEOUT));
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ timeout_triggered() */
static void timeout_triggered (int fd, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	if (!lua_isthread (L1, 1))
		luaL_error (L1, "ratchet internal error.");
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

/* {{{ alarm_triggered() */
static void alarm_triggered (int fd, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	if (!lua_isthread (L1, 1))
		luaL_error (L1, "ratchet internal error.");
	lua_State *L = lua_tothread (L1, 1);

	/* Call the run_thread() helper method. */
	lua_getfield (L, 1, "alarm_thread");
	lua_pushvalue (L, 1);
	lua_pushthread (L1);
	lua_xmove (L1, L, 1);
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ multi_event_triggered() */
static void multi_event_triggered (int fd, short event, void *arg)
{
	lua_State *L1 = (lua_State *) arg;
	if (!lua_isthread (L1, 1))
		luaL_error (L1, "ratchet internal error.");
	lua_State *L = lua_tothread (L1, 1);

	end_all_waiting_thread_events (L1);

	/* Figure out which object was triggered based on fd. */
	lua_rawgeti (L1, 3, fd);
	lua_replace (L1, 1);
	lua_settop (L1, 1);

	/* Call the run_thread() helper method. */
	lua_getfield (L, 1, "run_thread");
	lua_pushvalue (L, 1);
	lua_pushthread (L1);
	lua_xmove (L1, L, 1);
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ handle_thread_error() */
static void handle_thread_error (lua_State *L, int thread_i)
{
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "error_handler");
	if (!lua_isnil (L, -1))
	{
		lua_pushvalue (L, -3);
		lua_pushvalue (L, thread_i);
		lua_call (L, 2, 0);
		lua_pop (L, 1);
	}
	else
	{
		lua_pop (L, 2);
		lua_error (L);
	}
}
/* }}} */

/* ---- ratchet Functions --------------------------------------------------- */

/* {{{ ratchet_new() */
static int ratchet_new (lua_State *L)
{
	lua_settop (L, 2);

	struct event_base **new = (struct event_base **) lua_newuserdata (L, sizeof (struct event_base *));
	*new = event_base_new ();
	if (!*new)
		return luaL_error (L, "Failed to create event_base structure.");

	luaL_getmetatable (L, "ratchet_meta");
	lua_setmetatable (L, -2);

	/* Set up persistance table. */
	setup_persistance_tables (L);
	lua_setuservalue (L, -2);

	lua_insert (L, 1);

	/* Attach the first argument as an entry thread. */
	lua_State *L1 = lua_newthread (L);
	lua_pushvalue (L, 2);
	lua_xmove (L, L1, 1);
	lua_replace (L, 2);
	set_thread_persist (L, 2 /* index of thread */);
	set_thread_ready (L, 2 /* index of thread */);
	lua_settop (L, 1);

	return 1;
}
/* }}} */

/* {{{ ratchet_stackdump() */
static int ratchet_stackdump (lua_State *L)
{
	stackdump (L);
	return 0;
}
/* }}} */

/* ---- ratchet Methods ----------------------------------------------------- */

/* {{{ ratchet_gc() */
static int ratchet_gc (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	event_base_free (e_b);

	return 0;
}
/* }}} */

/* {{{ ratchet_event_gc() */
static int ratchet_event_gc (lua_State *L)
{
	struct event *ev = (struct event *) luaL_checkudata (L, 1, "ratchet_event_internal_meta");
	event_del (ev);

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

/* {{{ ratchet_get_num_threads() */
static int ratchet_get_num_threads (lua_State *L)
{
	(void) get_event_base (L, 1);
	int i;

	lua_settop (L, 1);
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "threads");
	lua_pushnil (L);
	for (i=1; lua_next (L, 3) != 0; i++)
		lua_pop (L, 1);

	lua_pushinteger (L, i-1);
	return 1;
}
/* }}} */

/* {{{ ratchet_loop_once() */
static int ratchet_loop_once (lua_State *L)
{
	struct event_base *e_b = get_event_base (L, 1);
	int flags = (lua_toboolean (L, 2) ? EVLOOP_NONBLOCK : EVLOOP_ONCE);

	lua_settop (L, 1);

	/* Execute self:start_threads_ready(). */
	lua_getfield (L, 1, "start_threads_ready");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);
	if (lua_toboolean (L, -1))
	{
		lua_pushboolean (L, 1);
		return 1;
	}
	lua_settop (L, 1);

	/* Execute self:start_threads_waiting(). */
	lua_getfield (L, 1, "start_threads_done_waiting");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);
	if (lua_toboolean (L, -1))
	{
		lua_pushboolean (L, 1);
		return 1;
	}
	lua_settop (L, 1);

	/* Return false if we're out of threads. */
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "threads");
	lua_pushnil (L);
	if (lua_next (L, -2) == 0)
	{
		lua_pushboolean (L, 0);
		return 1;
	}
	lua_settop (L, 1);

	/* Handle one iteration of event processing. */
	int ret = event_base_loop (e_b, flags);
	if (ret < 0)
		return luaL_error (L, "libevent internal error.");
	else if (ret > 0)
		return ratchet_error_str (L, "ratchet.loop_once()", "DEADLOCK", "Non-IO deadlock detected.");

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ ratchet_loop() */
static int ratchet_loop (lua_State *L)
{
	(void) get_event_base (L, 1);

	while (1)
	{
		lua_getfield (L, 1, "loop_once");
		lua_pushvalue (L, 1);
		lua_call (L, 1, 1);
		if (!lua_toboolean (L, -1))
			break;
		lua_pop (L, 1);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_get_space() */
static int ratchet_get_space (lua_State *L)
{
	(void) get_event_base (L, 1);

	lua_settop (L, 3);

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "thread_space");
	lua_pushvalue (L, 2);
	lua_rawget (L, -2);

	if (lua_isnil (L, -1))
	{
		lua_pop (L, 1);

		if (lua_isnil (L, 3))
		{
			lua_newtable (L);
			lua_pushvalue (L, 2);
			lua_pushvalue (L, -2);
			lua_rawset (L, -4);
		}
		else
		{
			lua_pushvalue (L, 2);
			lua_pushvalue (L, 3);
			lua_rawset (L, -3);
			lua_pushvalue (L, 3);
		}
	}

	return 1;
}
/* }}} */

/* {{{ ratchet_start_threads_ready() */
static int ratchet_start_threads_ready (lua_State *L)
{
	int some_ready = 0;

	(void) get_event_base (L, 1);
	lua_settop (L, 1);
	
	lua_getuservalue (L, 1);
	lua_getfield (L, 2, "ready");
	lua_pushnil (L);
	while (1)
	{
		if (0 == lua_next (L, 3))
			break;
		some_ready = 1;

		/* Remove entry from ready. */
		lua_pushvalue (L, 4);
		lua_pushnil (L);
		lua_settable (L, 3);

		/* Call self:run_thread(t). */
		lua_getfield (L, 1, "run_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 4);	/* The "key" is the thread to start. */
		lua_call (L, 2, 0);

		lua_pop (L, 2);
		lua_pushnil (L);
	}

	lua_pushboolean (L, some_ready);
	return 1;
}
/* }}} */

/* {{{ ratchet_start_threads_done_waiting() */
static int ratchet_start_threads_done_waiting (lua_State *L)
{
	int some_ready = 0;

	(void) get_event_base (L, 1);
	lua_settop (L, 1);

	lua_getuservalue (L, 1);
	lua_getfield (L, 2, "waiting_on");
	for (lua_pushnil (L); lua_next (L, 3); lua_pop (L, 1))
	{
		/* Check for empty value table. */
		lua_pushnil (L);
		if (lua_next (L, 5) == 0)
		{
			some_ready = 1;

			/* Remove entry from waiting_on. */
			lua_pushvalue (L, 3);
			lua_pushvalue (L, 4);
			lua_pushnil (L);
			lua_settable (L, -3);
			lua_pop (L, 1);

			/* Call self:run_thread(t). */
			lua_getfield (L, 1, "run_thread");
			lua_pushvalue (L, 1);
			lua_pushvalue (L, 4);	/* The "key" is the thread to start. */
			lua_call (L, 2, 0);
		}
		else
			lua_pop (L, 2);
	}

	lua_pushboolean (L, some_ready);
	return 1;
}
/* }}} */

/* {{{ ratchet_alarm_thread() */
static int ratchet_alarm_thread (lua_State *L)
{
	(void) get_event_base (L, 1);
	get_thread (L, 2, L1);

	int ret = LUA_OK;
	int ctx = 0;
	lua_getctx (L, &ctx);

	if (ctx == 0)
	{
		if (LUA_YIELD == lua_status (L1))
			end_all_waiting_thread_events (L1);
		lua_settop (L1, 0);

		lua_getuservalue (L, 1);
		lua_getfield (L, -1, "alarm_callbacks");
		lua_pushvalue (L, 2);
		lua_rawget (L, -2);
		if (!lua_isnil (L, -1))
		{
			lua_xmove (L, L1, 1);
			ret = lua_pcall (L1, 0, 0, 0);
		}
		else
			lua_pop (L, 1);
	}

	lua_settop (L, 2);

	if (ret == 0)
	{
		ratchet_error_push_constructor (L1);
		lua_pushliteral (L1, "Thread alarm!");
		lua_pushliteral (L1, "ALARM");
		lua_pushliteral (L1, "ratchet.thread.alarm()");
		lua_call (L1, 3, 1);
	}

	lua_xmove (L1, L, 1);
	end_thread_persist (L, 2);

	handle_thread_error (L, 2);

	return 0;
}
/* }}} */

/* {{{ ratchet_run_thread() */
static int ratchet_run_thread (lua_State *L)
{
	(void) get_event_base (L, 1);
	get_thread (L, 2, L1);

	int nargs, ret;

restart_thread:
	nargs = lua_gettop (L1);
	if (lua_status (L1) != LUA_YIELD)
		nargs--;
	ret = lua_resume (L1, L, nargs);

	if (ret == LUA_OK)
		end_thread_persist (L, 2);	/* Remove the entry from the persistance tables. */

	else if (ret == LUA_YIELD)
	{
		/* Check for a "get" call. */
		if (lua_islightuserdata (L1, 1) && RATCHET_YIELD_GET == lua_touserdata (L1, 1))
		{
			lua_pushvalue (L, 1);
			lua_xmove (L, L1, 1);
			lua_replace (L1, 1);
			lua_settop (L1, 1);

			goto restart_thread;
		}

		/* Call self:yield_thread(). */
		lua_getfield (L, 1, "yield_thread");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_call (L, 2, 0);
	}

	/* Handle the error from the thread. */
	else
	{
		lua_xmove (L1, L, 1);
		end_all_waiting_thread_events (L1);
		end_thread_persist (L, 2);

		handle_thread_error (L, 2);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_yield_thread() */
static int ratchet_yield_thread (lua_State *L)
{
	(void) get_event_base (L, 1);
	get_thread (L, 2, L1);

	int nrets = lua_gettop (L1);

	if (!lua_islightuserdata (L1, 1))
		return luaL_error (L, "Illegal yield of a ratchet thread.");

	/* Get a wait_for_xxxxx method corresponding to first yield arg. */
	void *yield_type = lua_touserdata (L1, 1);

	if (RATCHET_YIELD_WRITE == yield_type)
		lua_getfield (L, 1, "wait_for_write");

	else if (RATCHET_YIELD_READ == yield_type)
		lua_getfield (L, 1, "wait_for_read");

	else if (RATCHET_YIELD_SIGNAL == yield_type)
		lua_getfield (L, 1, "wait_for_signal");

	else if (RATCHET_YIELD_TIMEOUT == yield_type)
		lua_getfield (L, 1, "wait_for_timeout");

	else if (RATCHET_YIELD_MULTIRW == yield_type)
		lua_getfield (L, 1, "wait_for_multi");

	else
	{
		lua_pushnil (L);
		lua_settop (L1, 0);
		lua_newtable (L1);
	}

	if (lua_isfunction (L, -1))
	{
		/* Call wait_for_xxxxx method with self, thread, arg1, arg2... */
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_xmove (L1, L, nrets-1);
		lua_settop (L1, 0);
		lua_call (L, nrets+1, 0);
	}

	/* Leave the main thread at index 1 of the child thread. */
	lua_pushthread (L);
	lua_xmove (L, L1, 1);
	lua_insert (L1, 1);

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_write() */
static int ratchet_wait_for_write (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int fd = get_fd_from_object (L, 3);
	double timeout = get_timeout_from_object (L, 3);

	/* Cleanup table for kill()ing the thread. */
	lua_createtable (L1, 0, 1);

	if (fd < 0)
		return ratchet_error_str (L, NULL, "EBADF", "Invalid file descriptor: %d", fd);

	/* Build timeout data. */
	struct timeval tv;
	int use_tv = gettimeval (timeout, &tv);

	/* Build event. */
	struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
	luaL_getmetatable (L1, "ratchet_event_internal_meta");
	lua_setmetatable (L1, -2);

	/* Queue up the event. */
	event_assign (ev, e_b, fd, EV_WRITE, event_triggered, L1);
	event_add (ev, (use_tv ? &tv : NULL));

	lua_setfield (L1, -2, "event");

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_read() */
static int ratchet_wait_for_read (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int fd = get_fd_from_object (L, 3);
	double timeout = get_timeout_from_object (L, 3);

	/* Cleanup table for kill()ing the thread. */
	lua_createtable (L1, 0, 1);

	if (fd < 0)
		return ratchet_error_str (L, NULL, "EBADF", "Invalid file descriptor: %d", fd);

	/* Build timeout data. */
	struct timeval tv;
	int use_tv = gettimeval (timeout, &tv);

	/* Build event. */
	struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
	luaL_getmetatable (L1, "ratchet_event_internal_meta");
	lua_setmetatable (L1, -2);

	/* Queue up the event. */
	event_assign (ev, e_b, fd, EV_READ, event_triggered, L1);
	event_add (ev, (use_tv ? &tv : NULL));

	lua_setfield (L1, -2, "event");

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_signal() */
static int ratchet_wait_for_signal (lua_State *L)
{
	/* Gather args into usable data. */
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	int sig = (int) lua_tointeger (L, 3);
	struct timeval tv;
	int use_tv = gettimeval_opt (L, 4, &tv);

	/* Cleanup table for kill()ing the thread. */
	lua_createtable (L1, 0, 1);
	lua_newtable (L1);

	/* Build signal event. */
	struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
	luaL_getmetatable (L1, "ratchet_event_internal_meta");
	lua_setmetatable (L1, -2);
	lua_rawseti (L1, -2, 1);

	/* Queue up the signal event. */
	event_assign (ev, e_b, sig, EV_SIGNAL, signal_triggered, L1);
	event_add (ev, NULL);

	if (use_tv)
	{
		/* Build timeout event. */
		struct event *timeout = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
		luaL_getmetatable (L1, "ratchet_event_internal_meta");
		lua_setmetatable (L1, -2);
		lua_rawseti (L1, -2, 2);

		/* Queue up the timeout event. */
		evtimer_assign (timeout, e_b, signal_triggered, L1);
		evtimer_add (timeout, &tv);
	}

	lua_setfield (L1, -2, "event_list");

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
	gettimeval_arg (L, 3, &tv);

	/* Cleanup table for kill()ing the thread. */
	lua_createtable (L1, 0, 1);

	/* Build event and queue it up. */
	struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
	luaL_getmetatable (L1, "ratchet_event_internal_meta");
	lua_setmetatable (L1, -2);
	lua_setfield (L1, -2, "event");

	evtimer_assign (ev, e_b, timeout_triggered, L1);
	evtimer_add (ev, &tv);

	return 0;
}
/* }}} */

/* {{{ ratchet_wait_for_multi() */
static int ratchet_wait_for_multi (lua_State *L)
{
	/* Gather args into usable data. */
	lua_settop (L, 5);
	struct event_base *e_b = get_event_base (L, 1);
	get_thread (L, 2, L1);
	luaL_checktype (L, 3, LUA_TTABLE);
	if (!lua_isnoneornil (L, 4))
		luaL_checktype (L, 4, LUA_TTABLE);
	else
	{
		lua_newtable (L);
		lua_replace (L, 4);
	}
	struct timeval tv;
	int use_tv = gettimeval_opt (L, 5, &tv);

	int i, nread = lua_rawlen (L, 3), nwrite = lua_rawlen (L, 4);

	lua_createtable (L1, 0, 2);

	/* Create timeout event. */
	struct event *timeout = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
	luaL_getmetatable (L1, "ratchet_event_internal_meta");
	lua_setmetatable (L1, -2);
	lua_setfield (L1, -2, "event");

	/* Queue up timeout event. */
	evtimer_assign (timeout, e_b, timeout_triggered, L1);
	if (use_tv)
		evtimer_add (timeout, &tv);

	lua_newtable (L1);
	lua_createtable (L1, nread+nwrite, 0);

	for (i=1; i<=nread; i++)
	{
		lua_rawgeti (L, 3, i);
		int fd = get_fd_from_object (L, -1);

		lua_pushinteger (L1, fd);
		lua_xmove (L, L1, 1);
		lua_rawset (L1, -4);

		/* Build event. */
		struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
		luaL_getmetatable (L1, "ratchet_event_internal_meta");
		lua_setmetatable (L1, -2);
		lua_rawseti (L1, -2, i);

		/* Queue up the event. */
		event_assign (ev, e_b, fd, EV_READ, multi_event_triggered, L1);
		event_add (ev, NULL);
	}

	for (i=1; i<=nwrite; i++)
	{
		lua_rawgeti (L, 4, i);
		int fd = get_fd_from_object (L, -1);

		lua_pushinteger (L1, fd);
		lua_xmove (L, L1, 1);
		lua_rawset (L1, -4);

		/* Build event. */
		struct event *ev = (struct event *) lua_newuserdata (L1, event_get_struct_event_size ());
		luaL_getmetatable (L1, "ratchet_event_internal_meta");
		lua_setmetatable (L1, -2);
		lua_rawseti (L1, -2, nread+i);

		/* Queue up the event. */
		event_assign (ev, e_b, fd, EV_WRITE, multi_event_triggered, L1);
		event_add (ev, NULL);
	}

	lua_setfield (L1, -3, "event_list");

	return 0;
}
/* }}} */

/* ---- ratchet.thread Functions -------------------------------------------- */

/* {{{ ratchet_attach() */
static int ratchet_attach (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_attach);
	}

	lua_insert (L, 1);
	(void) get_event_base (L, 1);

	luaL_checkany (L, 2);	/* Function or callable object. */
	int nargs = lua_gettop (L) - 2;

	/* Set up new coroutine. */
	lua_State *L1 = lua_newthread (L);
	lua_insert (L, 2);
	lua_xmove (L, L1, nargs+1);

	set_thread_persist (L, 2 /* index of thread */);
	set_thread_ready (L, 2 /* index of thread */);

	lua_pushvalue (L, 2);
	return 1;
}
/* }}} */

/* {{{ ratchet_block_on() */
static int ratchet_block_on (lua_State *L)
{
	lua_pushlightuserdata (L, RATCHET_YIELD_MULTIRW);
	lua_insert (L, 1);

	int nargs = lua_gettop (L);
	return lua_yield (L, nargs);
}
/* }}} */

/* {{{ ratchet_sigwait() */
static int ratchet_sigwait (lua_State *L)
{
	lua_pushlightuserdata (L, RATCHET_YIELD_SIGNAL);
	int sig = get_signal (L, 1, -1);
	if (-1 == sig)
		return luaL_argerror (L, 1, "Invalid signal.");
	lua_pushinteger (L, sig);
	return lua_yield (L, 2);
}
/* }}} */

/* {{{ ratchet_wait_all() */
static int ratchet_wait_all (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_wait_all);
	}

	lua_insert (L, 1);
	(void) get_event_base (L, 1);

	int i;
	luaL_checktype (L, 2, LUA_TTABLE);
	lua_settop (L, 2);
	if (lua_pushthread (L))
		return luaL_error (L, "ratchet.thread.wait_all() cannot be called from main thread.");
	lua_pop (L, 1);

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "waiting_on");

	/* Iterate through table of children, building duplicate with
	 * the child threads as weak keys. */
	lua_newtable (L);
	lua_getmetatable (L, 4);
	lua_setmetatable (L, -2);
	for (i=1; ; i++)
	{
		lua_rawgeti (L, 2, i);
		if (lua_isnil (L, -1))
		{
			lua_pop (L, 1);
			break;
		}

		if (!lua_isthread (L, -1))
			return luaL_error (L, "Table item %d is not a thread.", i);

		lua_State *L1 = lua_tothread (L, -1);

		/* Skip if thread is finished or errored out. */
		if (lua_status (L1) == LUA_OK && lua_gettop (L1) == 0)
			continue;
		else if (lua_status (L1) != LUA_OK && lua_status (L1) != LUA_YIELD)
			continue;

		lua_pushboolean (L, 1);
		lua_rawset (L, 5);
	}

	lua_pushthread (L);
	lua_pushvalue (L, 5);
	lua_settable (L, 4);
	lua_settop (L, 2);

	lua_pushlightuserdata (L, RATCHET_YIELD_WAITALL);
	return lua_yield (L, 1);
}
/* }}} */

/* {{{ ratchet_thread_space() */
static int ratchet_thread_space (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_thread_space);
	}

	lua_insert (L, 1);
	lua_settop (L, 2);

	if (lua_pushthread (L))
		return luaL_error (L, "ratchet.thread.space() cannot be called from main thread.");
	lua_insert (L, 2);
	return ratchet_get_space (L);
}
/* }}} */

/* {{{ ratchet_running_thread() */
static int ratchet_running_thread (lua_State *L)
{
	if (lua_pushthread (L))
		lua_pushnil (L);
	return 1;
}
/* }}} */

/* {{{ ratchet_timer() */
static int ratchet_timer (lua_State *L)
{
	if (lua_pushthread (L))
		return luaL_error (L, "ratchet.thread.timer() cannot be called from main thread.");
	lua_pop (L, 1);

	int nargs = lua_gettop (L);
	lua_pushlightuserdata (L, RATCHET_YIELD_TIMEOUT);
	lua_insert (L, 1);

	return lua_yield (L, nargs+1);
}
/* }}} */

/* {{{ ratchet_pause() */
static int ratchet_pause (lua_State *L)
{
	if (lua_pushthread (L))
		return luaL_error (L, "ratchet.thread.pause() cannot be called from main thread.");
	lua_pop (L, 1);

	lua_pushlightuserdata (L, RATCHET_YIELD_PAUSE);
	return lua_yield (L, 1);
}
/* }}} */

/* {{{ ratchet_unpause() */
static int ratchet_unpause (lua_State *L)
{
	get_thread (L, 1, L1);

	/* Get the ratchet object from the thread stack and put it at index 1. */
	get_thread (L1, 1, L2);
	lua_pushvalue (L2, 1);
	lua_xmove (L2, L, 1);
	lua_insert (L, 1);
	lua_settop (L1, 0);

	/* Make sure it's unpause-able. */
	if (lua_status (L1) != LUA_YIELD)
		return luaL_error (L, "Thread is not yielding, cannot unpause.");

	/* Set up the extra arguments as return values from pause(). */
	int nargs = lua_gettop (L) - 2;
	lua_xmove (L, L1, nargs);

	/* Add the thread to the ready table so it gets resumed. */
	set_thread_ready (L, 2 /* index of thread */);

	return 0;
}
/* }}} */

/* {{{ ratchet_kill() */
static int ratchet_kill (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_kill);
	}

	lua_insert (L, 1);
	(void) get_event_base (L, 1);

	lua_State *L1 = lua_tothread (L, 2);
	if (LUA_YIELD == lua_status (L1))
		end_all_waiting_thread_events (L1);

	end_thread_persist (L, 2);

	return 0;
}
/* }}} */

/* {{{ ratchet_kill_all() */
static int ratchet_kill_all (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_kill_all);
	}

	lua_insert (L, 1);
	(void) get_event_base (L, 1);

	lua_settop (L, 2);

	int i;
	lua_State *L1;
	for (i=1; ; i++)
	{
		lua_rawgeti (L, 2, i);
		if (lua_isnil (L, 3))
			break;

		L1 = lua_tothread (L, 3);
		if (LUA_YIELD == lua_status (L1))
			end_all_waiting_thread_events (L1);

		end_thread_persist (L, 3);
		lua_settop (L, 2);
	}

	return 0;
}
/* }}} */

/* {{{ ratchet_alarm() */
static int ratchet_alarm (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_GET);
		return lua_yieldk (L, 1, ctx, ratchet_alarm);
	}

	lua_insert (L, 1);
	lua_settop (L, 3);
	if (lua_pushthread (L))
		return luaL_error (L, "ratchet.thread.alarm() cannot be called from main thread.");

	struct event_base *e_b = get_event_base (L, 1);
	struct timeval tv;
	gettimeval_arg (L, 2, &tv);

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "alarm_events");

	/* Clean up and delete any existing alarm. */
	lua_pushvalue (L, 4);
	lua_rawget (L, -2);
	if (!lua_isnil (L, -1))
	{
		struct event *ev = (struct event *) lua_touserdata (L, -1);
		event_del (ev);
	}
	lua_pop (L, 1);

	/* Setup new alarm event. */
	lua_pushvalue (L, 4);
	struct event *ev = (struct event *) lua_newuserdata (L, event_get_struct_event_size ());
	evtimer_assign (ev, e_b, alarm_triggered, L);
	evtimer_add (ev, &tv);
	lua_rawset (L, -3);

	/* Set the callback, if given. */
	lua_getfield (L, 5, "alarm_callbacks");
	lua_pushvalue (L, 4);
	lua_pushvalue (L, 3);
	lua_rawset (L, -3);

	return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet() */
int luaopen_ratchet (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{"new", ratchet_new},
		{"stackdump", ratchet_stackdump},
		{NULL}
	};

	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_method", ratchet_get_method},
		{"get_num_threads", ratchet_get_num_threads},
		{"loop", ratchet_loop},
		{"loop_once", ratchet_loop_once},
		{"get_space", ratchet_get_space},
		/* Undocumented, helper methods. */
		{"alarm_thread", ratchet_alarm_thread},
		{"run_thread", ratchet_run_thread},
		{"yield_thread", ratchet_yield_thread},
		{"wait_for_write", ratchet_wait_for_write},
		{"wait_for_read", ratchet_wait_for_read},
		{"wait_for_signal", ratchet_wait_for_signal},
		{"wait_for_timeout", ratchet_wait_for_timeout},
		{"wait_for_multi", ratchet_wait_for_multi},
		{"start_threads_ready", ratchet_start_threads_ready},
		{"start_threads_done_waiting", ratchet_start_threads_done_waiting},
		{NULL}
	};

	static const luaL_Reg thread_funcs[] = {
		{"attach", ratchet_attach},
		{"kill", ratchet_kill},
		{"kill_all", ratchet_kill_all},
		{"pause", ratchet_pause},
		{"unpause", ratchet_unpause},
		{"self", ratchet_running_thread},
		{"block_on", ratchet_block_on},
		{"sigwait", ratchet_sigwait},
		{"wait_all", ratchet_wait_all},
		{"space", ratchet_thread_space},
		{"timer", ratchet_timer},
		{"alarm", ratchet_alarm},
		{NULL}
	};

	static const luaL_Reg metameths[] = {
		{"__gc", ratchet_gc},
		{NULL}
	};

	static const luaL_Reg eventmetameths[] = {
		{"__gc", ratchet_event_gc},
		{NULL}
	};

	luaL_newmetatable (L, "ratchet_event_internal_meta");
	luaL_setfuncs (L, eventmetameths, 0);
	lua_pop (L, 1);

	luaL_newmetatable (L, "ratchet_meta");
	lua_newtable (L);
	luaL_setfuncs (L, meths, 0);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setglobal (L, "ratchet");

	luaL_newlib (L, thread_funcs);
	lua_setfield (L, -2, "thread");

	luaL_requiref (L, "ratchet.error", luaopen_ratchet_error, 0);
	lua_setfield (L, -2, "error");

	luaL_requiref (L, "ratchet.exec", luaopen_ratchet_exec, 0);
	lua_setfield (L, -2, "exec");

#if HAVE_SOCKET
	luaL_requiref (L, "ratchet.socket", luaopen_ratchet_socket, 0);
	lua_setfield (L, -2, "socket");
#endif
#if HAVE_OPENSSL
	luaL_requiref (L, "ratchet.ssl", luaopen_ratchet_ssl, 0);
	lua_setfield (L, -2, "ssl");
#endif
#if HAVE_ZMQ
	luaL_requiref (L, "ratchet.zmqsocket", luaopen_ratchet_zmqsocket, 0);
	lua_setfield (L, -2, "zmqsocket");
#endif
#if HAVE_DNS
	luaL_requiref (L, "ratchet.dns", luaopen_ratchet_dns, 0);
	lua_setfield (L, -2, "dns");
#endif
#if HAVE_TIMERFD
	luaL_requiref (L, "ratchet.timerfd", luaopen_ratchet_timerfd, 0);
	lua_setfield (L, -2, "timerfd");
#endif

	lua_pushstring (L, PACKAGE_VERSION);
	lua_setfield (L, -2, "version");

	/* Save class to registry. */
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_class");

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
