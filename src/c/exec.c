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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "ratchet.h"
#include "misc.h"

typedef void (*signal_handler) (int);

#define CHECK_SIG_NAME(L, i, s) if (0 == strcmp (#s, lua_tostring (L, i))) { sig = s; }

/* {{{ struct rexec_state */
struct rexec_state
{
	pid_t pid;
	int infds[2];
	int outfds[2];
	int errfds[2];
};
/* }}} */

/* {{{ clear_state() */
static void clear_state (struct rexec_state *state)
{
	memset (state, 0, sizeof (struct rexec_state));
	state->infds[0] = -1;
	state->infds[1] = -1;
	state->outfds[0] = -1;
	state->outfds[1] = -1;
	state->errfds[0] = -1;
	state->errfds[1] = -1;
}
/* }}} */

/* {{{ alloc_argv_array() */
static char **alloc_argv_array (lua_State *L, int index)
{
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "argv");
	lua_remove (L, -2);

	int num, i;
	for (num=1; ; num++)
	{
		lua_rawgeti (L, -1, num);
		if (lua_isnil (L, -1))
			break;
		lua_pop (L, 1);
	}
	lua_pop (L, 1);

	const char *item;
	size_t item_len;
	char **ret = (char **) malloc (sizeof (char *) * num);
	memset (ret, 0, sizeof (char *) * num);
	for (i=1; i<num; i++)
	{
		lua_rawgeti (L, -1, i);
		item = lua_tolstring (L, -1, &item_len);
		ret[i-1] = (char *) malloc (item_len+1);
		memcpy (ret[i-1], item, item_len+1);
		lua_pop (L, 1);
	}

	return ret;
}
/* }}} */

/* {{{ free_argv_array() */
static void free_argv_array (char **array)
{
	char **iter = array;
	while (*iter)
		free (*iter++);
	free (array);
}
/* }}} */

/* {{{ start_process() */
static int start_process (lua_State *L, struct rexec_state *state, char *const *argv)
{
	if (-1 == pipe (state->infds))
		return ratchet_error_errno (L, "ratchet.exec.start()", "pipe");
	if (-1 == pipe (state->outfds))
		return ratchet_error_errno (L, "ratchet.exec.start()", "pipe");
	if (-1 == pipe (state->errfds))
		return ratchet_error_errno (L, "ratchet.exec.start()", "pipe");

	pid_t pid = fork ();

	if (pid == 0)
	{
		close (state->infds[1]);
		close (state->outfds[0]);
		close (state->errfds[0]);
		close (STDIN_FILENO);
		close (STDOUT_FILENO);
		close (STDERR_FILENO);
		if (-1 == dup2 (state->infds[0], STDIN_FILENO))
			_exit (1);
		if (-1 == dup2 (state->outfds[1], STDOUT_FILENO))
			_exit (1);
		if (-1 == dup2 (state->errfds[1], STDERR_FILENO))
			_exit (1);
		close (state->infds[0]);
		close (state->outfds[1]);
		close (state->errfds[1]);

		if (-1 == execvp (argv[0], argv))
			_exit (1);
	}

	else
	{
		close (state->infds[0]);
		close (state->outfds[1]);
		close (state->errfds[1]);
		set_nonblocking (state->infds[1]);
		set_nonblocking (state->outfds[0]);
		set_nonblocking (state->errfds[0]);

		state->pid = pid;
	}

	return 0;
}
/* }}} */

/* {{{ sigchld_handler() */
static void sigchld_handler (int sig)
{
	/* Intentially empty. */
}
/* }}} */

/* {{{ enable_sigchld() */
static int enable_sigchld (lua_State *L)
{
	/* This function's intention is to set a handler for SIGCHLD so
	 * that it is not ignored. POSIX states that ignoring SIGCHLD
	 * will result in child processes being immediately cleaned up
	 * and possibly resulting in ECHILD errors from waitpid().
	 *
	 * It attempts to leave existing handlers in place by checking
	 * against SIG_IGN and SIG_DFL.
	 */

#if HAVE_SIGACTION
	struct sigaction old;
	if (-1 == sigaction (SIGCHLD, NULL, &old))
		return ratchet_error_errno (L, "ratchet.exec", "sigaction");

	if (SIG_IGN != old.sa_handler && SIG_DFL != old.sa_handler)
		return 0;

	struct sigaction new;
	memset (&new, 0, sizeof (struct sigaction));
	new.sa_handler = sigchld_handler;
	sigemptyset (&new.sa_mask);
	if (-1 == sigaction (SIGCHLD, &new, NULL))
		return ratchet_error_errno (L, "ratchet.exec", "sigaction");

	return 0;
#else
	typedef void (*sighandler) (int);
	sighandler ret = signal (SIGCHLD, sigchld_handler);

	if (SIG_ERR == ret)
		return ratchet_error_errno (L, "ratchet.exec", "signal");
	else if (SIG_IGN != ret && SIG_DFL != ret)
	{
		if (SIG_ERR == signal (SIGCHLD, ret))
			return ratchet_error_errno (L, "ratchet.exec", "signal");
	}

	return 0;
#endif
}
/* }}} */

/* ---- Namespace Functions ------------------------------------------------- */

/* {{{ rexec_new() */
static int rexec_new (lua_State *L)
{
	luaL_checktype (L, 1, LUA_TTABLE);

	struct rexec_state *state = (struct rexec_state *) lua_newuserdata (L, sizeof (struct rexec_state));
	clear_state (state);

	luaL_getmetatable (L, "ratchet_exec_meta");
	lua_setmetatable (L, -2);

	lua_newtable (L);
	lua_pushvalue (L, 1);
	lua_setfield (L, -2, "argv");
	lua_setuservalue (L, -2);

	return 1;
}
/* }}} */

/* ---- Member Functions ---------------------------------------------------- */

/* {{{ rexec_clean_up() */
static int rexec_clean_up (lua_State *L)
{
	struct rexec_state *state = (struct rexec_state *) luaL_checkudata (L, 1, "ratchet_exec_meta");
	if (state->infds[1] >= 0) close (state->infds[1]);
	if (state->outfds[0] >= 0) close (state->outfds[0]);
	if (state->errfds[0] >= 0) close (state->errfds[0]);
	state->infds[1] = -1;
	state->outfds[0] = -1;
	state->errfds[0] = -1;
	return 0;
}
/* }}} */

/* {{{ rexec_get_argv() */
static int rexec_get_argv (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "argv");
	return 1;
}
/* }}} */

/* {{{ rexec_start() */
static int rexec_start (lua_State *L)
{
	struct rexec_state *state = (struct rexec_state *) luaL_checkudata (L, 1, "ratchet_exec_meta");

	time_t start_time = time (NULL);
	char **argv = alloc_argv_array (L, 1);
	start_process (L, state, argv);
	free_argv_array (argv);

	lua_getuservalue (L, 1);

	lua_pushnumber (L, (lua_Number) start_time);
	lua_setfield (L, -2, "start_time");

	int **infd = (int **) lua_newuserdata (L, sizeof (int *));
	luaL_getmetatable (L, "ratchet_exec_file_write_meta");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "stdin");

	int **outfd = (int **) lua_newuserdata (L, sizeof (int *));
	luaL_getmetatable (L, "ratchet_exec_file_read_meta");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "stdout");

	int **errfd = (int **) lua_newuserdata (L, sizeof (int *));
	luaL_getmetatable (L, "ratchet_exec_file_read_meta");
	lua_setmetatable (L, -2);
	lua_setfield (L, -2, "stderr");

	*infd = &state->infds[1];
	*outfd = &state->outfds[0];
	*errfd = &state->errfds[0];

	return 0;
}
/* }}} */

/* {{{ rexec_get_start_time() */
static int rexec_get_start_time (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");
	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "start_time");
	return 1;
}
/* }}} */

/* {{{ rexec_stdin() */
static int rexec_stdin (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "stdin");
	if (!luaL_testudata (L, -1, "ratchet_exec_file_write_meta"))
		return ratchet_error_str (L, "ratchet.exec.stdin()", "EBADF", "Must call start().");

	return 1;
}
/* }}} */

/* {{{ rexec_stdout() */
static int rexec_stdout (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "stdout");
	if (!luaL_testudata (L, -1, "ratchet_exec_file_read_meta"))
		return ratchet_error_str (L, "ratchet.exec.stdout()", "EBADF", "Must call start().");

	return 1;
}
/* }}} */

/* {{{ rexec_stderr() */
static int rexec_stderr (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");

	lua_getuservalue (L, 1);
	lua_getfield (L, -1, "stderr");
	if (!luaL_testudata (L, -1, "ratchet_exec_file_read_meta"))
		return ratchet_error_str (L, "ratchet.exec.stderr()", "EBADF", "Must call start().");

	return 1;
}
/* }}} */

/* {{{ rexec_wait() */
static int rexec_wait (lua_State *L)
{
	struct rexec_state *state = (struct rexec_state *) luaL_checkudata (L, 1, "ratchet_exec_meta");
	if (!lua_isnoneornil (L, 2) && !lua_isnumber (L, 2))
		return luaL_argerror (L, 2, "Optional timeout number or nil expected.");

	int ctx = 0;
	lua_getctx (L, &ctx);
	if (ctx == 1 && !lua_toboolean (L, 3))
		return ratchet_error_str (L, "ratchet.exec.wait()", "ETIMEDOUT", "Timed out on wait.");
	lua_settop (L, 2);

	int status = 0;
	int ret = waitpid (state->pid, &status, WNOHANG);
	if (ret == -1)
		return ratchet_error_errno (L, "ratchet.exec.wait()", "waitpid");
	else if (0 == ret)
	{
		lua_pushlightuserdata (L, RATCHET_YIELD_SIGNAL);
		lua_pushinteger (L, SIGCHLD);
		lua_pushvalue (L, 2);
		return lua_yieldk (L, 3, 1, rexec_wait);
	}

	rexec_clean_up (L);

	lua_pushinteger (L, (lua_Integer) WEXITSTATUS (status));
	lua_pushboolean (L, WIFEXITED (status));
	return 2;
}
/* }}} */

/* {{{ rexec_communicate() */
static int rexec_communicate (lua_State *L)
{
	(void) luaL_checkudata (L, 1, "ratchet_exec_meta");
	int ctx = 0;

	lua_getctx (L, &ctx);

	const char *data = NULL;
	if (ctx <= 2)
		data = luaL_optstring (L, 2, NULL);

top:

	if (ctx == 0)
	{
		lua_settop (L, 2);
		lua_pushliteral (L, "");
		lua_pushliteral (L, "");

		lua_getfield (L, 1, "start");
		lua_pushvalue (L, 1);
		lua_callk (L, 1, 0, 1, rexec_communicate);
		ctx = 1;
		goto top;
	}

	else if (ctx == 1)
	{
		if (data)
		{
			lua_getuservalue (L, 1);
			lua_getfield (L, -1, "stdin");
			lua_remove (L, -2);
			lua_getfield (L, -1, "write");
			lua_insert (L, -2);
			lua_pushvalue (L, 2);
			lua_callk (L, 2, 1, 2, rexec_communicate);
			ctx = 2;
			goto top;
		}
		else
		{
			ctx = 2;
			goto top;
		}
	}

	else if (ctx == 2)
	{
		if (!lua_isnil (L, -1))
		{
			lua_replace (L, 2);
			data = lua_tostring (L, 2);
			ctx = 1;
			goto top;
		}
		lua_pop (L, 1);

		lua_getuservalue (L, 1);
		lua_getfield (L, -1, "stdin");
		lua_remove (L, -2);
		lua_getfield (L, -1, "close");
		lua_insert (L, -2);
		lua_callk (L, 1, 0, 3, rexec_communicate);

		lua_getuservalue (L, 1);
		lua_createtable (L, 2, 0);
		lua_getfield (L, -2, "stdout");
		lua_rawseti (L, -2, 1);
		lua_getfield (L, -2, "stderr");
		lua_rawseti (L, -2, 2);
		lua_replace (L, 2);
		lua_pop (L, 1);

		ctx = 3;
		goto top;
	}

	else if (ctx == 3)
	{
		lua_rawgeti (L, 2, 1);
		if (lua_isnil (L, -1))
		{
			lua_pop (L, 1);
			ctx = 9;
			goto top;
		}
		lua_pop (L, 1);

		lua_pushlightuserdata (L, RATCHET_YIELD_MULTIRW);
		lua_pushvalue (L, 2);
		lua_yieldk (L, 2, 4, rexec_communicate);
		return luaL_error (L, "unreachable");
	}

	else if (ctx == 4)
	{
		if (lua_isnumber (L, -1) && SIGPIPE == lua_tointeger (L, -1))
		{
			lua_pop (L, 1);
			ctx = 9;
			goto top;
		}

		lua_getuservalue (L, 1);
		lua_getfield (L, -1, "stdout");
		if (lua_compare (L, -1, -3, LUA_OPEQ))
		{
			lua_replace (L, -3);
			lua_pop (L, 1);
			ctx = 5;
			goto top;
		}
		lua_pop (L, 1);

		lua_getfield (L, -1, "stderr");
		if (lua_compare (L, -1, -3, LUA_OPEQ))
		{
			lua_replace (L, -3);
			lua_pop (L, 1);
			ctx = 7;
			goto top;
		}
		lua_pop (L, 3);

		ctx = 3;
		goto top;
	}

	else if (ctx == 5)
	{
		lua_getfield (L, -1, "read");
		lua_insert (L, -2);
		lua_callk (L, 1, 1, 6, rexec_communicate);
		ctx = 6;
		goto top;
	}

	else if (ctx == 6)
	{
		if (0 == strcmp ("", lua_tostring (L, -1)))
		{
			lua_pop (L, 1);
			lua_pushvalue (L, 2);
			lua_rawgeti (L, -1, 2);
			lua_rawseti (L, -2, 1);
			lua_pushnil (L);
			lua_rawseti (L, -2, 2);
			lua_pop (L, 1);
			ctx = 3;
			goto top;
		}
		else
		{
			lua_pushvalue (L, 3);
			lua_insert (L, -2);
			lua_concat (L, 2);
			lua_replace (L, 3);
			ctx = 3;
			goto top;
		}
	}

	else if (ctx == 7)
	{
		lua_getfield (L, -1, "read");
		lua_insert (L, -2);
		lua_callk (L, 1, 1, 8, rexec_communicate);
		ctx = 8;
		goto top;
	}

	else if (ctx == 8)
	{
		if (0 == strcmp ("", lua_tostring (L, -1)))
		{
			lua_pop (L, 1);
			lua_pushvalue (L, 2);
			lua_rawgeti (L, -1, 2);
			if (lua_isnil (L, -1))
				lua_rawseti (L, -2, 1);
			else
			{
				lua_pop (L, 1);
				lua_pushnil (L);
				lua_rawseti (L, -2, 2);
			}
			lua_pop (L, 1);
			ctx = 3;
			goto top;
		}
		else
		{
			lua_pushvalue (L, 4);
			lua_insert (L, -2);
			lua_concat (L, 2);
			lua_replace (L, 4);
			ctx = 3;
			goto top;
		}
	}

	else if (ctx == 9)
	{
		lua_getfield (L, 1, "wait");
		lua_pushvalue (L, 1);
		lua_callk (L, 1, 1, 10, rexec_communicate);
		ctx = 10;
		goto top;
	}

	else if (ctx == 10)
		return 3;

	return luaL_error (L, "unreachable");
}
/* }}} */

/* {{{ rexec_kill() */
static int rexec_kill (lua_State *L)
{
	struct rexec_state *state = (struct rexec_state *) luaL_checkudata (L, 1, "ratchet_exec_meta");
	lua_settop (L, 2);

	int sig = get_signal (L, 2, SIGTERM);
	if (-1 == sig)
		return luaL_argerror (L, 2, "Invalid signal.");

	if (-1 == kill (state->pid, sig))
		return ratchet_error_errno (L, "ratchet.exec.kill()", "kill");

	return 0;
}
/* }}} */

/* {{{ rexec_file_get_fd() */
static int rexec_file_get_fd (lua_State *L)
{
	int fd = (** (int **) lua_topointer (L, 1));
	lua_pushinteger (L, fd);
	return 1;
}
/* }}} */

/* {{{ rexec_file_close() */
static int rexec_file_close (lua_State *L)
{
	int *fd = (* (int **) lua_topointer (L, 1));
	if (*fd >= 0)
		close (*fd);
	*fd = -1;
	return 0;
}
/* }}} */

/* {{{ rexec_file_read() */
static int rexec_file_read (lua_State *L)
{
	int fd = (** (int **) luaL_checkudata (L, 1, "ratchet_exec_file_read_meta"));
	luaL_Buffer buffer;
	ssize_t ret;

	lua_settop (L, 2);

	luaL_buffinit (L, &buffer);
	char *prepped = luaL_prepbuffer (&buffer);

	size_t len = (size_t) luaL_optunsigned (L, 2, (lua_Unsigned) LUAL_BUFFERSIZE);
	if (len > LUAL_BUFFERSIZE)
		return luaL_error (L, "Cannot read more than %u bytes, %u requested", (unsigned) LUAL_BUFFERSIZE, (unsigned) len);

	ret = read (fd, prepped, len);
	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_READ);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rexec_file_read);
		}
		else
			return ratchet_error_errno (L, "ratchet.exec.file.read()", "read");
	}

	luaL_addsize (&buffer, (size_t) ret);
	luaL_pushresult (&buffer);

	return 1;
}
/* }}} */

/* {{{ rexec_file_write() */
static int rexec_file_write (lua_State *L)
{
	int fd = (** (int **) luaL_checkudata (L, 1, "ratchet_exec_file_write_meta"));
	size_t data_len, remaining;
	const char *data = luaL_checklstring (L, 2, &data_len);
	ssize_t ret;

	lua_settop (L, 2);

	signal_handler old = signal (SIGPIPE, SIG_IGN);
	ret = write (fd, data, data_len);
	int orig_errno = errno;
	signal (SIGPIPE, old);

	if (ret == -1)
	{
		errno = orig_errno;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			lua_pushlightuserdata (L, RATCHET_YIELD_WRITE);
			lua_pushvalue (L, 1);
			return lua_yieldk (L, 2, 1, rexec_file_write);
		}
		else
			return ratchet_error_errno (L, "ratchet.exec.file.write()", "write");
	}

	if ((size_t) ret < data_len)
	{
		remaining = data_len - (size_t) ret;
		lua_pushlstring (L, data+ret, remaining);
		return 1;
	}
	else
		return 0;
}
/* }}} */

/* ---- Public Functions ---------------------------------------------------- */

/* {{{ luaopen_ratchet_exec() */
int luaopen_ratchet_exec (lua_State *L)
{
	/* Static functions in the ratchet.exec namespace. */
	static const luaL_Reg funcs[] = {
		/* Documented methods. */
		{"new", rexec_new},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Meta-methods for ratchet.exec object metatables. */
	static const luaL_Reg metameths[] = {
		{"__gc", rexec_clean_up},
		{NULL}
	};

	/* Methods in the ratchet.exec class. */
	static const luaL_Reg meths[] = {
		/* Documented methods. */
		{"get_argv", rexec_get_argv},
		{"communicate", rexec_communicate},
		{"start", rexec_start},
		{"get_start_time", rexec_get_start_time},
		{"stdin", rexec_stdin},
		{"stdout", rexec_stdout},
		{"stderr", rexec_stderr},
		{"wait", rexec_wait},
		{"kill", rexec_kill},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Methods in readable file streams. */
	static const luaL_Reg readfilemeths[] = {
		/* Documented methods. */
		{"get_fd", rexec_file_get_fd},
		{"read", rexec_file_read},
		{"close", rexec_file_close},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Methods in writable file streams. */
	static const luaL_Reg writefilemeths[] = {
		/* Documented methods. */
		{"get_fd", rexec_file_get_fd},
		{"write", rexec_file_write},
		{"close", rexec_file_close},
		/* Undocumented, helper methods. */
		{NULL}
	};

	/* Set up file stream metatables. */
	luaL_newmetatable (L, "ratchet_exec_file_read_meta");
	luaL_newlib (L, readfilemeths);
	lua_setfield (L, -2, "__index");
	luaL_newmetatable (L, "ratchet_exec_file_write_meta");
	luaL_newlib (L, writefilemeths);
	lua_setfield (L, -2, "__index");
	lua_pop (L, 2);

	/* Set up the ratchet.exec namespace functions. */
	luaL_newlib (L, funcs);
	lua_pushvalue (L, -1);
	lua_setfield (L, LUA_REGISTRYINDEX, "ratchet_exec_class");

	/* Set up the ratchet.exec class and metatables. */
	luaL_newmetatable (L, "ratchet_exec_meta");
	luaL_setfuncs (L, metameths, 0);
	luaL_newlib (L, meths);
	lua_setfield (L, -2, "__index");
	lua_pop (L, 1);

	/* Ensure that the SIGCHLD signal is not being ignored. */
	enable_sigchld (L);

	return 1;
}
/* }}} */

// vim:foldmethod=marker:ai:ts=4:sw=4:
