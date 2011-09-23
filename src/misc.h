#ifndef __RATCHET_MISC_H
#define __RATCHET_MISC_H

#include <sys/time.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define throw_perror(L) throw_perror_ln (L, __FILE__, __LINE__)

#if RATCHET_THROW_ERRORS
#define handle_perror(L) throw_perror_ln (L, __FILE__, __LINE__)
#define handle_error_str(L, ...) throw_error_str_ln (L, __FILE__, __LINE__, __VA_ARGS__)
#define handle_error_top(L) throw_error_top_ln (L, __FILE__, __LINE__)
#else
#define handle_perror(L) return_perror_ln (L, __FILE__, __LINE__)
#define handle_error_str(L, ...) return_error_str_ln (L, __FILE__, __LINE__, __VA_ARGS__)
#define handle_error_top(L) return_error_top_ln (L, __FILE__, __LINE__)
#endif

#define stackdump(L) stackdump_ln (L, __FILE__, __LINE__)

struct luafunc {
	const char *fname;
	char *fstr;
};

int return_perror_ln (lua_State *L, const char *file, int line);
int throw_perror_ln (lua_State *L, const char *file, int line);
int return_error_str_ln (lua_State *L, const char *file, int line, const char *err, ...);
int throw_error_str_ln (lua_State *L, const char *file, int line, const char *err, ...);
int return_error_top_ln (lua_State *L, const char *file, int line);
int throw_error__ln (lua_State *L, const char *file, int line);

void build_lua_function (lua_State *L, const char *fstr);
void register_luafuncs (lua_State *L, int index, const struct luafunc *fs);
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
