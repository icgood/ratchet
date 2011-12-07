/* Copyright (c) 2010 Ian C. Good
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define _GNU_SOURCE
#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <event.h>
#include <netdb.h>
#include <string.h>

#include "ratchet.h"
#include "misc.h"

#define get_event_base(L, index) (*(struct event_base **) luaL_checkudata (L, index, "ratchet_meta"))
#define get_thread(L, index, s) luaL_checktype (L, index, LUA_TTHREAD); lua_State *s = lua_tothread (L, index)

const char *ratchet_version (void);

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet() */
int luaopen_ratchet (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{NULL}
	};

	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setglobal (L, "ratchet");

	luaL_requiref (L, "ratchet.kernel", luaopen_ratchet_kernel, 0);
	lua_setfield (L, -2, "kernel");

#if HAVE_SOCKET
	luaL_requiref (L, "ratchet.socket", luaopen_ratchet_socket, 0);
	lua_setfield (L, -2, "socket");
#endif
#if HAVE_OPENSSL
	luaL_requiref (L, "ratchet.ssl", luaopen_ratchet_ssl, 0);
	lua_setfield (L, -2, "ssl");
#endif
#if HAVE_ZMQ
	luaL_requiref (L, "ratchet.zmqsocket", luaopen_ratchet_zmqsocket, 0);
	lua_setfield (L, -2, "zmqsocket");
#endif
#if HAVE_DNS
	luaL_requiref (L, "ratchet.dns", luaopen_ratchet_dns, 0);
	lua_setfield (L, -2, "dns");
#endif
#if HAVE_TIMERFD
	luaL_requiref (L, "ratchet.timerfd", luaopen_ratchet_timerfd, 0);
	lua_setfield (L, -2, "timerfd");
#endif

	lua_pushstring (L, PACKAGE_VERSION);
	lua_setfield (L, -2, "version");

	return 1;
}
/* }}} */

/* {{{ ratchet_version() */
const char *ratchet_version (void)
{
	return PACKAGE_VERSION;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
