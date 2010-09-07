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

/* {{{ luaH_rawsetfield() */
void luaH_rawsetfield (lua_State *L, int index, const char *key)
{
	lua_pushvalue (L, index);
	lua_pushstring (L, key);
	lua_pushvalue (L, -3);
	lua_rawset (L, -3);
	lua_pop (L, 2);
}
/* }}} */

/* {{{ luaH_callfunction() */
int luaH_callfunction (lua_State *L, int index, int nargs)
{
	int t = lua_gettop (L);
	lua_pushvalue (L, index);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ luaH_callmethod() */
int luaH_callmethod (lua_State *L, int index, const char *method, int nargs)
{
	int t = lua_gettop (L);
	lua_pushvalue (L, index);
	lua_getfield (L, index, method);
	lua_insert (L, -2);
	lua_insert (L, -nargs-2);
	lua_insert (L, -nargs-2);
	lua_call (L, nargs+1, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ luaH_strmatch() */
int luaH_strmatch (lua_State *L, const char *match)
{
	lua_pushstring (L, match);
	int rets = luaH_callmethod (L, -2, "match", 1);
	if (lua_isnil (L, -1))
	{
		lua_pop (L, rets);
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ luaH_strequal() */
int luaH_strequal (lua_State *L, int index, const char *cmp)
{
	int ret;

	lua_pushvalue (L, index);
	lua_pushstring (L, cmp);
	ret = lua_equal (L, -2, -1);
	lua_pop (L, 2);

	return ret;
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
