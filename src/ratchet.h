#ifndef __RATCHET_H
#define __RATCHET_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

const char *ratchet_version (void);

int luaopen_ratchet (lua_State *L);
int luaopen_ratchet_socket (lua_State *L);
int luaopen_ratchet_uri (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
