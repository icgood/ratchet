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
	int t = lua_gettop (L) - nargs;
	lua_pushvalue (L, index);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ luaH_callboolfunction() */
int luaH_callboolfunction (lua_State *L, int index, int nargs)
{
	int ret;
	lua_pushvalue (L, index);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs, 1);
	ret = lua_toboolean (L, -1);
	lua_pop (L, 1);
	return ret;
}
/* }}} */

/* {{{ luaH_callmethod() */
int luaH_callmethod (lua_State *L, int index, const char *method, int nargs)
{
	int t = lua_gettop (L) - nargs;
	lua_pushvalue (L, index);
	lua_getfield (L, index, method);
	lua_insert (L, -nargs-2);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs+1, LUA_MULTRET);
	return lua_gettop (L) - t;
}
/* }}} */

/* {{{ luaH_callboolmethod() */
int luaH_callboolmethod (lua_State *L, int index, const char *method, int nargs)
{
	int ret;
	lua_pushvalue (L, index);
	lua_getfield (L, index, method);
	lua_insert (L, -nargs-2);
	lua_insert (L, -nargs-1);
	lua_call (L, nargs+1, 1);
	ret = lua_toboolean (L, -1);
	lua_pop (L, 1);
	return ret;
}
/* }}} */

/* {{{ luaH_unpack() */
int luaH_unpack (lua_State *L, int index)
{
	int i;
	int j = lua_objlen (L, index);
	for (i=1; i<=j; i++)
		lua_rawgeti (L, index, i);
	return j;
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

/* {{{ printf_index() */
static void printf_index (lua_State *L, int i)
{
	int t = lua_type (L, i);
	int j;

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
		case LUA_TTABLE: {
			printf ("<{");
			for (lua_pushnil (L); lua_next (L, i); lua_pop (L, 1))
			{
				int top = lua_gettop (L);
				printf_index (L, top-1);
				printf ("=");
				if (lua_istable (L, top))
				{
					for (j=1; j<=top-2; j++)
					{
						if (lua_equal (L, j, top))
						{
							printf ("<table:%p>", lua_topointer (L, top));
							break;
						}
					}
					if (j >= top-1)
						printf_index (L, top);
				}
				else
					printf_index (L, top);
				printf (",");
			}
			printf ("}:%p>", lua_topointer (L, i));
			break;
		}
		case LUA_TLIGHTUSERDATA:
		case LUA_TUSERDATA: {
			printf ("<%s", lua_typename (L, t));
			printf (":%p>", lua_topointer (L, i));
			break;
		}
		default: {
			printf ("%s", lua_typename (L, t));
			break;
		}
	}
}
/* }}} */

/* {{{ luaH_stackdump_ln() */
void luaH_stackdump_ln (lua_State *L, const char *file, int line)
{
	int i;
	int top = lua_gettop (L);

	printf ("------ stackdump %s:%d -------\n", file, line);
	for (i=1; i<=top; i++)
	{
		printf ("%d: ", i);
		printf_index (L, i);
		printf ("\n");
	}
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
