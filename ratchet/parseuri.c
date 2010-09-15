#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "parseuri.h"
#include "dns.h"

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
	lua_pushvalue (L, 1);

	if (luaH_strmatch (L, "^%/+%[(.-)%](%:?)(%d*)$"))
	{
		if (!luaH_strequal (L, -2, ":") || luaH_strequal (L, -1, ""))
		{
			lua_pop (L, 1);
			lua_pushnil (L);
		}
		lua_remove (L, -2);
	}

	else if (luaH_strmatch (L, "^%/+([^%:]*)(%:?)(%d*)$"))
	{
		if (!luaH_strequal (L, -2, ":") || luaH_strequal (L, -1, ""))
		{
			lua_pop (L, 1);
			lua_pushnil (L);
		}
		lua_remove (L, -2);
	}

	else if (luaH_strmatch (L, "^%/+(.*)$"))
		lua_pushnil (L);

	else
	{
		lua_pop (L, 1);
		lua_pushnil (L);
		return 1;
	}

	luaopen_luah_dns (L);
	lua_getfield (L, -1, "getaddrinfo");
	lua_remove (L, -2);
	lua_insert (L, -3);
	lua_call (L, 2, 1);

	/* Initialize named parameter table for socket class constructor. */
	lua_newtable (L);
	lua_insert (L, -2);
	lua_setfield (L, -2, "sockaddr");

	return 1;
}
/* }}} */

/* {{{ zmq_endpoint() */
static int zmq_endpoint (lua_State *L)
{
	lua_settop (L, 1);
	if (luaH_strmatch (L, "^([^%:]*)%:(.*)$"))
	{
		luaH_callmethod (L, 2, "upper");
		lua_replace (L, 2);
	}
	else
	{
		/* Use the original string as endpoint and nil as type. */
		lua_pushnil (L);
		lua_pushvalue (L, 1);
	}

	lua_newtable (L);
	lua_insert (L, 2);
	lua_setfield (L, 2, "endpoint");
	lua_setfield (L, 2, "type");

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
		lua_pushliteral (L, "tcp");
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
	lua_pushcfunction (L, zmq_endpoint);
	lua_setfield (L, -2, "zmq");
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
