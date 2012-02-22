if HAVE_LUA_INTERPRETER

if ENABLE_MODULE
TESTS_ENVIRONMENT = LUA_CPATH="../src/c/.libs/?.so;;" LUA_PATH="./?.lua;./?/init.lua;;" @LUA@
else
TESTS_ENVIRONMENT = LUA_PATH="./?.lua;./?/init.lua;;" @LUA@ -e \
		    'package.preload["ratchet"] = package.loadlib("../src/c/.libs/libratchet.so", "luaopen_ratchet")'
endif

TESTS = test_exec.lua \
	test_listen_connect.lua \
	test_send_recv.lua \
	test_pcall_errors.lua \
	test_wait_all.lua \
	test_thread_kill.lua \
	test_thread_space.lua \
	test_shutdown.lua \
	test_socketpair.lua \
	test_socketpad.lua \
	test_socket_byteorder.lua \
	test_socket_multi_recv.lua \
	test_message_bus_sockets.lua \
	test_message_bus_local.lua \
	test_unix_sockets.lua \
	test_event_timeout.lua \
	test_ssl_send_recv.lua \
	test_zmq_send_recv.lua \
	test_multi_protocol.lua \
	test_pause_unpause.lua \
	test_callable_object.lua \
	test_http_get.lua \
	test_smtp.lua \
	test_smtp_bigmessage.lua \
	test_sockopt.lua
XFAIL_TESTS = 
EXTRA_DIST = $(TESTS)

ratchet-link:
	ln -nsf ../src/lua ./ratchet

check_DATA = ratchet-link
CLEANFILES = ratchet

if HAVE_OPENSSL
check_DATA += cert.pem
CLEANFILES += cert.pem
cert.pem:
	openssl req -x509 -nodes -subj '/CN=localhost' -newkey rsa:1024 -keyout $@ -out $@ > /dev/null
else
XFAIL_TESTS += test_ssl_send_recv.lua
endif

if !HAVE_SOCKET
XFAIL_TESTS += test_listen_connect.lua \
	       test_send_recv.lua \
	       test_shutdown.lua \
	       test_socketpair.lua \
	       test_socket_byteorder.lua \
	       test_socket_multi_read.lua \
	       test_unix_sockets.lua \
	       test_ssl_send_recv.lua \
	       test_event_timeout.lua \
	       test_multi_protocol.lua \
	       test_sockopt.lua
endif

if !ENABLE_SOCKETPAD
XFAIL_TESTS += test_socketpad.lua
endif

if !HAVE_DNS
XFAIL_TESTS += test_listen_connect.lua \
	       test_send_recv.lua \
	       test_shutdown.lua \
	       test_ssl_send_recv.lua \
	       test_event_timeout.lua \
	       test_multi_protocol.lua \
	       test_sockopt.lua
endif

if !HAVE_ZMQ
XFAIL_TESTS += test_zmq_send_recv.lua \
	       test_multi_protocol.lua
endif

if !ENABLE_HTTP
XFAIL_TESTS += test_http_get.lua
endif

if !ENABLE_BUS
XFAIL_TESTS += test_message_bus_sockets.lua \
	       test_message_bus_local.lua
endif

if !ENABLE_SMTP
XFAIL_TESTS += test_smtp.lua \
	       test_smtp_bigmessage.lua
endif

endif
