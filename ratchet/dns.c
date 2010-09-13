#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "dns.h"

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

/* {{{ ratchet_dns_getaddrinfo() */
static int ratchet_dns_getaddrinfo (lua_State *L)
{
	const char *host = luaL_checkstring (L, 1);
	const char *port = NULL;
	struct addrinfo *results, *it;
	int n = 0;

	if (lua_isstring (L, 2))
		port = lua_tostring (L, 2);

	int error = getaddrinfo (host, port, NULL, &results);
	if (error)
		return luaL_error (L, "DNS lookup failure: %s", gai_strerror (error));

	for (it = results; it != NULL; it = it->ai_next)
	{
		if (!lua_checkstack (L, 1))
			break;
		void *ud = lua_newuserdata (L, it->ai_addrlen);
		memcpy (ud, it->ai_addr, it->ai_addrlen);
		n++;
	}

	freeaddrinfo (results);

	return n;
}
/* }}} */

/* {{{ ratchet_dns_getnameinfo() */
static int ratchet_dns_getnameinfo (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TUSERDATA);
	struct sockaddr *sa = (struct sockaddr *) lua_touserdata (L, 1);
	size_t salen = lua_objlen (L, 1);
	char hostname[NI_MAXHOST] = "";
	char service[NI_MAXSERV] = "";

	int error = getnameinfo (sa, salen, hostname, NI_MAXHOST, service, NI_MAXSERV, 0);
	if (error)
		return luaL_error (L, "DNS lookup failure: %s", gai_strerror (error));

	lua_pushstring (L, hostname);
	lua_pushstring (L, service);

	return 2;
}
/* }}} */

/* {{{ luaopen_luah_ratchet_dns() */
int luaopen_luah_ratchet_dns (lua_State *L)
{
	const luaL_Reg funcs[] = {
		{"getaddrinfo", ratchet_dns_getaddrinfo},
		{"getnameinfo", ratchet_dns_getnameinfo},
		{NULL}
	};

	luaL_register (L, "luah.ratchet.dns", funcs);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
