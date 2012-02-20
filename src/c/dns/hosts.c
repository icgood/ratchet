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

/* {{{ load_hosts() */
static struct dns_hosts *load_hosts (lua_State *L, int index)
{
	struct dns_hosts *hosts;
	const char *path;
	int error = 0, i;

	/* Check for nil or none. */
	if (lua_isnoneornil (L, index))
	{
		hosts = dns_hosts_local (&error);
		if (!hosts)
		{
			lua_pushfstring (L, "/etc/hosts: %s", dns_strerror (error));
			return NULL;
		}
		return hosts;
	}
	else
		luaL_checktype (L, index, LUA_TTABLE);

	/* Create new hosts object. */
	hosts = dns_hosts_open (&error);
	if (!hosts)
	{
		lua_pushfstring (L, "dns_hosts_open: %s", dns_strerror (error));
		return NULL;
	}

	/* Load given /etc/hosts alternative(s). */
	for (i=1; ; i++)
	{
		lua_rawgeti (L, index, i);
		if (lua_isnil (L, -1))
			break;

		path = lua_tostring (L, -1);
		error = dns_hosts_loadpath (hosts, path);
		if (error)
		{
			lua_pushfstring (L, "%s: %s", path, dns_strerror (error));
			return NULL;
		}

		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	return hosts;
}
/* }}} */

/* {{{ myhosts_new() */
static int myhosts_new (lua_State *L)
{
	lua_settop (L, 1);

	struct dns_hosts **new = (struct dns_hosts **) lua_newuserdata (L, sizeof (struct dns_hosts *));
	*new = load_hosts (L, 1);
	if (!*new)
		return ratchet_error_top (L, "ratchet.dns.hosts.new()", NULL);

	luaL_getmetatable (L, "ratchet_dns_hosts_meta");
	lua_setmetatable (L, -2);

	return 1;
}
/* }}} */

/* {{{ myhosts_gc() */
static int myhosts_gc (lua_State *L)
{
	struct dns_hosts **hosts = (struct dns_hosts **) luaL_checkudata (L, 1, "ratchet_dns_hosts_meta");
	dns_hosts_close (*hosts);
	*hosts = NULL;

	return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_dns_hosts() */
int luaopen_ratchet_dns_hosts (lua_State *L)
{
	/* Static functions in the namespace. */
	const luaL_Reg funcs[] = {
		{"new", myhosts_new},
		{NULL}
	};

	/* Meta-methods for object metatables. */
	const luaL_Reg metameths[] = {
		{"__gc", myhosts_gc},
		{NULL}
	};

	/* Set up the class and metatables. */
	luaL_newmetatable (L, "ratchet_dns_hosts_meta");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	/* Set up the namespace and functions. */
	luaL_newlib (L, funcs);
	lua_getfield (L, -1, "new");
	lua_call (L, 0, 1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_dns_hosts_default");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
