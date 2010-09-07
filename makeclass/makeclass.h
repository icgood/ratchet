#ifndef __LUAH_MAKECLASS_H
#define __LUAH_MAKECLASS_H

#include <lua.h>
#include "misc.h"

#define luaH_setclassfield luaH_rawsetfield

void luaH_makecclass (lua_State *L, const luaL_Reg *meths);
int luaH_makeclass (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
