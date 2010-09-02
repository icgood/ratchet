#ifndef __LUAH_RATCHET_PARSEURI_H
#define __LUAH_RATCHET_PARSEURI_H

#include <lua.h>

void luaH_parseuri_add_builtin (lua_State *L);
int luaH_parseuri (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
