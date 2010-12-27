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

#include <sys/time.h>
#include <math.h>
#include <string.h>

#include "misc.h"

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
void gettimeval (lua_State *L, int index, struct timeval *tv)
{
	double secs = (double) luaL_checknumber (L, index);
	double intpart, fractpart;
	fractpart = modf (secs, &intpart);
	tv->tv_sec = (long int) intpart;
	tv->tv_usec = (long int) (fractpart * 1000000.0);
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
