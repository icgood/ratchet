#ifndef __RATCHET_LUAOPENS_H
#define __RATCHET_LUAOPENS_H

#include <lua.h>

int luaopen_ratchet_dns (lua_State *L);
int luaopen_ratchet (lua_State *L);
int luaopen_ratchet_socket (lua_State *L);
int luaopen_ratchet_timerfd (lua_State *L);
int luaopen_ratchet_zmqsocket (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
