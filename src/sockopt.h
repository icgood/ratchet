#ifndef __RATCHET_SOCKOPT_H
#define __RATCHET_SOCKOPT_H

#include <lua.h>

int rsockopt_get (lua_State *L, const char *key, int fd);
int rsockopt_set (lua_State *L, const char *key, int fd, int valindex);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
