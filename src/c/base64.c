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

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <math.h>
#include <netdb.h>
#include <errno.h>

#if HAVE_OPENSSL
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#endif

#include "ratchet.h"
#include "misc.h"

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rb64_encode() */
static int rb64_encode (lua_State *L)
{
#if HAVE_OPENSSL
	size_t data_len = 0;
	const char *data = luaL_checklstring (L, 1, &data_len);

	BIO *b64, *mem;

	b64 = BIO_new (BIO_f_base64 ());
	BIO_set_flags (b64, BIO_FLAGS_BASE64_NO_NL);
	mem = BIO_new (BIO_s_mem ());
	mem = BIO_push (b64, mem);
	BIO_write (mem, data, data_len);
	(void) BIO_flush (mem);

	BUF_MEM *bptr;
	BIO_get_mem_ptr(b64, &bptr);

	lua_pushlstring (L, bptr->data, bptr->length);

	BIO_free_all (mem);

	return 1;
#else
	return luaL_error (L, "Compile ratchet with OpenSSL support for base64 support.")
#endif
}
/* }}} */

/* {{{ rb64_decode() */
static int rb64_decode (lua_State *L)
{
#if HAVE_OPENSSL
	size_t data_len = 0;
	const char *data = luaL_checklstring (L, 1, &data_len);
	luaL_Buffer inbuf;
	int len;
	BIO *b64, *mem;

	luaL_buffinit (L, &inbuf);

	b64 = BIO_new (BIO_f_base64 ());
	BIO_set_flags (b64, BIO_FLAGS_BASE64_NO_NL);
	mem = BIO_new_mem_buf ((void *) data, (int) data_len);
	mem = BIO_push (b64, mem);
	do
	{
		char *buf = luaL_prepbuffsize (&inbuf, 512);
		len = BIO_read (mem, buf, 512);
		if (len < 0)
			luaL_addsize (&inbuf, 0);
		else
			luaL_addsize (&inbuf, (size_t) len);
	} while (len > 0);

	BIO_free_all (mem);

	luaL_pushresult (&inbuf);

	return 1;
#else
	return luaL_error (L, "Compile ratchet with OpenSSL support for base64 support.")
#endif
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_base64() */
int luaopen_ratchet_base64 (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{"encode", rb64_encode},
		{"decode", rb64_decode},
		{NULL}
	};

	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_base64_class");

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
