#ifndef __RATCHET_MAKECLASS_H
#define __RATCHET_MAKECLASS_H

#include <lua.h>
#include "misc.h"

#define ratchet_setclassfield ratchet_rawsetfield

void ratchet_newclass (lua_State *L, const char *name, const luaL_Reg *meths, const luaL_Reg *funcs);
int ratchet_makeclass (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
