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

#define DNS_GET_POLL_TIMEOUT(n) (pow (2.0, (double) n))

#define DNS_TXT_SIZE_REGISTRY_KEY "ratchet_dns_txt_size"

#define raise_dns_error(L, s, c, e) raise_dns_error_ln (L, s, c, e, __FILE__, __LINE__)
#define get_dns_res(L, i) (*(struct dns_resolver **) luaL_checkudata (L, i, "ratchet_dns_meta"))

size_t dns_ptr_qname(void *dst, size_t lim, int af, void *addr);

/* {{{ raise_dns_error_ln() */
static int raise_dns_error_ln (lua_State *L, const char *subj, const char *call, int e, const char *file, int line)
{
	lua_settop (L, 0);

	ratchet_error_push_constructor (L);

	lua_pushstring (L, subj);
	lua_pushliteral (L, ": ");
	lua_pushstring (L, dns_strerror (e));
	lua_concat (L, 3);

	ratchet_error_push_code (L, e);
	lua_pushnil (L);
	lua_pushstring (L, file);
	lua_pushinteger (L, line);
	lua_pushstring (L, call);
	lua_pushinteger (L, e);
	lua_call (L, 7, 1);

	return lua_error (L);
}
/* }}} */

/* {{{ arg_or_registry() */
static void *arg_or_registry (lua_State *L, int index, const char *reg_default, const char *checkudata)
{
	if (lua_isnoneornil (L, index))
	{
		lua_getfield (L, LUA_REGISTRYINDEX, reg_default);
		lua_replace (L, index);
	}

	return luaL_checkudata (L, index, checkudata);
}
/* }}} */

/* {{{ get_query_type() */
static enum dns_type get_query_type (lua_State *L, int index)
{
	static const char *lst[] = {
		"a",
		"aaaa",
		"mx",
		"ns",
		"cname",
		"soa",
		"srv",
		"ptr",
		"txt",
		"spf",
		"sshfp",
		NULL
	};
	static enum dns_type types[] = {
		DNS_T_A,
		DNS_T_AAAA,
		DNS_T_MX,
		DNS_T_NS,
		DNS_T_CNAME,
		DNS_T_SOA,
		DNS_T_SRV,
		DNS_T_PTR,
		DNS_T_TXT,
		DNS_T_SPF,
		DNS_T_SSHFP
	};

	const char *typestr = lua_tostring (L, index);

	int i;
	for (i=0; ; i++)
	{
		if (lst[i] == NULL)
			break;

		if (!strcasecmp (lst[i], typestr))
			return types[i];
	}

	return luaL_error (L, "unknown DNS query type: %s", typestr);
}
/* }}} */

/* {{{ query_name() */
static const char *query_name (enum dns_type type)
{
	if (type == DNS_T_A)
		return "A";

	else if (type == DNS_T_AAAA)
		return "AAAA";

	else if (type == DNS_T_MX)
		return "MX";

	else if (type == DNS_T_NS)
		return "NS";

	else if (type == DNS_T_CNAME)
		return "CNAME";

	else if (type == DNS_T_SOA)
		return "SOA";

	else if (type == DNS_T_SRV)
		return "SRV";

	else if (type == DNS_T_PTR)
		return "PTR";

	else if (type == DNS_T_TXT)
		return "TXT";

	else if (type == DNS_T_SPF)
		return "SPF";

	else if (type == DNS_T_SSHFP)
		return "SSHFP";

	else
		return "*unknown*";
}
/* }}} */

/* {{{ parse_XXX() */

/* {{{ parse_rr_a() */
static int parse_rr_a (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_a rec;
	int error = dns_a_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_a_parse", error);

	struct in_addr *addr = (struct in_addr *) lua_newuserdata (L, sizeof (struct in_addr));
	memcpy (addr, &rec.addr, sizeof (struct in_addr));
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr_aaaa() */
static int parse_rr_aaaa (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_aaaa rec;
	int error = dns_aaaa_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_aaaa_parse", error);

	struct in6_addr *addr = (struct in6_addr *) lua_newuserdata (L, sizeof (struct in6_addr));
	memcpy (addr, &rec.addr, sizeof (struct in6_addr));
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ insert_mx_priority_sorted() */
static void insert_mx_priority_sorted (lua_State *L, int index, int priority)
{
	int i, j;

	lua_getfield (L, index, "priorities");

	for (i=1; ; i++)
	{
		lua_rawgeti (L, -1, i);
		if (lua_isnil (L, -1))
		{
			lua_pop (L, 1);
			lua_pushinteger (L, priority);
			lua_rawseti (L, -2, i);
			break;
		}

		int n = (int) lua_tointeger (L, -1);
		if (n == priority)
		{
			lua_pop (L, 1);
			break;
		}
		else if (n > priority)
		{
			for (j=i+1; ; j++)
			{
				lua_rawgeti (L, -2, j);
				lua_pushvalue (L, -2);
				lua_rawseti (L, -4, j);
				lua_remove (L, -2);
				if (lua_isnil (L, -1))
					break;
			}
			lua_pop (L, 1);
			
			lua_pushinteger (L, priority);
			lua_rawseti (L, -2, i);
			break;
		}

		lua_pop (L, 1);
	}

	lua_pop (L, 1);
}
/* }}} */

/* {{{ parse_rr_mx() */
static int parse_rr_mx (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_mx rec;
	int error = dns_mx_parse (&rec, rr, answer), j;
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_mx_parse", error);

	luaL_getmetatable (L, "ratchet_dns_mx_meta");
	lua_setmetatable (L, -2);

	lua_getfield (L, -1, "priorities");
	if (lua_isnil (L, -1))
	{
		lua_newtable (L);
		lua_setfield (L, -3, "priorities");
	}
	lua_pop (L, 1);

	insert_mx_priority_sorted (L, -1, (int) rec.preference);

	lua_rawgeti (L, -1, (int) rec.preference);
	if (lua_isnil (L, -1))
	{
		lua_createtable (L, 1, 0);
		lua_pushstring (L, rec.host);
		lua_rawseti (L, -2, 1);
		lua_rawseti (L, -3, (int) rec.preference);
	}
	else
	{
		for (j=1; ; j++)
		{
			lua_rawgeti (L, -1, j);
			if (lua_isnil (L, -1))
			{
				lua_pop (L, 1);
				lua_pushstring (L, rec.host);
				lua_rawseti (L, -2, j);
				break;
			}
			lua_pop (L, 1);
		}
	}
	lua_pop (L, 1);

	/* Increment n. */
	lua_getfield (L, -1, "n");
	lua_pushinteger (L, lua_tointeger (L, -1) + 1);
	lua_setfield (L, -3, "n");
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ parse_rr_ns() */
static int parse_rr_ns (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_ns rec;
	int error = dns_ns_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_ns_parse", error);

	lua_pushstring (L, rec.host);
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr_cname() */
static int parse_rr_cname (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_cname rec;
	int error = dns_cname_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_cname_parse", error);

	lua_pushstring (L, rec.host);
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr_ptr() */
static int parse_rr_ptr (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	struct dns_ptr rec;
	int error = dns_ptr_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_ptr_parse", error);

	lua_pushstring (L, rec.host);
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr_txt() */
static int parse_rr_txt (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	lua_getfield (L, LUA_REGISTRYINDEX, DNS_TXT_SIZE_REGISTRY_KEY);
	size_t size = (size_t) lua_tonumber (L, -1);
	if (size == 0)
		size = DNS_TXT_MINDATA;

	struct dns_txt rec;
	dns_txt_init (&rec, size);
	int error = dns_txt_parse (&rec, rr, answer);
	if (error)
		return raise_dns_error (L, lua_tostring (L, 2), "dns_txt_parse", error);

	lua_pushlstring (L, (char *) rec.data, rec.len);
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr() */
static int parse_rr (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	if (rr->type == DNS_T_A)
		return parse_rr_a (L, rr, answer, i);

	else if (rr->type == DNS_T_AAAA)
		return parse_rr_aaaa (L, rr, answer, i);

	else if (rr->type == DNS_T_MX)
		return parse_rr_mx (L, rr, answer, i);

	else if (rr->type == DNS_T_NS)
		return parse_rr_ns (L, rr, answer, i);

	else if (rr->type == DNS_T_CNAME)
		return parse_rr_cname (L, rr, answer, i);

//	else if (rr->type == DNS_T_SOA)
//		return parse_rr_soa (L, rr, answer, i);

//	else if (rr->type == DNS_T_SRV)
//		return parse_rr_srv (L, rr, answer, i);

	else if (rr->type == DNS_T_PTR)
		return parse_rr_ptr (L, rr, answer, i);

	else if (rr->type == DNS_T_TXT)
		return parse_rr_txt (L, rr, answer, i);

	else if (rr->type == DNS_T_SPF)
		return parse_rr_txt (L, rr, answer, i);

//	else if (rr->type == DNS_T_SSHFP)
//		return parse_rr_sshfp (L, rr, answer, i);

	return luaL_error (L, "unimplemented DNS_T_*: %d", (int) rr->type);
}
/* }}} */

/* }}} */

/* {{{ check_special_XXXX() */

/* {{{ check_special_a() */
static int check_special_a (lua_State *L, const char *data)
{
	if (0 == strcmp ("*", data))
	{
		struct in_addr *addr = (struct in_addr *) lua_newuserdata (L, sizeof (struct in_addr));
		addr->s_addr = INADDR_ANY;
		return 1;
	}

	struct in_addr *addr = (struct in_addr *) lua_newuserdata (L, sizeof (struct in_addr));
	if (inet_pton (AF_INET, data, addr) > 0)
		return 1;
	else
		lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ check_special_aaaa() */
static int check_special_aaaa (lua_State *L, const char *data)
{
	if (0 == strcmp ("*", data))
	{
		lua_pushlightuserdata (L, (void *) &in6addr_any);
		return 1;
	}

	struct in6_addr *addr = (struct in6_addr *) lua_newuserdata (L, sizeof (struct in6_addr));
	if (inet_pton (AF_INET6, data, addr) > 0)
		return 1;
	else
		lua_pop (L, 1);

	return 0;
}
/* }}} */

/* {{{ check_special() */
static int check_special (lua_State *L, const char *data, enum dns_type type)
{
	if (type == DNS_T_A)
		return check_special_a (L, data);

	else if (type == DNS_T_AAAA)
		return check_special_aaaa (L, data);

	return 0;
}
/* }}} */

/* }}} */

/* {{{ ensure_arpa_string() */
static const char *ensure_arpa_string (lua_State *L, int index)
{
	const char *data = lua_tostring (L, index);

	if (!strstr (data, "arpa"))
	{
		union { struct in_addr a; struct in6_addr a6; } addr;
		union { struct dns_a a; struct dns_aaaa a6; } dns_addr;

		if (inet_pton (AF_INET, data, &addr.a) > 0)
		{
			char qname[DNS_D_MAXNAME + 1];
			memcpy (&dns_addr.a.addr, &addr.a, sizeof (struct in_addr));
			if (dns_ptr_qname (qname, DNS_D_MAXNAME+1, AF_INET, &dns_addr.a) > 0)
			{
				lua_pushstring (L, qname);
				lua_replace (L, index);
				return lua_tostring (L, 2);
			}
		}
		else if (inet_pton (AF_INET6, data, &addr.a6) > 0)
		{
			char qname[DNS_D_MAXNAME + 1];
			memcpy (&dns_addr.a6.addr, &addr.a6, sizeof (struct in6_addr));
			if (dns_ptr_qname (qname, DNS_D_MAXNAME+1, AF_INET6, &dns_addr.a6) > 0)
			{
				lua_pushstring (L, qname);
				lua_replace (L, index);
				return lua_tostring (L, 2);
			}
		}
	}
	
	return data;
}
/* }}} */

/* {{{ push_dns_resolver_table() */
static int push_dns_resolver_table (lua_State *L, size_t num, int resconf_idx, int hosts_idx, int timeout_idx)
{
	lua_createtable (L, (int) num, 0);

	size_t i;
	for (i=1; i<=num; i++)
	{
		/* Construct a new ratchet.dns object. */
		lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_dns_class");
		lua_getfield (L, -1, "new");
		lua_pushvalue (L, resconf_idx);
		lua_pushvalue (L, hosts_idx);
		lua_pushvalue (L, timeout_idx);
		lua_call (L, 3, 1);

		lua_rawseti (L, -3, i);
		lua_pop (L, 1);
	}

	return 1;
}
/* }}} */

/* {{{ submit_dns_queries() */
static int submit_dns_queries (lua_State *L, size_t num, int res_idx, int data_idx, int types_idx)
{
	size_t i;

	for (i=1; i<=num; i++)
	{
		lua_rawgeti (L, res_idx, i);

		lua_getfield (L, -1, "submit_query");
		lua_pushvalue (L, -2);
		lua_pushvalue (L, data_idx);
		lua_rawgeti (L, types_idx, i);
		lua_call (L, 3, 0);

		lua_pop (L, 1);
	}

	return 0;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ mydns_new() */
static int mydns_new (lua_State *L)
{
	lua_settop (L, 3);
	struct dns_resolv_conf *resconf = *(struct dns_resolv_conf **) arg_or_registry (L, 1, "ratchet_dns_resolv_conf_default", "ratchet_dns_resolv_conf_meta");
	struct dns_hosts *hosts = *(struct dns_hosts **) arg_or_registry (L, 2, "ratchet_dns_hosts_default", "ratchet_dns_hosts_meta");
	lua_Number expire_timeout = luaL_optnumber (L, 3, (lua_Number) 10.0);

	struct dns_resolver **new = (struct dns_resolver **) lua_newuserdata (L, sizeof (struct dns_resolver *));
	int error = 0;

	*new = dns_res_open (resconf, hosts, dns_hints_mortal (dns_hints_local (resconf, &error)), NULL, dns_opts (), &error);
	if (!*new)
		return raise_dns_error (L, "ratchet.dns.new()", "dns_res_open", error);

	luaL_getmetatable (L, "ratchet_dns_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 2);
	lua_pushinteger (L, 0);
	lua_setfield (L, -2, "tries");
	lua_pushnumber (L, expire_timeout);
	lua_setfield (L, -2, "expire_timeout");
	lua_setuservalue (L, -2);

	return 1;
}
/* }}} */

/* {{{ mydns_mx_get_i() */
static int mydns_mx_get_i (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TTABLE);
	int n = luaL_checkint (L, 2);

	lua_getfield (L, 1, "n");
	int max_n = lua_tonumber (L, -1);
	lua_pop (L, 1);
	if (n <= 0 || n > max_n)
		return 0;

	int i, j, k=1;
	lua_getfield (L, 1, "priorities");
	for (i=1; ; i++)
	{
		lua_rawgeti (L, -1, i);
		if (lua_isnil (L, -1))
			break;

		int p = lua_tonumber (L, -1);
		lua_pop (L, 1);
		lua_rawgeti (L, 1, p);
		for (j=1; ; j++)
		{
			lua_rawgeti (L, -1, j);
			if (lua_isnil (L, -1))
				break;
			if (k++ == n)
				return 1;
			lua_pop (L, 1);
		}
		lua_pop (L, 2);
	}
	lua_pop (L, 2);

	return 0;
}
/* }}} */

/* {{{ mydns_gc() */
static int mydns_gc (lua_State *L)
{
	struct dns_resolver *res = get_dns_res (L, 1);
	dns_res_close (res);

	return 0;
}
/* }}} */

/* {{{ mydns_get_fd() */
static int mydns_get_fd (lua_State *L)
{
	struct dns_resolver *res = get_dns_res (L, 1);
	lua_pushinteger (L, dns_res_pollfd (res));

	return 1;
}
/* }}} */

/* {{{ mydns_get_timeout() */
static int mydns_get_timeout (lua_State *L)
{
	(void) get_dns_res (L, 1);

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "tries");
	int tries = (int) lua_tointeger (L, -1);
	lua_pop (L, 2);
	lua_pushnumber (L, (lua_Number) (DNS_GET_POLL_TIMEOUT (tries)));

	return 1;
}
/* }}} */

/* {{{ mydns_query_collect_results() */
static int mydns_query_collect_results (lua_State *L)
{
	lua_getfield (L, 1, "is_query_done");
	lua_pushvalue (L, 1);
	lua_call (L, 1, 1);
	
	if (lua_toboolean (L, -1))
	{
		lua_pop (L, 1);

		lua_getfield (L, 1, "parse_answer");
		lua_pushvalue (L, 1);
		lua_pushvalue (L, 2);
		lua_pushvalue (L, 3);
		lua_call (L, 3, 2);
		return 2;
	}
	else
	{
		lua_pop (L, 1);

		lua_pushliteral (L, "read");
		lua_pushvalue (L, 1);
		return lua_yieldk (L, 2, 0, mydns_query_collect_results);
	}
}
/* }}} */

/* {{{ mydns_query() */
static int mydns_query (lua_State *L)
{
	int nargs = lua_gettop (L);

	/* Construct a new ratchet.dns object. */
	lua_getfield (L, LUA_REGISTRYINDEX, "ratchet_dns_class");
	lua_getfield (L, -1, "new");
	lua_insert (L, 3);
	lua_pop (L, 1);
	lua_call (L, nargs - 2, 1);
	lua_insert (L, 1);

	/* Call dns:submit_query(data, type). */
	lua_getfield (L, 1, "submit_query");
	lua_pushvalue (L, 1);
	lua_pushvalue (L, 2);
	lua_pushvalue (L, 3);
	lua_call (L, 3, 0);

	return mydns_query_collect_results (L);
}
/* }}} */

/* {{{ mydns_query_all_collect_results() */
static int mydns_query_all_collect_results (lua_State *L)
{
	size_t num = lua_rawlen (L, 1);
	size_t i;

	int tries = 0, not_done = 0;
	if (lua_getctx (L, &tries) == LUA_YIELD)
		lua_pop (L, 1);

	lua_pushliteral (L, "multi");
	lua_newtable (L);

	for (i=1; i<=num; i++)
	{
		lua_rawgeti (L, 1, i);
		if (!lua_toboolean (L, -1))
		{
			lua_pop (L, 1);
			continue;
		}

		lua_getfield (L, -1, "is_query_done");
		lua_pushvalue (L, -2);
		lua_call (L, 1, 1);
		int is_query_done = lua_toboolean (L, -1);
		lua_pop (L, 1);

		if (is_query_done)
		{
			lua_getfield (L, -1, "parse_answer");
			lua_pushvalue (L, -2);
			lua_pushvalue (L, 2);
			lua_rawgeti (L, 3, i);
			lua_call (L, 3, 2);

			/* Set the error to answers[type.."_error"]. */
			lua_rawgeti (L, 3, i);
			lua_pushliteral (L, "_error");
			lua_concat (L, 2);
			lua_pushvalue (L, -2);
			lua_rawset (L, 4);

			/* Set the result to answers[type]. */
			lua_rawgeti (L, 3, i);
			lua_pushvalue (L, -3);
			lua_rawset (L, 4);

			lua_pop (L, 3);
			
			lua_pushboolean (L, 0);
			lua_rawseti (L, 1, i);
		}
		else
			lua_rawseti (L, -2, ++not_done);
	}

	lua_pushnil (L);
	lua_pushnumber (L, (lua_Number) DNS_GET_POLL_TIMEOUT (tries));

	if (not_done > 0)
		return lua_yieldk (L, 4, tries+1, mydns_query_all_collect_results);
	else
	{
		lua_pop (L, 4);
		return 1;
	}
}
/* }}} */

/* {{{ mydns_query_all() */
static int mydns_query_all (lua_State *L)
{
	luaL_checkstring (L, 1);	/* Query data. */

	if (lua_type (L, 2) != LUA_TTABLE)
	{
		lua_createtable (L, 2, 0);
		lua_pushliteral (L, "aaaa");
		lua_rawseti (L, -2, 1);
		lua_pushliteral (L, "a");
		lua_rawseti (L, -2, 2);
		lua_replace (L, 2);
	}
	lua_settop (L, 5);

	size_t num_types = lua_rawlen (L, 2);

	push_dns_resolver_table (L, num_types, 3, 4, 5);
	lua_insert (L, 1);
	lua_settop (L, 3);
	lua_createtable (L, 0, num_types);	/* Answer table. */

	submit_dns_queries (L, num_types, 1, 2, 3);

	return mydns_query_all_collect_results (L);
}
/* }}} */

/* {{{ mydns_submit_query() */
static int mydns_submit_query (lua_State *L)
{
	struct dns_resolver *res = get_dns_res (L, 1);
	const char *data = luaL_checkstring (L, 2);
	enum dns_type type = get_query_type (L, 3);

	if (type == DNS_T_PTR)
		data = ensure_arpa_string (L, 2);

	int error = dns_res_submit (res, data, type, DNS_C_IN);
	if (error)
		return raise_dns_error (L, data, "dns_res_submit", error);

	return 0;
}
/* }}} */

/* {{{ mydns_is_query_done() */
static int mydns_is_query_done (lua_State *L)
{
	struct dns_resolver *res = get_dns_res (L, 1);

	int error = dns_res_check (res);
	if (error != EAGAIN)
		lua_pushboolean (L, 1);
	else
	{
		lua_getuservalue (L, 1);
		lua_getfield (L, -1, "tries");
		int tries = (int) lua_tointeger (L, -1);
		lua_getfield (L, -2, "expire_timeout");
		double et = (double) lua_tonumber (L, -1);
		lua_pop (L, 2);

		double elapsed = (double) dns_res_elapsed (res);
		if (elapsed >= et)
			lua_pushboolean (L, 1);
		else
		{
			lua_pushinteger (L, tries+1);
			lua_setfield (L, -2, "tries");
			
			lua_pushboolean (L, 0);
		}
	}

	return 1;
}
/* }}} */

/* {{{ mydns_parse_answer() */
static int mydns_parse_answer (lua_State *L)
{
	struct dns_resolver *res = get_dns_res (L, 1);
	const char *data = luaL_checkstring (L, 2);
	enum dns_type type = get_query_type (L, 3);
	int error = 0;

	lua_settop (L, 3);

	struct dns_packet *answer = dns_res_fetch (res, &error);
	if (!answer)
	{
		lua_pushnil (L);
		lua_pushstring (L, dns_strerror (error));
		return 2;
	}

	lua_newtable (L);

	int found = 0;
	struct dns_rr rr;
	dns_rr_foreach (&rr, answer, .sort = &dns_rr_i_packet)
	{
		if (DNS_S_ANSWER == rr.section && type == rr.type)
			parse_rr (L, &rr, answer, ++found);
	}

	if (!found)
	{
		/* Check for specials. */
		if (check_special (L, data, type))
			lua_rawseti (L, -2, 1);
		else
		{
			/* Query failed. */
			lua_pushnil (L);
			lua_pushvalue (L, 2);
			lua_pushliteral (L, " has no ");
			lua_pushstring (L, query_name (type));
			lua_pushliteral (L, " record");
			lua_concat (L, 4);
			return 2;
		}
	}
	
	return 1;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_dns() */
int luaopen_ratchet_dns (lua_State *L)
{
	/* Set up metatable for MX results. */
	luaL_newmetatable (L, "ratchet_dns_mx_meta");
	lua_createtable (L, 0, 1);
	lua_pushcfunction (L, mydns_mx_get_i);
	lua_setfield (L, -2, "get_i");
	lua_setfield (L, -2, "__index");
	lua_pop (L, 1);

	/* Static functions in the ratchet.dns namespace. */
	const luaL_Reg funcs[] = {
		/* Documented methods. */
		{"query", mydns_query},
		{"query_all", mydns_query_all},
		/* Undocumented, helper methods. */
		{"new", mydns_new},
		{NULL}
	};

	/* Meta-methods for ratchet.dns object metatables. */
	const luaL_Reg metameths[] = {
		{"__gc", mydns_gc},
		{NULL}
	};

	/* Methods in the ratchet.dns class. */
	const luaL_Reg meths[] = {
		/* Documented methods. */
		/* Undocumented, helper methods. */
		{"get_fd", mydns_get_fd},
		{"get_timeout", mydns_get_timeout},
		{"submit_query", mydns_submit_query},
		{"is_query_done", mydns_is_query_done},
		{"parse_answer", mydns_parse_answer},
		{NULL}
	};

	/* Set up the ratchet.dns class and metatables. */
	luaL_newmetatable (L, "ratchet_dns_meta");
	lua_newtable (L);
	luaL_setfuncs (L, meths, 0);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, metameths, 0);
	lua_pop (L, 1);

	/* Set up the ratchet.dns namespace functions. */
	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_dns_class");

	/* Load the resolv_conf and hosts sub-modules. */
	luaL_requiref (L, "ratchet.dns.resolv_conf", luaopen_ratchet_dns_resolv_conf, 0);
	lua_setfield (L, -2, "resolv_conf");

	luaL_requiref (L, "ratchet.dns.hosts", luaopen_ratchet_dns_hosts, 0);
	lua_setfield (L, -2, "hosts");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
