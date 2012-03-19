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

#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "misc.h"

#define RETURN_SIGNAL_BY_NAME(s) if (0 == strcmp (#s, lua_tostring (L, index))) return s

/* {{{ strmatch() */
int strmatch (lua_State *L, int index, const char *match)
{
	int top = lua_gettop (L);
	lua_getfield (L, index, "match");
	lua_pushvalue (L, index);
	lua_pushstring (L, match);
	lua_call (L, 2, LUA_MULTRET);
	int rets = lua_gettop (L) - top;
	if (rets == 0 || lua_isnil (L, -1))
	{
		lua_pop  (L, rets);
		return 0;
	}
	return rets;
}
/* }}} */

/* {{{ strequal() */
int strequal (lua_State *L, int index, const char *s2)
{
	if (lua_isstring (L, index))
	{
		const char *s1 = lua_tostring (L, index);
		return (0 == strcmp (s1, s2));
	}
	
	return 0;
}
/* }}} */

/* {{{ fromtimeval() */
double fromtimeval (struct timeval *tv)
{
	double ret = 0.0;
	ret += (double) tv->tv_sec;
	ret += ((double) tv->tv_usec) / 1000000.0;
	return ret;
}
/* }}} */

/* {{{ gettimeval() */
int gettimeval (double secs, struct timeval *tv)
{
	if (secs < 0.0)
		return 0;
	double intpart, fractpart;
	fractpart = modf (secs, &intpart);
	tv->tv_sec = (long int) intpart;
	tv->tv_usec = (long int) (fractpart * 1000000.0);
	return 1;
}
/* }}} */

/* {{{ gettimeval_arg() */
int gettimeval_arg (lua_State *L, int index, struct timeval *tv)
{
	double secs = (double) luaL_checknumber (L, index);
	return gettimeval (secs, tv);
}
/* }}} */

/* {{{ gettimeval_opt() */
int gettimeval_opt (lua_State *L, int index, struct timeval *tv)
{
	double secs = (double) luaL_optnumber (L, index, -1.0);
	return gettimeval (secs, tv);
}
/* }}} */

/* {{{ fromtimespec() */
double fromtimespec (struct timespec *tv)
{
	double ret = 0.0;
	ret += (double) tv->tv_sec;
	ret += ((double) tv->tv_nsec) / 1000000000.0;
	return ret;
}
/* }}} */

/* {{{ gettimespec() */
int gettimespec (double secs, struct timespec *tv)
{
	if (secs < 0.0)
		return 0;
	double intpart, fractpart;
	fractpart = modf (secs, &intpart);
	tv->tv_sec = (long int) intpart;
	tv->tv_nsec = (long int) (fractpart * 1000000000.0);
	return 1;
}
/* }}} */

/* {{{ gettimespec_arg() */
int gettimespec_arg (lua_State *L, int index, struct timespec *tv)
{
	double secs = (double) luaL_checknumber (L, index);
	return gettimespec (secs, tv);
}
/* }}} */

/* {{{ gettimespec_opt() */
int gettimespec_opt (lua_State *L, int index, struct timespec *tv)
{
	double secs = (double) luaL_optnumber (L, index, -1.0);
	return gettimespec (secs, tv);
}
/* }}} */

/* {{{ get_signal() */
int get_signal (lua_State *L, int index, int def)
{
	if (lua_isnumber (L, index))
		return (int) lua_tointeger (L, index);

	else if (lua_isstring (L, index))
	{
		RETURN_SIGNAL_BY_NAME (SIGHUP);
		RETURN_SIGNAL_BY_NAME (SIGINT);
		RETURN_SIGNAL_BY_NAME (SIGQUIT);
		RETURN_SIGNAL_BY_NAME (SIGILL);
		RETURN_SIGNAL_BY_NAME (SIGABRT);
		RETURN_SIGNAL_BY_NAME (SIGFPE);
		RETURN_SIGNAL_BY_NAME (SIGKILL);
		RETURN_SIGNAL_BY_NAME (SIGSEGV);
		RETURN_SIGNAL_BY_NAME (SIGPIPE);
		RETURN_SIGNAL_BY_NAME (SIGALRM);
		RETURN_SIGNAL_BY_NAME (SIGTERM);
		RETURN_SIGNAL_BY_NAME (SIGUSR1);
		RETURN_SIGNAL_BY_NAME (SIGUSR2);
		RETURN_SIGNAL_BY_NAME (SIGCHLD);
		RETURN_SIGNAL_BY_NAME (SIGCONT);
		RETURN_SIGNAL_BY_NAME (SIGSTOP);
		RETURN_SIGNAL_BY_NAME (SIGTSTP);
		RETURN_SIGNAL_BY_NAME (SIGTTIN);
		RETURN_SIGNAL_BY_NAME (SIGTTOU);
		return -1;
	}
	else if (lua_isnoneornil (L, index))
		return def;

	else
		return -1;
}
/* }}} */

/* {{{ set_nonblocking() */
int set_nonblocking (int fd)
{
	int flags = 1;
#ifdef O_NONBLOCK
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return ioctl(fd, FIONBIO, &flags);
#endif

}
/* }}} */

/* {{{ set_closeonexec() */
int set_closeonexec (int fd)
{
	int flags;
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | FD_CLOEXEC);
}
/* }}} */

/* {{{ fprintf_index() */
static void fprintf_index (lua_State *L, FILE *out, int i)
{
	int t = lua_type (L, i);
	int j;

	switch (t)
	{
		case LUA_TSTRING: {
			fprintf (out, "'%s'", lua_tostring (L, i));
			break;
		}
		case LUA_TBOOLEAN: {
			fprintf (out, lua_toboolean (L, i) ? "true" : "false");
			break;
		}
		case LUA_TNUMBER: {
			fprintf (out, "%g", lua_tonumber (L, i));
			break;
		}
		case LUA_TTABLE: {
			int n = 0;
			for (lua_pushnil (L); lua_next (L, i); lua_pop (L, 1))
				n++;
			
			fprintf (out, "<%d:{", n);
			for (lua_pushnil (L); lua_next (L, i); lua_pop (L, 1))
			{
				int top = lua_gettop (L);
				fprintf_index (L, out, top-1);
				fprintf (out, "=");
				if (lua_istable (L, top))
				{
					for (j=1; j<=top-2; j++)
					{
						if (lua_compare (L, j, top, LUA_OPEQ))
						{
							fprintf (out, "<table:%p>", lua_topointer (L, top));
							break;
						}
					}
					if (j >= top-1)
						fprintf_index (L, out, top);
				}
				else
					fprintf_index (L, out, top);
				fprintf (out, ",");
			}
			fprintf (out, "}:%p>", lua_topointer (L, i));
			break;
		}
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA: {
			fprintf (out, "<%s", lua_typename (L, t));
			fprintf (out, ":%p>", lua_topointer (L, i));
			break;
		}
		case LUA_TUSERDATA: {
			fprintf (out, "<userdata:");
			lua_getuservalue (L, i);
			if (!lua_isnil (L, -1))
			{
				fprintf_index (L, out, lua_gettop (L));
				fprintf (out, ":");
			}
			lua_pop (L, 1);
			fprintf (out, "%p>", lua_topointer (L, i));
			break;
		}
		default: {
			fprintf (out, "%s", lua_typename (L, t));
			break;
		}
	}
}
/* }}} */

/* {{{ fstackdump_ln() */
void fstackdump_ln (lua_State *L, FILE *out, const char *file, int line)
{
	int i;
	int top = lua_gettop (L);

	fprintf (out, "------ stackdump %s:%d -------\n", file, line);
	for (i=1; i<=top; i++)
	{
		fprintf (out, "%d: ", i);
		fprintf_index (L, out, i);
		fprintf (out, "\n");
	}

	fflush (out);
}
/* }}} */

// vim:fdm=marker:ai:ts=4:sw=4:noet:
