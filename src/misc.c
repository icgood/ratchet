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

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "misc.h"

/* {{{ raise_perror_ln() */
int raise_perror_ln (lua_State *L, const char *file, int line)
{
	char errorbuf[512];

	if (errno)
	{
		if (strerror_r (errno, errorbuf, sizeof (errorbuf)) == -1)
			lua_pushfstring (L, "%s:%d: Unknown error occured. [errno=%d]", file, line, errno);
		else
			lua_pushfstring (L, "%s:%d: %s", file, line, errorbuf);
	}
	else
		lua_pushfstring (L, "%s:%d: Unknown error occured.", file, line);
	
	return lua_error (L);
}
/* }}} */

/* {{{ return_perror_ln() */
int return_perror_ln (lua_State *L, const char *file, int line)
{
	char errorbuf[512];

	lua_pushnil (L);

	if (errno)
	{
		if (strerror_r (errno, errorbuf, sizeof (errorbuf)) == -1)
			lua_pushfstring (L, "%s:%d: Unknown error occured. [errno=%d]", file, line, errno);
		else
			lua_pushfstring (L, "%s:%d: %s", file, line, errorbuf);
	}
	else
		lua_pushfstring (L, "%s:%d: Unknown error occured.", file, line);
	
	return 2;
}
/* }}} */

/* {{{ build_lua_function() */
void build_lua_function (lua_State *L, const char *fstr)
{
	luaL_loadstring (L, fstr);
	lua_call (L, 0, 1);
}
/* }}} */

/* {{{ register_luafuncs() */
void register_luafuncs (lua_State *L, int index, const struct luafunc *fs)
{
	const struct luafunc *it;

	if (index < 0)
		index = lua_gettop (L) + index + 1;

	for (it = fs; it->fname != NULL; it++)
	{
		build_lua_function (L, it->fstr);
		lua_setfield (L, index, it->fname);
	}
}
/* }}} */

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

/* {{{ set_nonblocking() */
int set_nonblocking (int fd)
{
	int flags = 1;
#ifdef O_NONBLOCK
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return ioctl(fd, FIOBIO, &flags);
#endif

}
/* }}} */

/* {{{ printf_index() */
static void printf_index (lua_State *L, int i)
{
	int t = lua_type (L, i);
	int j;

	switch (t)
	{
		case LUA_TSTRING: {
			printf ("'%s'", lua_tostring (L, i));
			break;
		}
		case LUA_TBOOLEAN: {
			printf (lua_toboolean (L, 1) ? "true" : "false");
			break;
		}
		case LUA_TNUMBER: {
			printf ("%g", lua_tonumber (L, i));
			break;
		}
		case LUA_TTABLE: {
			printf ("<{");
			for (lua_pushnil (L); lua_next (L, i); lua_pop (L, 1))
			{
				int top = lua_gettop (L);
				printf_index (L, top-1);
				printf ("=");
				if (lua_istable (L, top))
				{
					for (j=1; j<=top-2; j++)
					{
						if (lua_equal (L, j, top))
						{
							printf ("<table:%p>", lua_topointer (L, top));
							break;
						}
					}
					if (j >= top-1)
						printf_index (L, top);
				}
				else
					printf_index (L, top);
				printf (",");
			}
			printf ("}:%p>", lua_topointer (L, i));
			break;
		}
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		case LUA_TUSERDATA: {
			printf ("<%s", lua_typename (L, t));
			printf (":%p>", lua_topointer (L, i));
			break;
		}
		default: {
			printf ("%s", lua_typename (L, t));
			break;
		}
	}
}
/* }}} */

/* {{{ stackdump_ln() */
void stackdump_ln (lua_State *L, const char *file, int line)
{
	int i;
	int top = lua_gettop (L);

	printf ("------ stackdump %s:%d -------\n", file, line);
	for (i=1; i<=top; i++)
	{
		printf ("%d: ", i);
		printf_index (L, i);
		printf ("\n");
	}
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
