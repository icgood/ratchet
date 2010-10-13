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

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "misc.h"

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ rhelp_perror_ln() */
int rhelp_perror_ln (lua_State *L, const char *file, int line)
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

/* {{{ rhelp_setfieldint() */
void rhelp_setfieldint (lua_State *L, int index, const char *name, int value)
{
	lua_pushinteger (L, value);
	lua_setfield (L, index-1, name);
}
/* }}} */

/* {{{ rhelp_rawsetfield() */
void rhelp_rawsetfield (lua_State *L, int index, const char *key)
{
	lua_pushvalue (L, index);
	lua_pushstring (L, key);
	lua_pushvalue (L, -3);
	lua_rawset (L, -3);
	lua_pop (L, 2);
}
/* }}} */

/* {{{ rhelp_callfunction() */
int rhelp_callfunction (lua_State *L, int index, int nargs)
{
	int t = lua_gettop (L) - nargs;
	lua_pushvalue (L, index);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ rhelp_callboolfunction() */
int rhelp_callboolfunction (lua_State *L, int index, int nargs)
{
	int ret;
	lua_pushvalue (L, index);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs, 1);
	ret = lua_toboolean (L, -1);
	lua_pop (L, 1);
	return ret;
}
/* }}} */

/* {{{ rhelp_callmethod() */
int rhelp_callmethod (lua_State *L, int index, const char *method, int nargs)
{
	int t = lua_gettop (L) - nargs;
	lua_pushvalue (L, index);
	lua_getfield (L, index, method);
	lua_insert (L, -nargs-2);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs+1, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ rhelp_callboolmethod() */
int rhelp_callboolmethod (lua_State *L, int index, const char *method, int nargs)
{
	int ret;
	lua_pushvalue (L, index);
	lua_getfield (L, index, method);
	lua_insert (L, -nargs-2);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs+1, 1);
	ret = lua_toboolean (L, -1);
	lua_pop (L, 1);
	return ret;
}
/* }}} */

/* {{{ rhelp_tableremoven() */
void rhelp_tableremoven (lua_State *L, int index, int n)
{
	int i;
	lua_pushvalue (L, index);
	for (i=1; i<=n; i++)
	{
		lua_rawgeti (L, -1, n+i);
		lua_rawseti (L, -2, i);
	}
	for (i=n+1; ; i++)
	{
		lua_rawgeti (L, -1, i);
		if (lua_isnil (L, -1))
			break;
		lua_pop (L, 1);
		lua_pushnil (L);
		lua_rawseti (L, -2, i);
	}
	lua_pop (L, 2);
}
/* }}} */

/* {{{ rhelp_unpack() */
int rhelp_unpack (lua_State *L, int index)
{
	int i;
	int j = lua_objlen (L, index);
	for (i=1; i<=j; i++)
		lua_rawgeti (L, index, i);
	return j;
}
/* }}} */

/* {{{ rhelp_strmatch() */
int rhelp_strmatch (lua_State *L, const char *match)
{
	lua_pushstring (L, match);
	int rets = rhelp_callmethod (L, -2, "match", 1);
	if (lua_isnil (L, -1))
	{
		lua_pop (L, rets);
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ rhelp_strequal() */
int rhelp_strequal (lua_State *L, int index, const char *cmp)
{
	int ret;

	lua_pushvalue (L, index);
	lua_pushstring (L, cmp);
	ret = lua_equal (L, -2, -1);
	lua_pop (L, 2);

	return ret;
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

/* {{{ rhelp_stackdump_ln() */
void rhelp_stackdump_ln (lua_State *L, const char *file, int line)
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
