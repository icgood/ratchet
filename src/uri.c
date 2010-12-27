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

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ ruri_new() */
static int ruri_new (lua_State *L)
{
	const char *defschema = luaL_optstring (L, 1, "tcp");

	lua_newuserdata (L, sizeof (int));

	luaL_getmetatable (L, "ratchet_uri_meta");
	lua_setmetatable (L, -2);

	/* Set up the environment with the default schema. */
	lua_newtable (L);
	lua_pushstring (L, defschema);
	lua_setfield (L, -2, "");
	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ ruri_register() */
static int ruri_register (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_uri_meta");
	luaL_checkstring (L, 2);
	luaL_checkany (L, 3);	/* We'll fail later if this is not a callable object/function. */

	/* Check for empty-string schema. */
	if (lua_objlen (L, 2) == 0)
		return luaL_argerror (L, 2, "empty string not allowed");

	/* Set the string arg as key, callable as value in environment. */
	lua_getfenv (L, 1);
	lua_pushvalue (L, 2);
	lua_pushvalue (L, 3);
	lua_settable (L, -3);
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ ruri_call() */
static int ruri_call (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_uri_meta");
	luaL_checkstring (L, 2);
	lua_settop (L, 2);

	/* Call self:get_schema_and_data(uri). */
	lua_getfield (L, 1, "get_schema_and_data");
	lua_pushvalue (L, 1);
	lua_pushvalue (L, 2);
	lua_call (L, 2, 2);

	/* Call the filter, return results. */
	lua_call (L, 1, LUA_MULTRET);
	int rets = lua_gettop (L) - 2;
	return rets;
}
/* }}} */

/* {{{ ruri_get_schema_and_data() */
static int ruri_get_schema_and_data (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_uri_meta");
	luaL_checkstring (L, 2);
	lua_settop (L, 2);

	lua_getfenv (L, 1);

	/* Get the parts of the URI string. */
	lua_getfield (L, 2, "match");
	lua_pushvalue (L, 2);
	lua_pushliteral (L, "^([%w%+%.%-]+):(.*)$");
	lua_call (L, 2, 2);

	/* Check for broken URI and assume default schema. */
	if (lua_isnil (L, -2))
	{
		lua_pop (L, 2);
		lua_getfield (L, 3, "");
		lua_pushvalue (L, 2);
	}

	/* Get the filter for the schema. */
	lua_pushvalue (L, -2);
	lua_gettable (L, 3);
	if (lua_isnil (L, -1))
	{
		const char *schema = lua_tostring (L, -3);
		return luaL_error (L, "unknown schema [%s]", schema);
	}
	lua_replace (L, -3);

	return 2;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_uri() */
int luaopen_ratchet_uri (lua_State *L)
{
	/* Static functions in the ratchet.uri namespace. */
	static const luaL_Reg funcs[] = {
		{"new", ruri_new},
		{NULL}
	};

	/* Meta-methods for ratchet.uri object metatables. */
	static const luaL_Reg metameths[] = {
		{"__call", ruri_call},
		{NULL}
	};

	/* Methods in the ratchet.uri class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"register", ruri_register},
		{"resolve", ruri_call},
		/* Undocumented, helper methods. */
		{"get_schema_and_data", ruri_get_schema_and_data},
		{NULL}
	};

	/* Set up the ratchet.uri class and metatables. */
	luaL_newmetatable (L, "ratchet_uri_meta");
	lua_newtable (L);
	luaI_openlib (L, NULL, meths, 0);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, metameths, 0);
	lua_pop (L, 1);

	/* Set up the ratchet.uri namespace functions. */
	luaI_openlib (L, "ratchet.uri", funcs, 0);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
