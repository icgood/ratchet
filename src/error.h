#ifndef __RATCHET_ERROR_H
#define __RATCHET_ERROR_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define rerror_perror(L, f, s) rerror_perror_ln (L, f, s, __FILE__, __LINE__)
#define rerror_error_top(L, f, c) rerror_error_top_ln (L, f, c, __FILE__, __LINE__)
#define rerror_error(L, f, c, ...) rerror_error_ln (L, f, c, __FILE__, __LINE__, __VA_ARGS__)

void rerror_push_constructor (lua_State *L);
void rerror_push_code (lua_State *L, int e);
int rerror_perror_ln (lua_State *L, const char *function, const char *syscall, const char *file, int line);
int rerror_error_top_ln (lua_State *L, const char *function, const char *code, const char *file, int line);
int rerror_error_ln (lua_State *L, const char *function, const char *code, const char *file, int line, const char *description, ...);

#endif
// vim:foldmethod=marker:ai:ts=4:sw=4:
