#ifndef __LUAH_H
#define __LUAH_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <luah/misc.h>

int luaopen_luah (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
