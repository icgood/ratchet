#ifndef __LUAH_MISC_H
#define __LUAH_MISC_H

#include <lua.h>

#define luaH_perror(L) luaH_perror_ln (L, __FILE__, __LINE__)

#define luaH_setmethod(L, i, n, m) do { \
	lua_pushcfunction (L, m); \
	lua_setfield (L, i-1, n); \
} while (1 == 0)

int luaH_perror_ln (lua_State *L, const char *file, int line);
void luaH_setfieldint (lua_State *L, int index, const char *name, int value);
void luaH_stackdump (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
