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

#include <string.h>
#include <arpa/inet.h>
#include <event.h>
#include <udns.h>

#include "ratchet.h"
#include "misc.h"

#define QUERY_V4 1
#define QUERY_V6 2
#define QUERY_MX 4
#define QUERY_TXT 8
#define QUERY_PTR 16

#ifndef DNS_DEFAULT_QUERY
#define DNS_DEFAULT_QUERY QUERY_V6
#endif

#ifndef DNS_MAX_TIMEOUT
#define DNS_MAX_TIMEOUT 0
#endif

#define get_dns_ctx(L, i) ((struct mydns_data *) luaL_checkudata (L, i, "ratchet_dns_meta"))->ctx
#define get_dns_timeout(L, i) ((struct mydns_data *) luaL_checkudata (L, i, "ratchet_dns_meta"))->timeout

/* {{{ struct mydns_data */
struct mydns_data
{
	struct dns_ctx *ctx;
	double timeout;
};
/* }}} */

/* {{{ return_error() */
static int return_error (lua_State *L, struct dns_ctx *ctx)
{
	int err = dns_status (ctx);
	static const char *lst[] = {};

	lua_pushnil (L);
	if (err == DNS_E_BADQUERY)
		lua_pushliteral (L, "badquery");

	else if (err == DNS_E_TEMPFAIL)
		lua_pushliteral (L, "tempfail");

	else if (err == DNS_E_PROTOCOL)
		lua_pushliteral (L, "protocol");

	else if (err == DNS_E_NXDOMAIN)
		lua_pushliteral (L, "nxdomain");

	else if (err == DNS_E_NODATA)
		lua_pushliteral (L, "nodata");

	else /* if (err == DNS_E_NOMEM) */
		return luaL_error (L, "dns call ran out of memory.");

	return 2;
}
/* }}} */

/* {{{ query_finished() */
static void query_finished (lua_State *L, struct dns_ctx *ctx)
{
	for (lua_pushnil (L); lua_next (L, 2) != 0; lua_pop (L, 1))
	{
		if (!lua_toboolean (L, -1))
		{
			lua_pop (L, 2);
			return;
		}
	}

	/* Resume the thread. */
	lua_getfield (L, 1, "unpause");
	lua_pushvalue (L, 1);
	lua_pushthread (L);
	lua_call (L, 2, 0);

	/* Remove all excess stack items. */
	lua_insert (L, 1);
	lua_settop (L, 1);
}
/* }}} */

/* {{{ query_finished_XXX() */

/* {{{ query_finished_a4() */
static void query_finished_a4 (struct dns_ctx *ctx, struct dns_rr_a4 *result, void *data)
{
	lua_State *L = (lua_State *) data;

	int i;

	if (!result)
	{
		return_error (L, ctx);
		lua_setfield (L, 2, "ipv4_error");
		lua_setfield (L, 2, "ipv4");
		query_finished (L, ctx);
		return;
	}

	lua_createtable (L, result->dnsa4_nrr, 0);

	for (i=0; i<result->dnsa4_nrr; i++)
	{
		struct in_addr *addr = &result->dnsa4_addr[i];
		struct in_addr *new = (struct in_addr *) lua_newuserdata (L, sizeof (*addr));
		memcpy (new, addr, sizeof (*addr));
		lua_rawseti (L, -2, i+1);
	}

	lua_setfield (L, 2, "ipv4");
	query_finished (L, ctx);
}
/* }}} */

/* {{{ query_finished_a6() */
static void query_finished_a6 (struct dns_ctx *ctx, struct dns_rr_a6 *result, void *data)
{
	lua_State *L = (lua_State *) data;

	int i;

	if (!result)
	{
		return_error (L, ctx);
		lua_setfield (L, 2, "ipv6_error");
		lua_setfield (L, 2, "ipv6");
		query_finished (L, ctx);
		return;
	}

	lua_createtable (L, result->dnsa6_nrr, 0);

	for (i=0; i<result->dnsa6_nrr; i++)
	{
		struct in6_addr *addr = &result->dnsa6_addr[i];
		struct in6_addr *new = (struct in6_addr *) lua_newuserdata (L, sizeof (*addr));
		memcpy (new, addr, sizeof (*addr));
		lua_rawseti (L, -2, i+1);
	}

	lua_setfield (L, 2, "ipv6");
	query_finished (L, ctx);
}
/* }}} */

/* {{{ query_finished_mx() */
static void query_finished_mx (struct dns_ctx *ctx, struct dns_rr_mx *result, void *data)
{
	lua_State *L = (lua_State *) data;

	int i, j;

	if (!result)
	{
		return_error (L, ctx);
		lua_setfield (L, 2, "mx_error");
		lua_setfield (L, 2, "mx");
		query_finished (L, ctx);
		return;
	}

	lua_newtable (L);

	for (i=0; i<result->dnsmx_nrr; i++)
	{
		lua_rawgeti (L, -1, result->dnsmx_mx[i].priority);
		if (lua_isnil (L, -1))
		{
			lua_pop (L, 1);
			lua_newtable (L);
			lua_pushstring (L, result->dnsmx_mx[i].name);
			lua_rawseti (L, -2, 1);
			lua_rawseti (L, -2, result->dnsmx_mx[i].priority);
		}
		else
		{
			for (j=1; ; j++)
			{
				lua_rawgeti (L, -1, j);
				if (lua_isnil (L, -1))
				{
					lua_pop (L, 1);
					lua_pushstring (L, result->dnsmx_mx[i].name);
					lua_rawseti (L, -2, j);
				}
				else
					lua_pop (L, 1);
			}
			lua_pop (L, 1);
		}
	}

	lua_setfield (L, 2, "mx");
	query_finished (L, ctx);
}
/* }}} */

/* {{{ query_finished_ptr() */
static void query_finished_ptr (struct dns_ctx *ctx, struct dns_rr_ptr *result, void *data)
{
	lua_State *L = (lua_State *) data;

	int i;

	if (!result)
	{
		return_error (L, ctx);
		lua_setfield (L, 2, "ptr_error");
		lua_setfield (L, 2, "ptr");
		query_finished (L, ctx);
		return;
	}

	lua_createtable (L, result->dnsptr_nrr, 0);

	for (i=0; i<result->dnsptr_nrr; i++)
	{
		lua_pushstring (L, result->dnsptr_ptr[i]);
		lua_rawseti (L, -2, i+1);
	}

	lua_setfield (L, 2, "ptr");
	query_finished (L, ctx);
}
/* }}} */

/* {{{ query_finished_txt() */
static void query_finished_txt (struct dns_ctx *ctx, struct dns_rr_txt *result, void *data)
{
	lua_State *L = (lua_State *) data;

	int i;

	if (!result)
	{
		return_error (L, ctx);
		lua_setfield (L, 2, "txt_error");
		lua_setfield (L, 2, "txt");
		query_finished (L, ctx);
		return;
	}

	lua_createtable (L, result->dnstxt_nrr, 0);

	for (i=0; i<result->dnstxt_nrr; i++)
	{
		lua_pushlstring (L, result->dnstxt_txt[i].txt, (size_t) result->dnstxt_txt[i].len);
		lua_rawseti (L, -2, i+1);
	}

	lua_setfield (L, 2, "txt");
	query_finished (L, ctx);
}
/* }}} */

/* }}} */

/* {{{ check_already_ipv6() */
static int check_already_ipv6 (lua_State *L, const char *data)
{
	if (0 == strcmp ("*", data))
	{
		lua_createtable (L, 1, 0);
		lua_pushlightuserdata (L, (void *) &in6addr_any);
		lua_rawseti (L, -2, 1);
		lua_setfield (L, -2, "ipv6");
		return 1;
	}

	struct in6_addr *addr = (struct in6_addr *) lua_newuserdata (L, sizeof (struct in6_addr));
	if (inet_pton (AF_INET6, data, addr) > 0)
	{
		lua_createtable (L, 1, 0);
		lua_insert (L, -2);
		lua_rawseti (L, -2, 1);
		lua_setfield (L, -2, "ipv6");
		return 1;
	}

	lua_pop (L, 1);
	return 0;
}
/* }}} */

/* {{{ check_already_ipv4() */
static int check_already_ipv4 (lua_State *L, const char *data)
{
	if (0 == strcmp ("*", data))
	{
		lua_createtable (L, 1, 0);
		struct in_addr *addr = (struct in_addr *) lua_newuserdata (L, sizeof (struct in_addr));
		addr->s_addr = INADDR_ANY;
		lua_rawseti (L, -2, 1);
		lua_setfield (L, -2, "ipv4");
		return 1;
	}

	struct in_addr *addr = (struct in_addr *) lua_newuserdata (L, sizeof (struct in_addr));
	if (inet_pton (AF_INET, data, addr) > 0)
	{
		lua_createtable (L, 1, 0);
		lua_insert (L, -2);
		lua_rawseti (L, -2, 1);
		lua_setfield (L, -2, "ipv4");
		return 1;
	}

	lua_pop (L, 1);
	return 0;
}
/* }}} */

/* {{{ reset_timeouts() */
static void reset_timeouts (lua_State *L, struct dns_ctx *ctx)
{
	dns_timeouts (ctx, DNS_MAX_TIMEOUT, 0);

	lua_getfenv (L, 1);

	/* Unpause dns thread, if it's paused. */
	lua_getfield (L, -1, "thread");
	if (lua_status (lua_tothread (L, -1)) == LUA_YIELD)
	{
		/* Prepare to call ratchet:unpause(). */
		lua_getfield (L, -2, "ratchet");
		lua_getfield (L, -1, "unpause");
		lua_insert (L, -2);

		/* Call as ratchet:unpause(dns_thread, false). */
		lua_pushvalue (L, -3);
		lua_pushboolean (L, 0);
		lua_call (L, 3, 0);
	}

	lua_pop (L, 2);
}
/* }}} */

/* {{{ timeout_handler() */
static void timeout_handler (struct dns_ctx *ctx, int new_timeout, void *data)
{
	if (ctx)
	{
		double *timeout = (double *) data;
		if (new_timeout < 0)
			*timeout = -1.0;
		else
			*timeout = (double) new_timeout;
	}
}
/* }}} */

/* {{{ get_query_type() */
static int get_query_type (lua_State *L, int index)
{
	static const char *lst[] = {"ipv4", "ipv6", "mx", "txt", "ptr", NULL};
	static const int howlst[] = {
		QUERY_V4, QUERY_V6,
		QUERY_MX, QUERY_TXT, QUERY_PTR
	};

	int i, j, ret = 0;
	for (i=1; ; i++)
	{
		lua_rawgeti (L, index, i);
		if (lua_isstring (L, -1))
		{
			const char *type = lua_tostring (L, -1);
			for (j=0; lst[j] != NULL; j++)
			{
				if (0 == strcmp (lst[j], type))
				{
					ret |= howlst[j];
					break;
				}
			}
			if (lst[j] == NULL)
				return luaL_error (L, "invalid query type given: %s", type);
			lua_pop (L, 1);
		}
		else
			break;
	}
	lua_pop (L, 1);

	if (!ret)
		ret = DNS_DEFAULT_QUERY;

	return ret;
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ mydns_new() */
static int mydns_new (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_meta");

	struct mydns_data *new = (struct mydns_data *) lua_newuserdata (L, sizeof (struct mydns_data));
	memset (new, 0, sizeof (struct mydns_data));
	new->ctx = dns_new (NULL);
	if (!new->ctx)
		return luaL_error (L, "dns_new failed");
	dns_open (new->ctx);
	new->timeout = -1.0;
	dns_set_tmcbck (new->ctx, timeout_handler, &new->timeout);

	luaL_getmetatable (L, "ratchet_dns_meta");
	lua_setmetatable (L, -2);

	/* Set up the environment table. */
	lua_newtable (L);

	/* Save the ratchet object. */
	lua_pushvalue (L, 1);
	lua_setfield (L, -2, "ratchet");

	/* Attach the DNS as a background thread. */
	lua_getfield (L, 1, "attach_background");
	lua_pushvalue (L, 1);
	lua_pushvalue (L, -4);
	lua_call (L, 2, 1);
	lua_setfield (L, -2, "thread");

	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ mydns_gc() */
static int mydns_gc (lua_State *L)
{
	struct dns_ctx *ctx = get_dns_ctx (L, 1);
	dns_free (ctx);

	return 0;
}
/* }}} */

/* {{{ mydns_call() */
static int mydns_call (lua_State *L)
{
	get_dns_ctx (L, 1);

	while (1)
	{
		lua_getfield (L, 1, "wait");
		lua_pushvalue (L, 1);
		lua_call (L, 1, 0);
	}

	return 0;
}
/* }}} */

/* {{{ mydns_get_fd() */
static int mydns_get_fd (lua_State *L)
{
	struct dns_ctx *ctx = get_dns_ctx (L, 1);
	lua_pushinteger (L, dns_sock (ctx));

	return 1;
}
/* }}} */

/* {{{ mydns_get_timeout() */
static int mydns_get_timeout (lua_State *L)
{
	double timeout = get_dns_timeout (L, 1);
	lua_pushnumber (L, (lua_Number) timeout);

	return 1;
}
/* }}} */

/* {{{ mydns_ioevent() */
static int mydns_ioevent (lua_State *L)
{
	struct dns_ctx *ctx = get_dns_ctx (L, 1);
	dns_ioevent (ctx, 0);

	return 0;
}
/* }}} */

/* {{{ mydns_timeouts() */
static int mydns_timeouts (lua_State *L)
{
	struct dns_ctx *ctx = get_dns_ctx (L, 1);
	dns_timeouts (ctx, DNS_MAX_TIMEOUT, 0);

	return 0;
}
/* }}} */

/* {{{ mydns_submit() */
static int mydns_submit (lua_State *L)
{
	struct dns_ctx *ctx = get_dns_ctx (L, 1);
	const char *data = luaL_checkstring (L, 2);
	int query_type = get_query_type (L, 3);
	int queries = 0;
	lua_settop (L, 3);

	if (lua_pushthread (L))
		return luaL_error (L, "submit cannot be called from main thread.");
	lua_pop (L, 1);

	/* Ratchet object is the first yielded argument. */
	lua_getfenv (L, 1);
	lua_getfield (L, -1, "ratchet");

	/* Results table. As results come in, they are filled in. When all table keys are
	 * filled in (i.e. not false) the thread will resume. */
	lua_newtable (L);

	if (query_type & QUERY_V6)
	{
		if (!check_already_ipv6 (L, data))
		{
			struct dns_query *query = dns_submit_a6 (ctx, data, 0, query_finished_a6, L);
			if (!query)
			{
				return_error (L, ctx);
				lua_setfield (L, -3, "ipv6_error");
				lua_setfield (L, -2, "ipv6");
			}
			else
			{
				queries++;
				lua_pushboolean (L, 0);
				lua_setfield (L, -2, "ipv6");
			}
		}
	}

	if (query_type & QUERY_V4)
	{
		if (!check_already_ipv4 (L, data))
		{
			struct dns_query *query = dns_submit_a4 (ctx, data, 0, query_finished_a4, L);
			if (!query)
			{
				return_error (L, ctx);
				lua_setfield (L, -3, "ipv4_error");
				lua_setfield (L, -2, "ipv4");
			}
			else
			{
				queries++;
				lua_pushboolean (L, 0);
				lua_setfield (L, -2, "ipv4");
			}
		}
	}

	if (query_type & QUERY_MX)
	{
		struct dns_query *query = dns_submit_mx (ctx, data, 0, query_finished_mx, L);
		if (!query)
		{
			return_error (L, ctx);
			lua_setfield (L, -3, "mx_error");
			lua_setfield (L, -2, "mx");
		}
		else
		{
			queries++;
			lua_pushboolean (L, 0);
			lua_setfield (L, -2, "mx");
		}
	}

	if (query_type & QUERY_TXT)
	{
		struct dns_query *query = dns_submit_txt (ctx, data, DNS_C_IN, 0, query_finished_txt, L);
		if (!query)
		{
			return_error (L, ctx);
			lua_setfield (L, -3, "txt_error");
			lua_setfield (L, -2, "txt");
		}
		else
		{
			queries++;
			lua_pushboolean (L, 0);
			lua_setfield (L, -2, "txt");
		}
	}

	if (query_type & QUERY_PTR)
	{
		struct in6_addr addr6;
		struct in_addr addr;

		int ret6 = inet_pton (AF_INET6, data, &addr6);
		int ret = inet_pton (AF_INET, data, &addr);

		if (ret6 > 0)
		{
			struct dns_query *query = dns_submit_a6ptr (ctx, &addr6, query_finished_ptr, L);
			if (!query)
			{
				return_error (L, ctx);
				lua_setfield (L, -3, "ptr_error");
				lua_setfield (L, -2, "ptr");
			}
			else
			{
				queries++;
				lua_pushboolean (L, 0);
				lua_setfield (L, -2, "ptr");
			}
		}
		else if (ret > 0)
		{
			struct dns_query *query = dns_submit_a4ptr (ctx, &addr, query_finished_ptr, L);
			if (!query)
			{
				return_error (L, ctx);
				lua_setfield (L, -3, "ptr_error");
				lua_setfield (L, -2, "ptr");
			}
			else
			{
				queries++;
				lua_pushboolean (L, 0);
				lua_setfield (L, -2, "ptr");
			}
		}
	}

	if (queries > 0)
	{
		reset_timeouts (L, ctx);
		return lua_yield (L, 2);
	}
	else
		return 1;
}
/* }}} */

/* ---- Lua-implemented Functions ------------------------------------------- */

/* {{{ wait() */
#define mydns_wait "return function (self, ...)\n" \
	"	if coroutine.yield('read', self) then\n" \
	"		return self:ioevent(...)\n" \
	"	else\n" \
	"		return self:timeouts(...)\n" \
	"	end\n" \
	"end\n"
/* }}} */

/* {{{ wait_all() */
#define mydns_wait_all "return function (self, ...)\n" \
	"	while true do\n" \
	"		self:wait(...)\n" \
	"	end\n" \
	"end\n"
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_dns() */
int luaopen_ratchet_dns (lua_State *L)
{
	/* Static functions in the ratchet.dns namespace. */
	static const luaL_Reg funcs[] = {
		{"new", mydns_new},
		{NULL}
	};

	/* Meta-methods for ratchet.dns object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", mydns_gc},
		{NULL}
	};

	/* Meta-methods for ratchet.dns object metatables written in Lua. */
	static const struct luafunc luametameths[] = {
		{"__call", mydns_wait_all},
		{NULL}
	};

	/* Methods in the ratchet.dns class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_fd", mydns_get_fd},
		{"get_timeout", mydns_get_timeout},
		{"submit", mydns_submit},
		/* Undocumented, helper methods. */
		{"ioevent", mydns_ioevent},
		{"timeouts", mydns_timeouts},
		{NULL}
	};

	/* Methods in the ratchet.dns class implemented in Lua. */
	static const struct luafunc luameths[] = {
		/* Documented methods. */
		{"wait", mydns_wait},
		/* Undocumented, helper methods. */
		{NULL}
	};

	dns_init (NULL, 0);

	/* Set up the ratchet.dns namespace functions. */
	luaI_openlib (L, "ratchet.dns", funcs, 0);

	/* Set up the ratchet.dns class and metatables. */
	luaL_newmetatable (L, "ratchet_dns_meta");
	lua_newtable (L);
	lua_pushvalue (L, -3);
	luaI_openlib (L, NULL, meths, 1);
	register_luafuncs (L, -1, luameths);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, metameths, 0);
	register_luafuncs (L, -1, luametameths);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
