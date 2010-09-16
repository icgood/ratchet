#ifndef __LUAH_MISC_H
#define __LUAH_MISC_H

#include <lua.h>

#define luaH_perror(L) luaH_perror_ln (L, __FILE__, __LINE__)
#define luaH_stackdump(L) luaH_stackdump_ln (L, __FILE__, __LINE__)

#define luaH_setmethod(L, n, m) do { \
	lua_pushcfunction (L, m); \
	lua_setfield (L, -2, n); \
} while (1 == 0)

#define luaH_dupvalue(L, i) do { \
	lua_pushvalue (L, i); \
	lua_insert (L, i); \
} while (1 == 0)

int luaH_perror_ln (lua_State *L, const char *file, int line);
void luaH_setfieldint (lua_State *L, int index, const char *name, int value);
void luaH_rawsetfield (lua_State *L, int index, const char *key);
int luaH_callfunction (lua_State *L, int index, int nargs);
int luaH_callboolfunction (lua_State *L, int index, int nargs);
int luaH_callmethod (lua_State *L, int index, const char *method, int nargs);
int luaH_callboolmethod (lua_State *L, int index, const char *method, int nargs);
int luaH_unpack (lua_State *L, int index);
int luaH_strmatch (lua_State *L, const char *match);
int luaH_strequal (lua_State *L, int index, const char *cmp);
void luaH_stackdump_ln (lua_State *L, const char *file, int line);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
