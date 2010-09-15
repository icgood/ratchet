#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>
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

/* {{{ mydns_getaddrinfo() */
static int mydns_getaddrinfo (lua_State *L)
{
	const char *host = NULL, *port = NULL;
	struct addrinfo hints, *results, *it;
	int n = 0;

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG | AI_NUMERICSERV);

	if (lua_isstring (L, 1))
		host = lua_tostring (L, 1);
	if (lua_isstring (L, 2))
		port = lua_tostring (L, 2);

	if (strcmp ("*", host) == 0)
	{
		hints.ai_flags |= AI_PASSIVE;
		host = NULL;
	}

	int error = getaddrinfo (host, port, &hints, &results);
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

/* {{{ mydns_getnameinfo() */
static int mydns_getnameinfo (lua_State *L)
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

/* {{{ luaopen_luah_dns() */
int luaopen_luah_dns (lua_State *L)
{
	const luaL_Reg funcs[] = {
		{"getaddrinfo", mydns_getaddrinfo},
		{"getnameinfo", mydns_getnameinfo},
		{NULL}
	};

	luaL_register (L, "luah.dns", funcs);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
