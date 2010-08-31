#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "urls.h"

const char scheme_chars[] = "abcdefghijklmnopqrstuvwxyz"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"0123456789"
	"+.-";

/* {{{ luaH_parseurl_unix() */
static int luaH_parseurl_unix (lua_State *L, const char *unixfile)
{
	struct sockaddr_un *addr = (struct sockaddr_un *) lua_newuserdata (L, sizeof (struct sockaddr_un));

	addr->sun_family = AF_UNIX;
	strncpy (addr->sun_path, unixfile, sizeof (addr->sun_path) - 1);

	return 1;
}
/* }}} */

/* {{{ luaH_parseurl_ipv6() */
static int luaH_parseurl_ipv6 (lua_State *L, const char *rest)
{
	struct sockaddr_in6 *addr (struct sockaddr_in6 *) lua_newuserdata (L, sizeof (struct sockaddr_in6));

	addr->sin6_family = AF_INET6;

	return 1;
}
/* }}} */

/* {{{ luaH_parseurl_ipv4() */
static int luaH_parseurl_ipv4 (lua_State *L, const char *rest)
{
	struct sockaddr_in *addr (struct sockaddr_in *) lua_newuserdata (L, sizeof (struct sockaddr_in));

	addr->sin_family = AF_INET;

	return 1;
}
/* }}} */

/* {{{ luaH_parseurl() */
int luaH_parseurl (lua_State *L)
{
	const char *url = luaL_checkstring (L, 1);
	size_t scheme_len;

	scheme_len = strspn (url, scheme_chars);
	if (url[scheme_len] != ':')
		return luaL_error (L, "Invalid URL: <%s>", url);

	if (strncmp ("unix", url, scheme_len) == 0)
		return luaH_parseurl_unix (L, (const char *) (url+scheme_len+1));
	else if (strncmp ("ipv6", url, scheme_len) == 0)
		return luaH_parseurl_ipv6 (L, (const char *) (url+scheme_len+1));
	else
		return luaH_parseurl_ipv4 (L, (const char *) (url+scheme_len+1));
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
