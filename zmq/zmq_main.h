#ifndef __RATCHET_ZMQ_H
#define __RATCHET_ZMQ_H

#include <lua.h>

#ifndef RATCHET_ZMQ_CONTEXT_REGISTRY
#define RATCHET_ZMQ_CONTEXT_REGISTRY "ratchet_zmq_default_context"
#endif

int luaopen_ratchet_zmq (lua_State *L);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
