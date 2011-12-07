#ifndef __RATCHET_H
#define __RATCHET_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "dns/dns.h"

const char *ratchet_version (void);

int luaopen_ratchet (lua_State *L);
int luaopen_ratchet_kernel (lua_State *L);
int luaopen_ratchet_socket (lua_State *L);
int luaopen_ratchet_ssl (lua_State *L);
int luaopen_ratchet_timerfd (lua_State *L);
int luaopen_ratchet_zmqsocket (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
