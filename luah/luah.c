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

#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "misc.h"
#include "makeclass.h"
#include "epoll.h"
#include "zmq_main.h"
#include "dns.h"
#include "ratchet.h"
#include "socket.h"

/* {{{ luaopen_luah() */
int luaopen_luah (lua_State *L)
{
	const luaL_Reg funcs[] = {
		{"makeclass", luaH_makeclass},
		{NULL}
	};

	luaL_register (L, "luah", funcs);

	luaopen_luah_ratchet (L);
	lua_setfield (L, -2, "ratchet");
	luaopen_luah_rlimit (L);
	lua_setfield (L, -2, "rlimit");
	luaopen_luah_zmq (L);
	lua_setfield (L, -2, "zmq");
	luaopen_luah_epoll (L);
	lua_setfield (L, -2, "epoll");
	luaopen_luah_dns (L);
	lua_setfield (L, -2, "dns");
	luaopen_luah_socket (L);
	lua_setfield (L, -2, "socket");
	luaopen_luah_xml (L);
	lua_setfield (L, -2, "xml");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
