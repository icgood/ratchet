#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <sys/resource.h>

#include "misc.h"
#include "rlimit.h"

/* {{{ myrlimit_get() */
static int myrlimit_get (lua_State *L)
{
	int resource = luaL_checkint (L, 1);
	struct rlimit l;

	if (getrlimit (resource, &l) < 0)
		return luaH_perror (L);

	lua_pushnumber (L, (lua_Number) l.rlim_cur);
	lua_pushnumber (L, (lua_Number) l.rlim_max);

	return 2;
}
/* }}} */

/* {{{ myrlimit_set() */
static int myrlimit_set (lua_State *L)
{
	int resource = luaL_checkint (L, 1);
	lua_Number soft = luaL_checknumber (L, 2);
	lua_Number hard = luaL_checknumber (L, 3);
	struct rlimit l = {(rlim_t) soft, (rlim_t) hard};

	if (setrlimit (resource, &l) < 0)
		return luaH_perror (L);

	return 0;
}
/* }}} */

#define set_rlimit_int(s) lua_pushinteger(L, RLIMIT_##s); lua_setfield(L, -2, #s);

/* {{{ luaopen_luah_rlimit() */
int luaopen_luah_rlimit (lua_State *L)
{
	const luaL_Reg funcs[] = {
		{"get", myrlimit_get},
		{"set", myrlimit_set},
		{NULL}
	};

	luaL_register (L, "luah.rlimit", funcs);

	set_rlimit_int (AS);
	set_rlimit_int (CORE);
	set_rlimit_int (CPU);
	set_rlimit_int (DATA);
	set_rlimit_int (FSIZE);
#ifdef RLIMIT_LOCKS
	set_rlimit_int (LOCKS);
#endif
	set_rlimit_int (MEMLOCK);
#ifdef RLIMIT_MSGQUEUE
	set_rlimit_int (MSGQUEUE);
#endif
#ifdef RLIMIT_NICE
	set_rlimit_int (NICE);
#endif
	set_rlimit_int (NOFILE);
	set_rlimit_int (NPROC);
	set_rlimit_int (RSS);
#ifdef RLIMIT_RTPRIO
	set_rlimit_int (RTPRIO);
#endif
#ifdef RLIMIT_RTTIME
	set_rlimit_int (RTTIME);
#endif
#ifdef RLIMIT_SIGPENDING
	set_rlimit_int (SIGPENDING);
#endif
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
