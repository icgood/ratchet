#ifndef __RATCHET_MISC_H
#define __RATCHET_MISC_H

#include <sys/time.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define raise_perror(L) raise_perror_ln (L, __FILE__, __LINE__)
#define return_perror(L) return_perror_ln (L, __FILE__, __LINE__)
#define stackdump(L) stackdump_ln (L, __FILE__, __LINE__)

#ifdef RATCHET_RETURN_ERRORS
#define handle_perror return_perror
#else
#define handle_perror raise_perror
#endif

struct luafunc {
	const char *fname;
	char *fstr;
};

int raise_perror_ln (lua_State *L, const char *file, int line);
int return_perror_ln (lua_State *L, const char *file, int line);
void build_lua_function (lua_State *L, const char *fstr);
void register_luafuncs (lua_State *L, int index, const struct luafunc *fs);
int strmatch (lua_State *L, int index, const char *match);
int strequal (lua_State *L, int index, const char *s2);
int gettimeval (double secs, struct timeval *tv);
int gettimeval_arg (lua_State *L, int index, struct timeval *tv);
int gettimeval_opt (lua_State *L, int index, struct timeval *tv);
int set_nonblocking (int fd);
int set_reuseaddr (int fd);
void stackdump_ln (lua_State *L, const char *file, int line);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
