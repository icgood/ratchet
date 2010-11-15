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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "timer.h"

/* {{{ timespec_to_double() */
static double timespec_to_double (const struct timespec *ts)
{
	double secs = 0.0f;

	secs += (double) ts->tv_sec;
	secs += (double) ts->tv_nsec / 1000000000.0;

	return secs;
}
/* }}} */

/* {{{ double_to_timespec() */
static void double_to_timespec (double secs, struct timespec *ts)
{
	double intpart, fractpart;

	fractpart = modf (secs, &intpart) * 1000000000.0;
	ts->tv_sec = (time_t) intpart;
	ts->tv_nsec = (long) fractpart;
}
/* }}} */

/* {{{ fd_set_blocking_flag() */
static int fd_set_blocking_flag (int fd, int blocking)
{
	int flags;

#ifdef O_NONBLOCK
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	if (!blocking)
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	else
		return fcntl(fd, F_SETFL, flags & (~O_NONBLOCK));
#else
	flags = (blocking ? 0 : 1);
	return ioctl(fd, FIONBIO, &flags);
#endif
}
/* }}} */

/* {{{ mytimer_init() */
static int mytimer_init (lua_State *L)
{
	const char *clockid_str = luaL_optstring (L, 2, "");
	int clockid;

	if (strcmp (clockid_str, "realtime") == 0)
		clockid = CLOCK_REALTIME;
	else
		clockid = CLOCK_MONOTONIC;

	int tfd = timerfd_create (clockid, 0);
	if (tfd < 0)
		return rhelp_perror (L);

	lua_pushinteger (L, tfd);
	lua_setfield (L, 1, "fd");

	lua_pushinteger (L, clockid);
	lua_setfield (L, 1, "clockid");

	return 0;
}
/* }}} */

/* {{{ mytimer_close() */
static int mytimer_close (lua_State *L)
{
	int fd;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	if (fd >= 0)
		close (fd);
	lua_pop (L, 1);

	lua_pushinteger (L, -1);
	lua_setfield (L, 1, "fd");

	return 0;
}
/* }}} */

/* {{{ mytimer_getfd() */
static int mytimer_getfd (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	return 1;
}
/* }}} */

/* {{{ mytimer_set_blocking() */
static int mytimer_set_blocking (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	fd_set_blocking_flag (lua_tointeger (L, -1), 1);
	return 1;
}
/* }}} */

/* {{{ mytimer_set_nonblocking() */
static int mytimer_set_nonblocking (lua_State *L)
{
	lua_getfield (L, 1, "fd");
	fd_set_blocking_flag (lua_tointeger (L, -1), 0);
	return 1;
}
/* }}} */

/* {{{ mytimer_set() */
static int mytimer_set (lua_State *L)
{
	double value = luaL_optnumber (L, 2, 0.0);
	double interval = luaL_optnumber (L, 3, 0.0);

	struct itimerspec tsp;
	memset (&tsp, 0, sizeof (struct itimerspec));
	if (value != 0.0)
		double_to_timespec (value, &tsp.it_value);
	if (interval != 0.0)
		double_to_timespec (interval, &tsp.it_interval);

	lua_getfield (L, 1, "fd");
	int fd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	struct itimerspec *old = (struct itimerspec *) lua_newuserdata (L, sizeof (struct itimerspec));
	if (timerfd_settime (fd, 0, &tsp, old) < 0)
		rhelp_perror (L);

	/* Old itimerspec is on top of the stack, return it with get(). */
	return rhelp_callmethod (L, 1, "get", 1);
}
/* }}} */

/* {{{ mytimer_set_abs() */
static int mytimer_set_abs (lua_State *L)
{
	double value = luaL_optnumber (L, 2, 0.0);
	double interval = luaL_optnumber (L, 3, 0.0);

	struct itimerspec tsp;
	memset (&tsp, 0, sizeof (struct itimerspec));
	if (value != 0.0)
		double_to_timespec (value, &tsp.it_value);
	if (interval != 0.0)
		double_to_timespec (interval, &tsp.it_interval);

	lua_getfield (L, 1, "fd");
	int fd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	struct itimerspec *old = (struct itimerspec *) lua_newuserdata (L, sizeof (struct itimerspec));
	if (timerfd_settime (fd, TFD_TIMER_ABSTIME, &tsp, old) < 0)
		rhelp_perror (L);

	/* Old itimerspec is on top of the stack, return it with get(). */
	return rhelp_callmethod (L, 1, "get", 1);
}
/* }}} */

/* {{{ mytimer_get() */
static int mytimer_get (lua_State *L)
{
	if (lua_isuserdata (L, 2))
	{
		struct itimerspec *its = (struct itimerspec *) lua_touserdata (L, 2);
		lua_pushnumber (L, timespec_to_double (&its->it_value));
		lua_pushnumber (L, timespec_to_double (&its->it_interval));
	}
	else
	{
		struct itimerspec its;

		lua_getfield (L, 1, "fd");
		int fd = lua_tointeger (L, -1);
		lua_pop (L, 1);

		if (fd < 0)
			return 0;

		if (timerfd_gettime (fd, &its) < 0)
			rhelp_perror (L);
		lua_pushnumber (L, timespec_to_double (&its.it_value));
		lua_pushnumber (L, timespec_to_double (&its.it_interval));
	}

	return 2;
}
/* }}} */

#if HAVE_CLOCK_GETTIME
/* {{{ mytimer_get_now() */
static int mytimer_get_now (lua_State *L)
{
	struct timespec ts;
	int clockid = CLOCK_MONOTONIC;

	if (lua_istable (L, 1))
	{
		lua_getfield (L, 1, "clockid");
		if (lua_isnumber (L, -1))
			clockid = lua_tointeger (L, -1);
	}

	if (clock_gettime ((clockid_t) clockid, &ts) < 0)
		rhelp_perror (L);

	lua_pushnumber (L, timespec_to_double (&ts));
	return 1;
}
/* }}} */
#endif

/* {{{ mytimer_recv() */
static int mytimer_recv (lua_State *L)
{
	int fd;
	uint64_t n;

	lua_getfield (L, 1, "fd");
	fd = lua_tointeger (L, -1);
	lua_pop (L, 1);

	if (read (fd, &n, sizeof (uint64_t)) != sizeof (uint64_t))
	{
		lua_pushinteger (L, -1);
		errno = 0;
	}
	else
		lua_pushnumber (L, (lua_Number) n);

	return 1;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_timer() */
int luaopen_ratchet_timer (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", mytimer_init},
		{"getfd", mytimer_getfd},
		{"set_blocking", mytimer_set_blocking},
		{"set_nonblocking", mytimer_set_nonblocking},
		{"set", mytimer_set},
		{"set_abs", mytimer_set_abs},
		{"get", mytimer_get},
#if HAVE_CLOCK_GETTIME
		{"get_now", mytimer_get_now},
#endif
		{"recv", mytimer_recv},
		{"close", mytimer_close},
		{NULL}
	};

	rhelp_newclass (L, "ratchet.timer", meths, NULL);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
