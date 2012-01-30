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

#include <math.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "ratchet.h"
#include "misc.h"
#include "libdns/dns.h"

/* {{{ load_resolv_conf() */
static struct dns_resolv_conf *load_resolv_conf (lua_State *L, int index)
{
	struct dns_resolv_conf *resconf;
	const char *path;
	int error = 0, i;

	/* Check for nil or none. */
	if (lua_isnoneornil (L, index))
	{
		lua_createtable (L, 1, 0);
		lua_pushliteral (L, "/etc/resolv.conf");
		lua_rawseti (L, -2, 1);
		lua_replace (L, index);
	}
	else
		luaL_checktype (L, index, LUA_TTABLE);
	
	/* Create new resconf object. */
	resconf = dns_resconf_open (&error);
	if (!resconf)
	{
		lua_pushfstring (L, "dns_resconf_open: %s", dns_strerror (error));
		return NULL;
	}

	/* Load /etc/resolv.conf or given alternative(s). */
	for (i=1; ; i++)
	{
		lua_rawgeti (L, index, i);
		if (lua_isnil (L, -1))
			break;

		path = lua_tostring (L, -1);
		error = dns_resconf_loadpath (resconf, path);
		if (error)
		{
			lua_pushfstring (L, "%s: %s", path, dns_strerror (error));
			return NULL;
		}

		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	return resconf;
}
/* }}} */

/* {{{ myresconf_new() */
static int myresconf_new (lua_State *L)
{
	lua_settop (L, 1);

	struct dns_resolv_conf **new = (struct dns_resolv_conf **) lua_newuserdata (L, sizeof (struct dns_resolv_conf *));
	*new = load_resolv_conf (L, 1);
	if (!*new)
		return ratchet_error_top (L, "ratchet.dns.resolv_conf.new()", NULL);

	luaL_getmetatable (L, "ratchet_dns_resolv_conf_meta");
	lua_setmetatable (L, -2);

	return 1;
}
/* }}} */

/* {{{ myresconf_gc() */
static int myresconf_gc (lua_State *L)
{
	struct dns_resolv_conf **resconf = (struct dns_resolv_conf **) luaL_checkudata (L, 1, "ratchet_dns_resolv_conf_meta");
	dns_resconf_close (*resconf);
	*resconf = NULL;

	return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_dns_resolv_conf() */
int luaopen_ratchet_dns_resolv_conf (lua_State *L)
{
	/* Static functions in the namespace. */
	const luaL_Reg funcs[] = {
		{"new", myresconf_new},
		{NULL}
	};

	/* Meta-methods for object metatables. */
	const luaL_Reg metameths[] = {
		{"__gc", myresconf_gc},
		{NULL}
	};

	/* Set up the class and metatables. */
	luaL_newmetatable (L, "ratchet_dns_resolv_conf_meta");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	/* Set up the namespace and functions. */
	luaL_newlib (L, funcs);
	lua_getfield (L, -1, "new");
	lua_call (L, 0, 1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_dns_resolv_conf_default");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
