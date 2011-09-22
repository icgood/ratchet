if HAVE_LUA_INTERPRETER

if ENABLE_MODULE
TESTS_ENVIRONMENT = LUA_CPATH="../src/.libs/?.so;;" @LUA@
else
TESTS_ENVIRONMENT = @LUA@ -e 'package.preload["ratchet"] = package.loadlib("../src/.libs/libratchet.so", "luaopen_ratchet")'
endif

TESTS = test_listen_connect.lua \
	test_send_recv.lua \
	test_error_handler.lua \
	test_wait_all.lua \
	test_thread_error_handler.lua \
	test_thread_global_scope.lua \
	test_shutdown.lua \
	test_socketpair.lua \
	test_socketpad.lua \
	test_unix_sockets.lua \
	test_event_timeout.lua \
	test_ssl_send_recv.lua \
	test_zmq_send_recv.lua \
	test_multi_protocol.lua \
	test_pause_unpause.lua \
	test_callable_object.lua \
	test_http_get.lua \
	test_smtp.lua \
	test_sockopt.lua
XFAIL_TESTS = 
EXTRA_DIST = $(TESTS)

check_DATA = 
CLEANFILES = 

if HAVE_OPENSSL
check_DATA += cert.pem
CLEANFILES += cert.pem
cert.pem:
	openssl req -x509 -nodes -subj '/CN=localhost' -newkey rsa:1024 -keyout $@ -out $@ > /dev/null
else
XFAIL_TESTS += test_ssl_send_recv.lua
endif

if HAVE_SOCKET
TESTS_ENVIRONMENT += -e 'package.preload["ratchet.socketpad"] = assert(loadfile("../src/socketpad/init.lua"))'
XFAIL_TESTS +=  
else
XFAIL_TESTS += test_listen_connect.lua \
	       test_send_recv.lua \
	       test_shutdown.lua \
	       test_socketpair.lua \
	       test_socketpad.lua \
	       test_unix_sockets.lua \
	       test_ssl_send_recv.lua \
	       test_event_timeout.lua \
	       test_multi_protocol.lua \
	       test_sockopt.lua
endif

if HAVE_DNS
XFAIL_TESTS +=  
else
XFAIL_TESTS += test_listen_connect.lua \
	       test_send_recv.lua \
	       test_shutdown.lua \
	       test_ssl_send_recv.lua \
	       test_event_timeout.lua \
	       test_multi_protocol.lua \
	       test_sockopt.lua
endif

if HAVE_ZMQ
XFAIL_TESTS += 
else
XFAIL_TESTS += test_zmq_send_recv.lua \
	       test_multi_protocol.lua
endif

if ENABLE_HTTP
TESTS_ENVIRONMENT += -e 'package.preload["ratchet.http.client"] = assert(loadfile("../src/http/client.lua"))' \
                     -e 'package.preload["ratchet.http.server"] = assert(loadfile("../src/http/server.lua"))' \
                     -e 'package.preload["ratchet.http.common"] = assert(loadfile("../src/http/common.lua"))'
XFAIL_TESTS +=
else
XFAIL_TESTS += test_http_get.lua
endif

if ENABLE_SMTP
TESTS_ENVIRONMENT += -e 'package.preload["ratchet.smtp.client"] = assert(loadfile("../src/smtp/client.lua"))' \
                     -e 'package.preload["ratchet.smtp.server"] = assert(loadfile("../src/smtp/server.lua"))' \
                     -e 'package.preload["ratchet.smtp.common"] = assert(loadfile("../src/smtp/common.lua"))'
XFAIL_TESTS +=
else
XFAIL_TESTS += test_smtp.lua
endif

endif
