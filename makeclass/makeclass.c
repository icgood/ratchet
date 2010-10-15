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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"

/* {{{ rhelp_callinitmethod() */
static int rhelp_callinitmethod (lua_State *L, int nargs)
{
	int mytop;

	lua_getfield (L, 1, "init");
	lua_pushvalue (L, 1);
	mytop = lua_gettop (L) - nargs - 1;
	if (!lua_isnil (L, -2))
	{
		lua_insert (L, mytop);
		lua_insert (L, mytop);
		lua_call (L, nargs+1, LUA_MULTRET);
		return lua_gettop (L) - mytop + 1;
	}
	else
	{
		lua_pop (L, 2 + nargs);
		return 0;
	}
}
/* }}} */

/* {{{ rhelp_setgcmethod() */
static void rhelp_setgcmethod (lua_State *L)
{
	lua_getfield (L, 1, "del");
	if (!lua_isnil (L, -1))
	{
		lua_newuserdata (L, sizeof (int));
		lua_newtable (L);
		lua_pushvalue (L, -3);
		lua_setfield (L, -2, "__gc");
		lua_pushvalue (L, 1);
		lua_setfield (L, -2, "__index");
		lua_setmetatable (L, -2);
		lua_setfield (L, 1, "__gcobj");
	}
	lua_pop (L, 1);
}
/* }}} */

/* {{{ rhelp_isinstance() */
static int rhelp_isinstance (lua_State *L)
{
	lua_getfield (L, 2, "prototype");
	lua_getmetatable (L, 1);
	int ret = lua_equal (L, -1, -2);
	lua_pop (L, 2);
	lua_pushboolean (L, ret);
	return 1;
}
/* }}} */

/* {{{ rhelp_newclass_new() */
static int rhelp_newclass_new (lua_State *L)
{
	int initrets;
	int nargs = lua_gettop (L) - 1;

	lua_newtable (L);
	lua_getfield (L, 1, "prototype");
	lua_setmetatable (L, -2);
	lua_replace (L, 1);

	rhelp_setgcmethod (L);
	initrets = rhelp_callinitmethod (L, nargs);

	return 1 + initrets;
}
/* }}} */

/* {{{ rhelp_newclass_newindex() */
static int rhelp_newclass_newindex (lua_State *L)
{
	lua_getfield (L, 1, "prototype");
	lua_insert (L, -3);
	lua_settable (L, -3);
	lua_pop (L, 1);
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ rhelp_newclass() */
void rhelp_newclass (lua_State *L, const char *name, const luaL_Reg *meths, const luaL_Reg *funcs)
{
	static const luaL_Reg nomeths[] = {{NULL}};

	if (!meths)
		meths = nomeths;
	if (!funcs)
		funcs = nomeths;
	if (!name)
		lua_newtable (L);
	luaL_register (L, name, funcs);

	/* Set up the class prototype object. */
	lua_newtable (L);
	luaL_register (L, NULL, meths);
	lua_pushvalue (L, -1);
	lua_setfield (L, -2, "__index");
	lua_pushcfunction (L, rhelp_isinstance);
	lua_setfield (L, -2, "isinstance");
	lua_pushvalue (L, -2);
	lua_setfield (L, -2, "class");
	lua_setfield (L, -2, "prototype");

	/* Set up the class object metatable. */
	lua_newtable (L);
	lua_pushcfunction (L, rhelp_newclass_new);
	lua_setfield (L, -2, "__call");
	lua_pushcfunction (L, rhelp_newclass_newindex);
	lua_setfield (L, -2, "__newindex");
	lua_getfield (L, -2, "prototype");
	lua_setfield (L, -2, "__index");
	lua_setmetatable (L, -2);
}
/* }}} */

/* {{{ rhelp_makeclass() */
int rhelp_makeclass (lua_State *L)
{
	const char *name = lua_tostring (L, -1);
	rhelp_newclass (L, name, NULL, NULL);
	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
