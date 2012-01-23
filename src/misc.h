#ifndef __RATCHET_MISC_H
#define __RATCHET_MISC_H

#include <sys/time.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define stackdump(L) stackdump_ln (L, __FILE__, __LINE__)

int strmatch (lua_State *L, int index, const char *match);
int strequal (lua_State *L, int index, const char *s2);
double fromtimeval (struct timeval *tv);
int gettimeval (double secs, struct timeval *tv);
int gettimeval_arg (lua_State *L, int index, struct timeval *tv);
int gettimeval_opt (lua_State *L, int index, struct timeval *tv);
double fromtimespec (struct timespec *tv);
int gettimespec (double secs, struct timespec *tv);
int gettimespec_arg (lua_State *L, int index, struct timespec *tv);
int gettimespec_opt (lua_State *L, int index, struct timespec *tv);
int set_nonblocking (int fd);
void stackdump_ln (lua_State *L, const char *file, int line);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
