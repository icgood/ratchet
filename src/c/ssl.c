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
#include <signal.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "ratchet.h"
#include "misc.h"

/* {{{ handle_ssl_error() */
static int handle_ssl_error (lua_State *L, const char *func, int ret, unsigned long error, int orig_errno)
{
	unsigned long error_queue = ERR_get_error ();
	if (error_queue)
	{
		const char *reason = ERR_reason_error_string (error);
		ERR_clear_error ();
		if (reason)
			return ratchet_error_str (L, func, "SSLERROR", "SSL error: %s", reason);
	}

	if (error == SSL_ERROR_SYSCALL)
	{
		if (ret == 0)
			return ratchet_error_str (L, func, "SSLEOF", "Unexpected EOF received");

		else if (ret == -1 && orig_errno)
		{
			errno = orig_errno;
			return ratchet_error_errno (L, func, "<unknown>");
		}
	}

	return ratchet_error_str (L, func, "SSLERROR", "Unknown SSL error");
}
/* }}} */

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

/* {{{ setup_ssl_methods() */
#define setup_ssl_method_field(n) lua_pushlightuserdata (L, n ## _ ## method); lua_setfield (L, -2, #n) 
void setup_ssl_methods (lua_State *L)
{
	setup_ssl_method_field (SSLv2);
	setup_ssl_method_field (SSLv2_server);
	setup_ssl_method_field (SSLv2_client);
	setup_ssl_method_field (SSLv3);
	setup_ssl_method_field (SSLv3_server);
	setup_ssl_method_field (SSLv3_client);
	setup_ssl_method_field (TLSv1);
	setup_ssl_method_field (TLSv1_server);
	setup_ssl_method_field (TLSv1_client);
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rssl_ctx_new() */
static int rssl_ctx_new (lua_State *L)
{
	typedef SSL_METHOD *(*ssl_meth) (void);
	ssl_meth meth = (ssl_meth) SSLv3_method;
	SSL_CTX *ctx;

	/* Check for override of default method. */
	if (!lua_isnoneornil (L, 1))
	{
		luaL_checktype (L, 1, LUA_TLIGHTUSERDATA);
		meth = (ssl_meth) lua_topointer (L, 1);
	}

	/* Create context. */
	ctx = SSL_CTX_new (meth ());
	if (!ctx)
		return luaL_error (L, "Creation of SSL_CTX object failed");

	/* Set up Lua object. */
	SSL_CTX **new = (SSL_CTX **) lua_newuserdata (L, sizeof (SSL_CTX *));
	*new = ctx;

	luaL_getmetatable (L, "ratchet_ssl_ctx_meta");
	lua_setmetatable (L, -2);

	lua_createtable (L, 0, 1);
	lua_pushvalue (L, 2);
	lua_setfield (L, -2, "password");
	lua_setuservalue (L, -2);

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
	lua_setuservalue (L, -2);

	return 1;
}
/* }}} */

/* {{{ rssl_ctx_set_verify_mode() */
static int rssl_ctx_set_verify_mode (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	static const char *lst[] = {
		"none",
		"peer",
		"fail",
		"once",
		NULL
	};
	static const int modelst[] = {
		SSL_VERIFY_NONE,
		SSL_VERIFY_PEER,
		SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
		SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE
	};
	int mode = modelst[luaL_checkoption (L, 2, "peer", lst)];

	SSL_CTX_set_verify (ctx, mode, NULL);

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_load_certs() */
static int rssl_ctx_load_certs (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	const char *certchainfile = luaL_checkstring (L, 2);
	const char *privkeyfile = luaL_optstring (L, 3, certchainfile);
	const char *password = luaL_optstring (L, 4, NULL);

	/* Load keys and certificates. */
	if (!SSL_CTX_use_certificate_chain_file (ctx, certchainfile))
		return luaL_error (L, "Could not read certificate chain file: %s", certchainfile);
	if (password)
	{
		SSL_CTX_set_default_passwd_cb (ctx, password_cb);
		SSL_CTX_set_default_passwd_cb_userdata (ctx, (void *) password);
	}
	if (!SSL_CTX_use_PrivateKey_file (ctx, privkeyfile, SSL_FILETYPE_PEM))
		return luaL_error (L, "Could not read key file: %s", privkeyfile);

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_load_cas() */
static int rssl_ctx_load_cas (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	const char *ca_path = luaL_optstring (L, 2, NULL);
	const char *ca_file = luaL_optstring (L, 3, NULL);
	if (!ca_file && !ca_path)
		return luaL_argerror (L, 2, "Path to trusted CAs required.");
	int depth = luaL_optint (L, 4, 1);

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

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_load_randomness() */
static int rssl_ctx_load_randomness (lua_State *L)
{
	luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	const char *random = luaL_optstring (L, 2, NULL);
	int rand_max_bytes = luaL_optint (L, 3, 1048576);

	/* Load randomness. */
	char buffer[LUAL_BUFFERSIZE];
	if (!random)
		random = RAND_file_name (buffer, LUAL_BUFFERSIZE);
	if (!random)
		return luaL_argerror (L, 2, "Could not find default random file");
	if (!RAND_load_file (random, rand_max_bytes))
		return luaL_error (L, "Could not load randomness: %s", random);

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_load_dh_params() */
static int rssl_ctx_load_dh_params (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	const char *file = luaL_checkstring (L, 2);

	BIO *bio = BIO_new_file (file, "r");
	if (!bio)
		return luaL_error (L, "Could not open DH params file: %s", file);

	DH *ret = PEM_read_bio_DHparams (bio, NULL, NULL, NULL);
	BIO_free (bio);
	if (!ret)
		return luaL_error (L, "Could not read DH params file: %s", file);

	if (SSL_CTX_set_tmp_dh (ctx, ret) < 0)
		return luaL_error (L, "Could not set DH params: %s", file);

	return 0;
}
/* }}} */

/* {{{ rssl_ctx_generate_tmp_rsa() */
static int rssl_ctx_generate_tmp_rsa (lua_State *L)
{
	SSL_CTX *ctx = *(SSL_CTX **) luaL_checkudata (L, 1, "ratchet_ssl_ctx_meta");
	int bits = luaL_optint (L, 2, 512);
	unsigned long e = (unsigned long) luaL_optlong (L, 3, RSA_F4);

	RSA *rsa = RSA_generate_key (bits, e, NULL, NULL);
	if (!rsa)
		return luaL_error (L, "Could not generate a %d-bit RSA key", bits);

	if (!SSL_CTX_set_tmp_rsa (ctx, rsa))
		return luaL_error (L, "Could not set set temporary RSA key");

	RSA_free (rsa);

	return 0;
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

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "engine");

	return 1;
}
/* }}} */

/* {{{ rssl_session_verify_certificate() */
static int rssl_session_verify_certificate (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");
	size_t host_len;
	const char *host = luaL_optlstring (L, 2, NULL, &host_len);

	/* Check if we even got a certificate form peer. */
	X509 *peer = SSL_get_peer_certificate (session);
	if (!peer)
	{
		lua_pushboolean (L, 0);
		return 1;
	}
	else
		lua_pushboolean (L, 1);

	/* Verify certificate against authorities. */
	if (SSL_get_verify_result (session) != X509_V_OK)
	{
		X509_free (peer);
		lua_pushboolean (L, 0);
		return 2;
	}
	else
		lua_pushboolean (L, 1);

	/* Check the "Common Name" fields from the peer certificate. */
	if (host)
	{
		X509_NAME *peername = X509_get_subject_name (peer);

		int i = -1, found = 0;
		while (1)
		{
			i = X509_NAME_get_index_by_NID (peername, NID_commonName, i);
			if (i == -1)
				break;

			X509_NAME_ENTRY *entry = X509_NAME_get_entry (peername, i);
			ASN1_STRING *data = X509_NAME_ENTRY_get_data (entry);

			if (host_len != (size_t) ASN1_STRING_length (data))
			{
				X509_free (peer);
				lua_pushboolean (L, 0);
				return 3;
			}
			else if (0 == memcmp (host, ASN1_STRING_data (data), host_len))
				found = 1;
		}
		X509_free (peer);

		lua_pushboolean (L, found);
		return 3;
	}
	else
	{
		X509_free (peer);
		return 2;
	}
}
/* }}} */

/* {{{ rssl_session_get_rfc2253() */
static int rssl_session_get_rfc2253 (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	X509 *peer = SSL_get_peer_certificate (session);
	if (peer)
	{
		X509_NAME *peername = X509_get_subject_name (peer);

		BIO *mem = BIO_new (BIO_s_mem ());
		X509_NAME_print_ex (mem, peername, 0, XN_FLAG_RFC2253);

		luaL_Buffer buffer;
		luaL_buffinit (L, &buffer);
		char *prepped = luaL_prepbuffer (&buffer);
		int ret = BIO_read (mem, prepped, LUAL_BUFFERSIZE);
		luaL_addsize (&buffer, (size_t) ret);
		luaL_pushresult (&buffer);

		BIO_free (mem);

		X509_free (peer);
	}
	else
		lua_pushnil (L);

	return 1;
}
/* }}} */

/* {{{ rssl_session_get_cipher() */
static int rssl_session_get_cipher (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	lua_pushstring (L, SSL_get_cipher (session));

	return 1;
}
/* }}} */

/* {{{ rssl_session_shutdown() */
static int rssl_session_shutdown (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 2))
		return ratchet_error_str (L, "ratchet.ssl.session.shutdown()", "ETIMEDOUT", "Timed out on shutdown.");
	lua_settop (L, 1);

	void (*old) (int) = signal (SIGPIPE, SIG_IGN);
	int ret = SSL_shutdown (session);
	if (ret == 0)
		ret = SSL_shutdown (session);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	unsigned long error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			lua_pushboolean (L, 1);
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_shutdown);

		case SSL_ERROR_WANT_WRITE:
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_shutdown);

		case SSL_ERROR_SYSCALL:
			if (ret == 0 || (ret == -1 && (orig_errno == EPIPE || orig_errno == EBADF)))
			{
				lua_pushboolean (L, 0);
				return 1;
			}

		default:
			return handle_ssl_error (L, "ratchet.ssl.session.shutdown()", ret, error, orig_errno);
	}

	return luaL_error (L, "unreachable");
}
/* }}} */

/* {{{ rssl_session_read() */
static int rssl_session_read (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.ssl.session.read()", "ETIMEDOUT", "Timed out on read.");
	lua_settop (L, 2);

	luaL_Buffer buffer;
	luaL_buffinit (L, &buffer);
	char *prepped = luaL_prepbuffer (&buffer);

	size_t len = (size_t) luaL_optunsigned (L, 2, (lua_Unsigned) LUAL_BUFFERSIZE);
	if (len > LUAL_BUFFERSIZE)
		return luaL_error (L, "Cannot recv more than %u bytes, %u requested", (unsigned) LUAL_BUFFERSIZE, (unsigned) len);

	void (*old) (int) = signal (SIGPIPE, SIG_IGN);
	int ret = SSL_read (session, prepped, len);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	unsigned long error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			luaL_addsize (&buffer, (size_t) ret);
			luaL_pushresult (&buffer);
			return 1;

		case SSL_ERROR_ZERO_RETURN:
			lua_pushliteral (L, "");
			return 1;

		case SSL_ERROR_WANT_READ:
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_read);

		case SSL_ERROR_WANT_WRITE:
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_read);

		default:
			return handle_ssl_error (L, "ratchet.ssl.session.read()", ret, error, orig_errno);
	}

	return luaL_error (L, "unreachable");
}
/* }}} */

/* {{{ rssl_session_write() */
static int rssl_session_write (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");
	size_t size;
	const char *data = luaL_checklstring (L, 2, &size);

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.ssl.session.write()", "ETIMEDOUT", "Timed out on write.");
	lua_settop (L, 2);

	ERR_clear_error ();

	void (*old) (int) = signal (SIGPIPE, SIG_IGN);
	int ret = SSL_write (session, data, (int) size);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	unsigned long error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			return 0;

		case SSL_ERROR_WANT_READ:
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_write);

		case SSL_ERROR_WANT_WRITE:
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_write);

		default:
			return handle_ssl_error (L, "ratchet.ssl.session.write()", ret, error, orig_errno);
	}

	return luaL_error (L, "unreachable");
}
/* }}} */

/* {{{ rssl_session_connect() */
static int rssl_session_connect (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 2))
		return ratchet_error_str (L, "ratchet.ssl.session.client_handshake()", "ETIMEDOUT", "Timed out on client_handshake.");
	lua_settop (L, 1);

	void (*old) (int) = signal (SIGPIPE, SIG_IGN);
	int ret = SSL_connect (session);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	unsigned long error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			return 0;

		case SSL_ERROR_WANT_READ:
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_connect);

		case SSL_ERROR_WANT_WRITE:
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_connect);

		default:
			return handle_ssl_error (L, "ratchet.ssl.session.client_handshake()", ret, error, orig_errno);
	}

	return luaL_error (L, "unreachable");
}
/* }}} */

/* {{{ rssl_session_accept() */
static int rssl_session_accept (lua_State *L)
{
	SSL *session = *(SSL **) luaL_checkudata (L, 1, "ratchet_ssl_session_meta");

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 2))
		return ratchet_error_str (L, "ratchet.ssl.session.server_handshake()", "ETIMEDOUT", "Timed out on server_handshake.");
	lua_settop (L, 1);

	void (*old) (int) = signal (SIGPIPE, SIG_IGN);
	int ret = SSL_accept (session);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	unsigned long error = SSL_get_error (session, ret);
	switch (error)
	{
		case SSL_ERROR_NONE:
			return 0;

		case SSL_ERROR_WANT_READ:
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_accept);

		case SSL_ERROR_WANT_WRITE:
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "engine");
			lua_remove (L, -2);
			return lua_yieldk (L, 2, 1, rssl_session_accept);

		default:
			return handle_ssl_error (L, "ratchet.ssl.session.server_handshake()", ret, error, orig_errno);
	}

	return luaL_error (L, "unreachable");
}
/* }}} */

/* ---- Socket Functions ---------------------------------------------------- */

/* {{{ rsock_get_encryption() */
int rsock_get_encryption (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_socket_meta");

	lua_getuservalue (L, 1);
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

	lua_getuservalue (L, 1);
	lua_pushvalue (L, -2);
	lua_setfield (L, -2, "ssl");
	lua_pop (L, 1);

	return 1;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_ssl() */
int luaopen_ratchet_ssl (lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{"new", rssl_ctx_new},
		{NULL}
	};

	static const luaL_Reg ctxmeths[] = {
		/* Documented methods. */
		{"create_session", rssl_ctx_create_session},
		{"set_verify_mode", rssl_ctx_set_verify_mode},
		{"load_certs", rssl_ctx_load_certs},
		{"load_cas", rssl_ctx_load_cas},
		{"load_randomness", rssl_ctx_load_randomness},
		{"load_dh_params", rssl_ctx_load_dh_params},
		{"generate_tmp_rsa", rssl_ctx_generate_tmp_rsa},
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
		{"verify_certificate", rssl_session_verify_certificate},
		{"get_rfc2253", rssl_session_get_rfc2253},
		{"get_cipher", rssl_session_get_cipher},
		{"read", rssl_session_read},
		{"write", rssl_session_write},
		{"shutdown", rssl_session_shutdown},
		{"connect", rssl_session_connect},
		{"accept", rssl_session_accept},
		{"client_handshake", rssl_session_connect},
		{"server_handshake", rssl_session_accept},
		/* Undocumented, helper methods. */
		{NULL}
	};

	static const luaL_Reg sslmetameths[] = {
		{"__gc", rssl_session_gc},
		{NULL}
	};

	luaL_newmetatable (L, "ratchet_ssl_ctx_meta");
	lua_newtable (L);
	luaL_setfuncs (L, ctxmeths, 0);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, ctxmetameths, 0);
	lua_pop (L, 1);

	luaL_newmetatable (L, "ratchet_ssl_session_meta");
	lua_newtable (L);
	luaL_setfuncs (L, sslmeths, 0);
	lua_setfield (L, -2, "__index");
	luaL_setfuncs (L, sslmetameths, 0);
	lua_pop (L, 1);

	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_ssl_class");

	setup_ssl_methods (L);

	/* Global system initialization. */
	SSL_library_init ();
	SSL_load_error_strings ();

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
