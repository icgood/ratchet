#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "parseuri.h"

#define IPV4_IP_PATTERN 

/* {{{ unix_endpoint() */
static int unix_endpoint (lua_State *L)
{
	struct sockaddr_un *addr = (struct sockaddr_un *) lua_newuserdata (L, sizeof (struct sockaddr_un));
	memset (addr, 0, sizeof (struct sockaddr_un));

	addr->sun_family = AF_UNIX;
	strncpy (addr->sun_path, lua_tostring (L, 1), sizeof (addr->sun_path) - 1);

	return 1;
}
/* }}} */

/* {{{ tcp_endpoint() */
static int tcp_endpoint (lua_State *L)
{
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *) lua_newuserdata (L, sizeof (struct sockaddr_in6));
	memset (addr, 0, sizeof (struct sockaddr_in6));

	addr->sin6_family = AF_INET6;
	lua_pushvalue (L, -2);

	if (luaH_strmatch (L, "^%/+(%d%d?%d?%.%d%d?%d?%.%d%d?%d?%.%d%d?%d?)(%:?)(%d*)$"))
	{
		if (luaH_strequal (L, -2, ":"))
			addr->sin6_port = htons ((uint16_t) atoi (lua_tostring (L, -1)));
		lua_pop (L, 2);
		lua_pushstring (L, "::FFFF:");
		lua_insert (L, -2);
		lua_concat (L, 2);
	}

	else if (luaH_strmatch (L, "^%/+%[(.-)%]%(%:?)(%d*)$"))
	{
		if (luaH_strequal (L, -2, ":"))
			addr->sin6_port = htons ((uint16_t) atoi (lua_tostring (L, -1)));
		lua_pop (L, 2);
	}

	else if (luaH_strmatch (L, "^%/+(.*)$")) { }

	else
	{
		lua_pop (L, 2);
		lua_pushnil (L);
		return 1;
	}

	if (1 != inet_pton (AF_INET6, lua_tostring (L, -1), &addr->sin6_addr))
	{
		lua_pop (L, 3);
		lua_pushnil (L);
		return 1;
	}
	lua_pop (L, 2);

	return 1;
}
/* }}} */

/* {{{ luaH_parseuri() */
int luaH_parseuri (lua_State *L)
{
	const char *url = luaL_checkstring (L, 1);
	luaL_checktype (L, 2, LUA_TTABLE);

	lua_pushvalue (L, 1);
	if (!luaH_strmatch (L, "^([%w%+%.%-]+):(.*)$"))
	{
		lua_pushstring (L, "tcp");
		lua_pushfstring (L, "//%s", url);
	}
	lua_remove (L, -3);

	lua_pushvalue (L, -2);
	lua_gettable (L, 2);
	if (lua_isnil (L, -1))
		return luaL_error (L, "Unknown URL scheme: <%s>", url);
	lua_insert (L, -2);

	lua_call (L, 1, 1);
	return 2;
}
/* }}} */

/* {{{ luaH_parseuri_add_builtin() */
void luaH_parseuri_add_builtin (lua_State *L)
{
	lua_pushcfunction (L, unix_endpoint);
	lua_setfield (L, -2, "unix");
	lua_pushcfunction (L, tcp_endpoint);
	lua_setfield (L, -2, "tcp");
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
