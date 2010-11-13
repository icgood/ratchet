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

/* {{{ mytimer_parse_timer() */
static int mytimer_parse_timer (lua_State *L)
{
	const char *str = luaL_checkstring (L, 1);
	struct itimerspec *tsp = (struct itimerspec *) lua_newuserdata (L, sizeof (struct itimerspec));
	int clockid = CLOCK_MONOTONIC;
	memset (tsp, 0, sizeof (struct itimerspec));

	if (strncmp (str, "realtime:", 9) == 0)
	{
		clockid = CLOCK_REALTIME;
		str += 9;
	}
	lua_pushinteger (L, clockid);

	/* The following is adapted from Michael Kerrisk's "The Linux
	 * Programming Interface", No Starch Press, 2010. */
	char *cptr, *sptr;

	cptr = strchr (str, ':');
	if (cptr != NULL)
		*cptr = '\0';

	sptr = strchr (str, '/');
	if (sptr != NULL)
		*sptr = '\0';

	tsp->it_value.tv_sec = atoi (str);
	tsp->it_value.tv_nsec = (sptr != NULL) ? atoi (sptr + 1) : 0;

	if (cptr != NULL)
	{
		sptr = strchr (cptr + 1, '/');
		if (sptr != NULL)
			*sptr = '\0';
		tsp->it_interval.tv_sec = atoi (cptr + 1);
		tsp->it_interval.tv_nsec = (sptr != NULL) ? atoi (sptr + 1) : 0;
	}

	return 2;
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
	struct itimerspec *tsp = (struct itimerspec *) lua_touserdata (L, 2);
	int clockid = luaL_checkint (L, 3);

	if (!tsp)
		return luaL_typerror (L, 3, "itimerspec userdata");

	int tfd = timerfd_create (clockid, 0);
	if (tfd < 0)
		return rhelp_perror (L);

	lua_pushinteger (L, tfd);
	lua_setfield (L, 1, "fd");

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
		{"recv", mytimer_recv},
		{"close", mytimer_close},
		{NULL}
	};
	luaL_Reg funcs[] = {
		{"parse_timer", mytimer_parse_timer},
		{NULL}
	};

	rhelp_newclass (L, "ratchet.timer", meths, funcs);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
