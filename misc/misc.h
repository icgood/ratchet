#ifndef __RATCHET_MISC_H
#define __RATCHET_MISC_H

#include <lua.h>

#define rhelp_perror(L) rhelp_perror_ln (L, __FILE__, __LINE__)
#define rhelp_stackdump(L) rhelp_stackdump_ln (L, __FILE__, __LINE__)

#define rhelp_setmethod(L, n, m) do { \
	lua_pushcfunction (L, m); \
	lua_setfield (L, -2, n); \
} while (1 == 0)

#define rhelp_dupvalue(L, i) do { \
	lua_pushvalue (L, i); \
	lua_insert (L, i); \
} while (1 == 0)

int rhelp_perror_ln (lua_State *L, const char *file, int line);
void rhelp_setfieldint (lua_State *L, int index, const char *name, int value);
void rhelp_rawsetfield (lua_State *L, int index, const char *key);
int rhelp_callfunction (lua_State *L, int index, int nargs);
int rhelp_callboolfunction (lua_State *L, int index, int nargs);
int rhelp_callmethod (lua_State *L, int index, const char *method, int nargs);
int rhelp_callboolmethod (lua_State *L, int index, const char *method, int nargs);
void rhelp_tableremoven (lua_State *L, int index, int n);
int rhelp_unpack (lua_State *L, int index);
int rhelp_strmatch (lua_State *L, const char *match);
int rhelp_strequal (lua_State *L, int index, const char *cmp);
void rhelp_stackdump_ln (lua_State *L, const char *file, int line);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
