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

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <string.h>
#include <errno.h>

#include "ratchet.h"
#include "misc.h"
#include "error.h"

#define timerfd_fd(L, i) (int *) luaL_checkudata (L, i, "ratchet_timerfd_meta")

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rtfd_new() */
static int rtfd_new (lua_State *L)
{
	static const char *lst[] = {"monotonic", "realtime", NULL};
	static const int howlst[] = {CLOCK_MONOTONIC, CLOCK_REALTIME};
	int how = howlst[luaL_checkoption (L, 1, "monotonic", lst)];

	int *tfd = (int *) lua_newuserdata (L, sizeof (int));
	*tfd = timerfd_create (how, 0);
	if (*tfd < 0)
		return rerror_perror (L, "ratchet.timerfd.new()", "timerfd_create");

	if (set_nonblocking (*tfd) < 0)
		return rerror_perror (L, "ratchet.timerfd.new()", NULL);

	luaL_getmetatable (L, "ratchet_timerfd_meta");
	lua_setmetatable (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rtfd_gc() */
static int rtfd_gc (lua_State *L)
{
	int fd = *timerfd_fd (L, 1);
	if (fd >= 0)
		close (fd);

	return 0;
}
/* }}} */

/* {{{ rtfd_get_fd() */
static int rtfd_get_fd (lua_State *L)
{
	int fd = *timerfd_fd (L, 1);
	lua_pushinteger (L, fd);
	return 1;
}
/* }}} */

/* {{{ rtfd_settime() */
static int rtfd_settime (lua_State *L)
{
	int fd = *timerfd_fd (L, 1);
	struct itimerspec new_value, old_value;
	memset (&new_value, 0, sizeof (new_value));
	gettimespec_opt (L, 2, &new_value.it_value);
	gettimespec_opt (L, 3, &new_value.it_interval);
	static const char *lst[] = {"relative", "absolute", NULL};
	static const int howlst[] = {0, TFD_TIMER_ABSTIME};
	int how = howlst[luaL_checkoption (L, 4, "relative", lst)];

	int ret = timerfd_settime (fd, how, &new_value, &old_value);
	if (ret == -1)
		return rerror_perror (L, "ratchet.timerfd.settime()", "timerfd_settime");

	lua_pushnumber (L, (lua_Number) fromtimespec (&old_value.it_value));
	lua_pushnumber (L, (lua_Number) fromtimespec (&old_value.it_interval));

	return 2;
}
/* }}} */

/* {{{ rtfd_gettime() */
static int rtfd_gettime (lua_State *L)
{
	int fd = *timerfd_fd (L, 1);
	struct itimerspec curr_value;

	int ret = timerfd_gettime (fd, &curr_value);
	if (ret == -1)
		return rerror_perror (L, "ratchet.timerfd.gettime()", "timerfd_gettime");

	lua_pushnumber (L, (lua_Number) fromtimespec (&curr_value.it_value));
	lua_pushnumber (L, (lua_Number) fromtimespec (&curr_value.it_interval));

	return 2;
}
/* }}} */

/* {{{ rtfd_close() */
static int rtfd_close (lua_State *L)
{
	int *fd = timerfd_fd (L, 1);
	if (*fd < 0)
		return 0;

	int ret = close (*fd);
	if (ret == -1)
		return rerror_perror (L, "ratchet.timerfd.close()", "close");
	*fd = -1;

	lua_pushboolean (L, 1);
	return 1;
}
/* }}} */

/* {{{ rtfd_read() */
static int rtfd_read (lua_State *L)
{
	int ctx = 0;
	if (LUA_OK == lua_getctx (L, &ctx))
	{
		lua_pushliteral (L, "read");
		lua_pushvalue (L, 1);

		return lua_yieldk (L, 2, 0, rtfd_read);
	}

	lua_settop (L, 1);
	int tfd = *timerfd_fd (L, 1);
	uint64_t fires;
	ssize_t ret;

	ret = read (tfd, &fires, sizeof (fires));
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_getfield (L, 1, "read");
			lua_pushvalue (L, 1);
			lua_call (L, 1, 2);
			return 2;
		}

		else
			return rerror_perror (L, "ratchet.timerfd.read()", "read");
	}

	lua_pushnumber (L, (lua_Number) fires);

	return 1;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_timerfd() */
int luaopen_ratchet_timerfd (lua_State *L)
{
	/* Static functions in the ratchet.timerfd namespace. */
	static const luaL_Reg funcs[] = {
		{"new", rtfd_new},
		{NULL}
	};

	/* Meta-methods for ratchet.timerfd object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rtfd_gc},
		{NULL}
	};

	/* Methods in the ratchet.timerfd class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_fd", rtfd_get_fd},
		{"settime", rtfd_settime},
		{"gettime", rtfd_gettime},
		{"close", rtfd_close},
		{"read", rtfd_read},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Set up the ratchet.timerfd namespace functions. */
	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_timerfd_class");

	/* Set up the ratchet.timerfd class and metatables. */
	luaL_newmetatable (L, "ratchet_timerfd_meta");
	lua_newtable (L);
	luaL_setfuncs (L, meths, 0);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
