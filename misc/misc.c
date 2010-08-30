#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "misc.h"

/* {{{ luaH_perror_ln() */
int luaH_perror_ln (lua_State *L, const char *file, int line)
{
	char errorbuf[512];

	if (errno)
	{
		if (strerror_r (errno, errorbuf, sizeof (errorbuf)) == -1)
			lua_pushfstring (L, "%s:%d: Unknown error occured. [errno=%d]", file, line, errno);
		else
			lua_pushfstring (L, "%s:%d: %s", file, line, errorbuf);
	}
	else
		lua_pushfstring (L, "%s:%d: Unknown error occured.", file, line);
	
	return lua_error (L);
}
/* }}} */

/* {{{ luaH_setfieldint() */
void luaH_setfieldint (lua_State *L, int index, const char *name, int value)
{
	lua_pushinteger (L, value);
	lua_setfield (L, index-1, name);
}
/* }}} */

/* {{{ luaH_stackdump() */
void luaH_stackdump (lua_State *L)
{
	int i;
	int top = lua_gettop (L);

	for (i=1; i<=top; i++)
	{
		int t = lua_type (L, i);
		switch (t)
		{
			case LUA_TSTRING: {
				printf ("'%s'", lua_tostring (L, i));
				break;
			}
			case LUA_TBOOLEAN: {
				printf (lua_toboolean (L, 1) ? "true" : "false");
				break;
			}
			case LUA_TNUMBER: {
				printf ("%g", lua_tonumber (L, i));
				break;
			}
			default: {
				printf ("%s", lua_typename (L, t));
				break;
			}
		}
		printf ("  ");
	}
	printf ("\n");
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
