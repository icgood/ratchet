#ifndef __rhelp_MAKECLASS_H
#define __rhelp_MAKECLASS_H

#include <lua.h>
#include "misc.h"

#define rhelp_setclassfield rhelp_rawsetfield

void rhelp_newclass (lua_State *L, const char *name, const luaL_Reg *meths, const luaL_Reg *funcs);
int rhelp_makeclass (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
