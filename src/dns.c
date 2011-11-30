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

#include "luaopens.h"
#include "misc.h"
#include "libdns/dns.h"

#define DNS_GET_POLL_TIMEOUT(n) (pow (2.0, (double) n))

#ifndef DNS_QUERY_DEFAULT
#ifndef DNS_QUERY_IPV6_DEFAULT
#define DNS_QUERY_DEFAULT "ipv4"
#else
#define DNS_QUERY_DEFAULT "ipv6"
#endif
#endif

#define DNS_RESOLV_CONF_DEFAULT "ratchet_dns_resolv_conf_default"
#define DNS_HOSTS_DEFAULT "ratchet_dns_hosts_default"
#define DNS_TXT_SIZE_REGISTER "ratchet_dns_txt_size"

size_t dns_ptr_qname(void *dst, size_t lim, int af, void *addr);

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
		"ipv4",
		"ipv6",
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
	return types[luaL_checkoption (L, index, DNS_QUERY_DEFAULT, lst)];
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
		return handle_error_str (L, "dns_a_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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
		return handle_error_str (L, "dns_aaaa_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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
		return handle_error_str (L, "dns_mx_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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
		return handle_error_str (L, "dns_ns_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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
		return handle_error_str (L, "dns_cname_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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
		return handle_error_str (L, "dns_ptr_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

	lua_pushstring (L, rec.host);
	lua_rawseti (L, -2, i);

	return 0;
}
/* }}} */

/* {{{ parse_rr_txt() */
static int parse_rr_txt (lua_State *L, struct dns_rr *rr, struct dns_packet *answer, int i)
{
	lua_getfield (L, LUA_REGISTRYINDEX, DNS_TXT_SIZE_REGISTER);
	size_t size = (size_t) lua_tonumber (L, -1);
	if (size == 0)
		size = DNS_TXT_MINDATA;

	struct dns_txt rec;
	dns_txt_init (&rec, size);
	int error = dns_txt_parse (&rec, rr, answer);
	if (error)
		return handle_error_str (L, "dns_txt_parse: %s: %s", lua_tostring (L, 2), dns_strerror (error));

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

	return handle_error_str (L, "unimplemented DNS_T_*: %d", (int) rr->type);
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

/* {{{ ratchet_dns_resolv_conf_meta */

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
		return handle_error_top (L);

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

/* {{{ setup_dns_resolv_conf() */
static int setup_dns_resolv_conf (lua_State *L)
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
	lua_setfield (L, LUA_REGISTRYINDEX, DNS_RESOLV_CONF_DEFAULT);

	return 1;
}
/* }}} */

/* }}} */

/* {{{ ratchet_dns_hosts_meta */

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
		return handle_error_top (L);

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

/* {{{ setup_dns_hosts() */
static int setup_dns_hosts (lua_State *L)
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
	lua_setfield (L, LUA_REGISTRYINDEX, DNS_HOSTS_DEFAULT);

	return 1;
}
/* }}} */

/* }}} */

/* {{{ ratchet_dns_mx_meta */

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

/* {{{ setup_dns_mx() */
static int setup_dns_mx (lua_State *L)
{
	/* Set up metatable for MX results. */
	luaL_newmetatable (L, "ratchet_dns_mx_meta");
	lua_createtable (L, 0, 1);
	lua_pushcfunction (L, mydns_mx_get_i);
	lua_setfield (L, -2, "get_i");
	lua_setfield (L, -2, "__index");
	lua_pop (L, 1);

	return 0;
}
/* }}} */

/* }}} */

/* {{{ ratchet_dns_meta */

#define get_dns_res(L, i) (*(struct dns_resolver **) luaL_checkudata (L, i, "ratchet_dns_meta"))

/* {{{ mydns_new() */
static int mydns_new (lua_State *L)
{
	lua_settop (L, 3);
	struct dns_resolv_conf *resconf = *(struct dns_resolv_conf **) arg_or_registry (L, 1, DNS_RESOLV_CONF_DEFAULT, "ratchet_dns_resolv_conf_meta");
	struct dns_hosts *hosts = *(struct dns_hosts **) arg_or_registry (L, 2, DNS_HOSTS_DEFAULT, "ratchet_dns_hosts_meta");
	lua_Number expire_timeout = luaL_optnumber (L, 3, (lua_Number) 10.0);

	struct dns_resolver **new = (struct dns_resolver **) lua_newuserdata (L, sizeof (struct dns_resolver *));
	int error = 0;

	*new = dns_res_open (resconf, hosts, dns_hints_mortal (dns_hints_local (resconf, &error)), NULL, dns_opts (), &error);
	if (!*new)
		return handle_error_str (L, "dns_res_open: %s", dns_strerror (error));

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

/* {{{ mydns_query() */
#define mydns_query "return function (data, type, ...)\n" \
	"	local dns = ratchet.dns.new(...)\n" \
	"	dns:submit_query(data, type)\n" \
	"	while not dns:is_query_done() do\n" \
	"		coroutine.yield('read', dns)\n" \
	"	end\n" \
	"	return dns:parse_answer(data, type)\n" \
	"end\n"
/* }}} */

/* {{{ mydns_query_all() */
#define mydns_query_all "return function (data, types, ...)\n" \
	"	local dnsobjs, answers = {}, {}\n" \
	"	if not types then\n" \
	"		types = ratchet.dns.default_types\n" \
	"	end\n" \
	"	for i, t in ipairs(types) do\n" \
	"		local dnsobj = ratchet.dns.new(...)\n" \
	"		rawset(dnsobjs, t, dnsobj)\n" \
	"		dnsobj:submit_query(data, t)\n" \
	"	end\n" \
	"	for t, dnsobj in pairs(dnsobjs) do\n" \
	"		while not dnsobj:is_query_done() do\n" \
	"			coroutine.yield('read', dnsobj)\n" \
	"		end\n" \
	"		answers[t], answers[t..'_error'] = dnsobj:parse_answer(data, t)\n" \
	"	end\n" \
	"	return answers\n" \
	"end\n"
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
		return handle_error_str (L, "dns_res_submit: %s: %s", data, dns_strerror (error));

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

/* {{{ setup_dns() */
static int setup_dns (lua_State *L)
{
	/* Static functions in the ratchet.dns namespace. */
	const luaL_Reg funcs[] = {
		/* Documented methods. */
		/* Undocumented, helper methods. */
		{"new", mydns_new},
		{NULL}
	};

	/* Static functions implemented in Lua in the ratchet.dns namespace. */
	const struct luafunc luafuncs[] = {
		/* Documented methods. */
		{"query", mydns_query},
		{"query_all", mydns_query_all},
		/* Undocumented, helper methods. */
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
	register_luafuncs (L, -1, luafuncs);

	/* Set up {'ipv6', 'ipv4'} as the default query types. */
	lua_createtable (L, 2, 0);
	lua_pushliteral (L, "ipv6");
	lua_rawseti (L, -2, 1);
	lua_pushliteral (L, "ipv4");
	lua_rawseti (L, -2, 2);
	lua_setfield (L, -2, "default_types");

	return 1;
}
/* }}} */

/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_dns() */
int luaopen_ratchet_dns (lua_State *L)
{
	setup_dns_mx (L);
	setup_dns (L);

	setup_dns_resolv_conf (L);
	lua_setfield (L, -2, "resolv_conf");

	setup_dns_hosts (L);
	lua_setfield (L, -2, "hosts");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
