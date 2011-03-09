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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "misc.h"

/* {{{ password_cb() */
static int password_cb (char *buf, int size, int rwflag, void *userdata)
{
	const char *password = (char *) userdata;
	int plen = strlen (password);

	if (size < plen+1)
		return 0;

	strcpy (buf, password);
	return plen;
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rssl_new() */
static int rssl_new (lua_State *L)
{
	const char *keyfile = luaL_checkstring (L, 1);
	const char *password = luaL_optstring (L, 2, NULL);
	const char *ca_file = luaL_optstring (L, 3, NULL);
	const char *ca_path = luaL_optstring (L, 4, NULL);
	if (!ca_file && !ca_path)
		return luaL_argerror (L, 3, "CA-file or CA-path required");
	const char *random = luaL_optstring (L, 5, NULL);
	int depth = luaL_optint (L, 6, 1);
	int rand_max_bytes = luaL_optint (L, 7, 1048576);

	SSL_METHOD *meth;
	SSL_CTX *ctx;

	/* Create context. */
	meth = SSLv3_method ();
	ctx = SSL_CTX_new (meth);
	if (!ctx)
		return luaL_error (L, "Creation of SSL_CTX object failed");

	/* Load keys and certificates. */
	if (!SSL_CTX_use_certificate_chain_file (ctx, keyfile))
		return luaL_error (L, "Could not read certificate chain file: %s", keyfile);
	SSL_CTX_set_default_passwd_cb (ctx, password_cb);
	SSL_CTX_set_default_passwd_cb_userdata (ctx, password);
	if (!SSL_CTX_use_PrivateKey_file (ctx, keyfile, SSL_FILETYPE_PEM))
		return luaL_error (L, "Could not read key file: %s", keyfile);

	/* Load the CAs we trust. */
	if (!SSL_CTX_load_verify_locations (ctx, ca_file, ca_path))
	{
		if (ca_path && ca_file)
			return luaL_error (L, "Could not read CA locations: %s %s", ca_file, ca_path);
		else if (ca_path)
			return luaL_error (L, "Could not read CA path: %s", ca_path);
		else
			return luaL_error (L, "Could not read CA file: %s", ca_file);
	}
	SSL_CTX_set_verify_depth (ctx, depth);

	/* Load randomness. */
	char buffer[2048];
	if (!random)
		random = RAND_file_name (buffer, 2048);
	if (!random)
		return luaL_argerror (L, 5, "Could not find default random file");
	if (!RAND_load_file (random, rand_max_bytes))
		return luaL_error (L, "Could not load randomness: %s", random);

	/* Set up Lua object. */
	SSL_CTX **new = (SSL_CTX **) lua_newuserdata (L, sizeof (SSL_CTX *));
	*new = ctx;

	luaL_getmetatable (L, "ratchet_ssl_ctx_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "password");
	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rssl_ctx_gc() */
static int rssl_ctx_gc (lua_State *L)
{
	SSL_CTX **ctx = (SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	if (*ctx)
		SSL_CTX_free (*ctx);
	*ctx = NULL;

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_create_session() */
static int rssl_ctx_create_session (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	luaL_checkany (L, 2);
	luaL_checktype (L, 3, LUA_TLIGHTUSERDATA);
	BIO *rbio = (BIO *) lua_topointer (L, 3);
	BIO *wbio = rbio;
	if (!lua_isnoneornil (L, 4))
	{
		luaL_checktype (L, 4, LUA_TLIGHTUSERDATA);
		wbio = (BIO *) lua_topointer (L, 4);
	}

	SSL *ssl = SSL_new (ctx);
	if (!ssl)
		return luaL_error (L, "Could not create SSL object");
	SSL_set_bio (ssl, rbio, wbio);

	/* Set up Lua object. */
	SSL **new = (SSL **) lua_newuserdata (L, sizeof (SSL *));
	*new = ssl;

	luaL_getmetatable (L, "ratchet_ssl_session_meta");
	lua_setmetatable (L, -2);

	/* Save the engine for later. */
	lua_createtable (L, 0, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "engine");
	lua_setfenv (L, -2);

	return 1;
}
/* }}} */

/* {{{ rssl_session_gc() */
static int rssl_session_gc (lua_State *L)
{
	SSL **session = (SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");
	if (*session)
		SSL_free (*session);
	*session = NULL;

	return 0;
}
/* }}} */

/* {{{ rssl_session_get_engine() */
static int rssl_session_get_engine (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	lua_getfenv (L, 1);
	lua_getfield (L, -1, "engine");

	return 1;
}
/* }}} */

/* {{{ rssl_session_rawread() */
static int rssl_session_rawread (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");
	luaL_Buffer buffer;

	luaL_buffinit (L, &buffer);
	char *prepped = luaL_prepbuffer (&buffer);

	int ret = SSL_read (session, prepped, LUAL_BUFFERSIZE);
	int error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			luaL_addsize (&buffer, (size_t) ret);
			luaL_pushresult (&buffer);
			return 1;

		case SSL_ERROR_ZERO_RETURN:
			lua_pushnil (L);
			lua_pushliteral (L, "shutdown");
			return 2;

		case SSL_ERROR_WANT_READ:
			lua_pushnil (L);
			lua_pushliteral (L, "read");
			return 2;

		case SSL_ERROR_WANT_WRITE:
			lua_pushnil (L);
			lua_pushliteral (L, "write");
			return 2;

		default:
			return luaL_error (L, "SSL_read: %s", ERR_error_string (error, NULL));
	}

	return 0;
}
/* }}} */

/* {{{ rssl_session_rawwrite() */
static int rssl_session_rawwrite (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");
	size_t size;
	const char *data = luaL_checklstring (L, 2, &size);

	int ret = SSL_write (session, data, (int) size);
	int error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			lua_pushboolean (L, 1);
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushnil (L);
			lua_pushliteral (L, "read");
			return 2;

		case SSL_ERROR_WANT_WRITE:
			lua_pushnil (L);
			lua_pushliteral (L, "write");
			return 2;

		default:
			return luaL_error (L, "SSL_write: %s", ERR_error_string (error, NULL));
	}

	return 0;
}
/* }}} */

/* {{{ rssl_session_rawshutdown() */
static int rssl_session_rawshutdown (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ret = SSL_shutdown (session);
	int error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			lua_pushliteral (L, "");
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushnil (L);
			lua_pushliteral (L, "read");
			return 2;

		case SSL_ERROR_WANT_WRITE:
			lua_pushnil (L);
			lua_pushliteral (L, "write");
			return 2;

		default:
			return luaL_error (L, "SSL_shutdown: %s", ERR_error_string (error, NULL));
	}

	return 0;
}
/* }}} */

/* {{{ rssl_session_rawhandshake_connect() */
static int rssl_session_rawhandshake_connect (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ret = SSL_connect (session);
	int error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			lua_pushboolean (L, 1);
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushnil (L);
			lua_pushliteral (L, "read");
			return 2;

		case SSL_ERROR_WANT_WRITE:
			lua_pushnil (L);
			lua_pushliteral (L, "write");
			return 2;

		default:
			return luaL_error (L, "SSL_connect: %s", ERR_error_string (error, NULL));
	}

	return 0;
}
/* }}} */

/* {{{ rssl_session_rawhandshake_accept() */
static int rssl_session_rawhandshake_accept (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ret = SSL_accept (session);
	int error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			lua_pushboolean (L, 1);
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushnil (L);
			lua_pushliteral (L, "read");
			return 2;

		case SSL_ERROR_WANT_WRITE:
			lua_pushnil (L);
			lua_pushliteral (L, "write");
			return 2;

		default:
			return luaL_error (L, "SSL_accept: %s", ERR_error_string (error, NULL));
	}

	return 0;
}
/* }}} */

/* ---- Socket Functions ---------------------------------------------------- */

/* {{{ rsock_get_encryption() */
int rsock_get_encryption (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_socket_meta");

	lua_getfenv (L, 1);
	lua_getfield (L, -1, "ssl");

	return 1;
}
/* }}} */

/* {{{ rsock_encrypt() */
int rsock_encrypt (lua_State *L)
{
	int fd = *(int *) luaL_checkudata (L, 1, "ratchet_socket_meta");
	luaL_checkudata (L, 2, "ratchet_ssl_ctx_meta");

	BIO *bio = BIO_new_socket (fd, BIO_NOCLOSE);
	if (!bio)
		return luaL_error (L, "Could not create BIO object from: %d", fd);

	lua_getfield (L, 2, "create_session");
	lua_pushvalue (L, 2);
	lua_pushvalue (L, 1);
	lua_pushlightuserdata (L, bio);
	lua_call (L, 3, 1);
	SSL *session = *(SSL **) lua_topointer (L, -1);

	lua_getfenv (L, 1);
	lua_pushvalue (L, -2);
	lua_setfield (L, -2, "ssl");
	lua_pop (L, 1);

	/* Check if need to initiate TLS/SSL handshake. */
	lua_getfield (L, 1, "SO_ACCEPTCONN");
	if (lua_toboolean (L, -1))
		SSL_set_accept_state (session);
	else
		SSL_set_connect_state (session);
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* ---- Lua-implemented Functions ------------------------------------------- */

/* {{{ read() */
#define rssl_session_read "return function (self, ...)\n" \
	"	repeat\n" \
	"		local ret, yield_on = self:rawread(...)\n" \
	"		if not yield_on then\n" \
	"			return ret\n" \
	"		elseif yield_on == 'shutdown' then\n" \
	"			return self:shutdown()\n" \
	"		end\n" \
	"	until not coroutine.yield(yield_on, self:get_engine())\n" \
	"end\n"
/* }}} */

/* {{{ write() */
#define rssl_session_write "return function (self, ...)\n" \
	"	repeat\n" \
	"		local ret, yield_on = self:rawwrite(...)\n" \
	"		if not yield_on then\n" \
	"			return ret\n" \
	"		end\n" \
	"	until not coroutine.yield(yield_on, self:get_engine())\n" \
	"end\n"
/* }}} */

/* {{{ handshake_connect() */
#define rssl_session_handshake_connect "return function (self, ...)\n" \
	"	repeat\n" \
	"		local ret, yield_on = self:rawhandshake_connect(...)\n" \
	"		if not yield_on then\n" \
	"			return ret\n" \
	"		end\n" \
	"	until not coroutine.yield(yield_on, self:get_engine())\n" \
	"end\n"
/* }}} */

/* {{{ handshake_accept() */
#define rssl_session_handshake_accept "return function (self, ...)\n" \
	"	repeat\n" \
	"		local ret, yield_on = self:rawhandshake_accept(...)\n" \
	"		if not yield_on then\n" \
	"			return ret\n" \
	"		end\n" \
	"	until not coroutine.yield(yield_on, self:get_engine())\n" \
	"end\n"
/* }}} */

/* {{{ shutdown() */
#define rssl_session_shutdown "return function (self, ...)\n" \
	"	repeat\n" \
	"		local ret, yield_on = self:rawshutdown(...)\n" \
	"		if not yield_on then\n" \
	"			return ret\n" \
	"		end\n" \
	"	until not coroutine.yield(yield_on, self:get_engine())\n" \
	"end\n"
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_ssl() */
int luaopen_ratchet_ssl (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{"new", rssl_new},
		{NULL}
	};

	static const luaL_Reg ctxmeths[] = {
		/* Documented methods. */
		{"create_session", rssl_ctx_create_session},
		/* Undocumented, helper methods. */
		{NULL}
	};

	static const luaL_Reg ctxmetameths[] = {
		{"__gc", rssl_ctx_gc},
		{NULL}
	};

	static const luaL_Reg sslmeths[] = {
		/* Documented methods. */
		{"get_engine", rssl_session_get_engine},
		/* Undocumented, helper methods. */
		{"rawread", rssl_session_rawread},
		{"rawwrite", rssl_session_rawwrite},
		{"rawshutdown", rssl_session_rawshutdown},
		{"rawhandshake_connect", rssl_session_rawhandshake_connect},
		{"rawhandshake_accept", rssl_session_rawhandshake_accept},
		{NULL}
	};

	static const struct luafunc sslluameths[] = {
		/* Documented methods. */
		{"read", rssl_session_read},
		{"write", rssl_session_write},
		{"shutdown", rssl_session_shutdown},
		{"handshake_connect", rssl_session_handshake_connect},
		{"handshake_accept", rssl_session_handshake_accept},
		/* Undocumented, helper methods. */
		{NULL}
	};

	static const luaL_Reg sslmetameths[] = {
		{"__gc", rssl_session_gc},
		{NULL}
	};

	luaL_newmetatable (L, "ratchet_ssl_ctx_meta");
	lua_newtable (L);
	luaI_openlib (L, NULL, ctxmeths, 0);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, ctxmetameths, 0);
	lua_pop (L, 1);

	luaL_newmetatable (L, "ratchet_ssl_session_meta");
	lua_newtable (L);
	luaI_openlib (L, NULL, sslmeths, 0);
	register_luafuncs (L, -1, sslluameths);
	lua_setfield (L, -2, "__index");
	luaI_openlib (L, NULL, sslmetameths, 0);
	lua_pop (L, 1);

	luaI_openlib (L, "ratchet.ssl", funcs, 0);

	/* Global system initialization. */
	SSL_library_init ();
	SSL_load_error_strings ();

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
