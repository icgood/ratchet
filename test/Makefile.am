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
	test_shutdown.lua \
	test_unix_sockets.lua \
	test_event_timeout.lua \
	test_ssl_listen_connect.lua \
	test_ssl_send_recv.lua \
	test_zmq_send_recv.lua \
	test_multi_protocol.lua \
	test_pause_unpause.lua \
	test_callable_object.lua \
	test_sockopt.lua
XFAIL_TESTS = 

if HAVE_OPENSSL
XFAIL_TESTS +=
else
XFAIL_TESTS += test_ssl_listen_connect.lua \
	       test_ssl_send_recv.lua
endif

if HAVE_SOCKET
XFAIL_TESTS +=  
else
XFAIL_TESTS += test_listen_connect.lua \
	       test_send_recv.lua \
	       test_shutdown.lua \
	       test_unix_sockets.lua \
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

endif
