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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>
#include <expat.h>
#include <errno.h>

#include "misc.h"
#include "makeclass.h"
#include "xml.h"

/* {{{ generic_start_cb() */
static void XMLCALL generic_start_cb (void *user_data, const char *name, const char **atts)
{
	lua_State *L = (lua_State *) user_data;
	int i;

	lua_getfield (L, 1, "start_cb");
	lua_getfield (L, 1, "state");
	lua_pushstring (L, name);
	lua_newtable (L);
	for (i=0; atts[i] != NULL; i+=2)
	{
		lua_pushstring (L, atts[i]);
		lua_pushstring (L, atts[i+1]);
		lua_rawset (L, -3);
	}
	lua_call (L, 3, 0);
}
/* }}} */

/* {{{ generic_end_cb() */
static void XMLCALL generic_end_cb (void *user_data, const char *name)
{
	lua_State *L = (lua_State *) user_data;

	lua_getfield (L, 1, "end_cb");
	lua_getfield (L, 1, "state");
	lua_pushstring (L, name);
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ generic_data_cb() */
static void generic_data_cb (void *user_data, const char *s, int len)
{
	lua_State *L = (lua_State *) user_data;

	lua_getfield (L, 1, "data_cb");
	lua_getfield (L, 1, "state");
	lua_pushlstring (L, s, (size_t) len);
	lua_call (L, 2, 0);
}
/* }}} */

/* {{{ myxml_init() */
static int myxml_init (lua_State *L)
{
	lua_settop (L, 5);
	const char *encoding = NULL;
	XML_StartElementHandler start_cb = NULL;
	XML_EndElementHandler end_cb = NULL;
	XML_CharacterDataHandler data_cb = NULL;

	lua_newtable (L);
	lua_setfield (L, 1, "state");

	/* Grab the named parameters. */
	for (lua_pushnil (L); lua_next (L, 2) != 0; lua_pop (L, 1))
	{
		if (luaH_strequal (L, -2, "state"))
		{
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "state");
		}
		else if (luaH_strequal (L, -2, "startelem"))
		{
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "start_cb");
			start_cb = generic_start_cb;
		}
		else if (luaH_strequal (L, -2, "endelem"))
		{
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "end_cb");
			end_cb = generic_end_cb;
		}
		else if (luaH_strequal (L, -2, "elemdata"))
		{
			lua_pushvalue (L, -1);
			lua_setfield (L, 1, "data_cb");
			data_cb = generic_data_cb;
		}
		else if (luaH_strequal (L, -2, "encoding"))
			encoding = lua_tostring (L, -1);
		else
			luaL_argerror (L, 2, "unknown named params given");
	}

	XML_Parser parser = XML_ParserCreate (encoding);
	if (!parser)
		return luaL_error (L, "XML parser create failed");

	XML_SetUserData (parser, L);
	if (start_cb || end_cb)
		XML_SetElementHandler (parser, start_cb, end_cb);
	if (data_cb)
		XML_SetCharacterDataHandler (parser, data_cb);

	lua_pushlightuserdata (L, parser);
	lua_setfield (L, 1, "parser");

	return 0;
}
/* }}} */

/* {{{ myxml_del() */
static int myxml_del (lua_State *L)
{
	lua_getfield (L, 1, "parser");
	XML_Parser parser = (XML_Parser) lua_touserdata (L, -1);
	lua_pop (L, 1);

	if (parser)
		XML_ParserFree (parser);

	lua_pushlightuserdata (L, NULL);
	lua_setfield (L, 1, "parser");

	return 0;
}
/* }}} */

/* {{{ myxml_parse() */
static int myxml_parse (lua_State *L)
{
	const char *data;
	size_t datalen;
	int more_coming = 0;

	lua_settop (L, 3);

	lua_getfield (L, 1, "parser");
	XML_Parser parser = (XML_Parser) lua_touserdata (L, -1);
	lua_pop (L, 1);

	data = lua_tolstring (L, 2, &datalen);
	more_coming = lua_toboolean (L, 3);

	int ret = XML_Parse (parser, data, datalen, (more_coming ? 0 : 1));
	if (ret == 0)
	{
		lua_pushboolean (L, 0);
		int error = (int) XML_GetErrorCode (parser);
		lua_pushstring (L, XML_ErrorString (error));
	}
	else
	{
		lua_pushboolean (L, 1);
		lua_pushnil (L);
	}
	
	return 2;
}
/* }}} */

/* {{{ myxml_parsesome() */
static int myxml_parsesome (lua_State *L)
{
	lua_settop (L, 2);
	lua_pushboolean (L, 1);

	return luaH_callmethod (L, 1, "parse", 2);
}
/* }}} */

/* {{{ luaopen_luah_xml() */
int luaopen_luah_xml (lua_State *L)
{
	luaL_Reg meths[] = {
		{"init", myxml_init},
		{"del", myxml_del},
		{"parse", myxml_parse},
		{"parsesome", myxml_parsesome},
		{NULL}
	};

	luaH_newclass (L, "luah.xml", meths);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
