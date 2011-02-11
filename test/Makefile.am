TESTS = test_listen_connect.lua \
	test_send_recv.lua \
	test_zmq_send_recv.lua \
	test_multi_protocol.lua \
	test_error_handler.lua \
	test_shutdown.lua \
	test_pause_unpause.lua \
	test_callable_object.lua \
	test_sockopt.lua \
	test_uri.lua

EXTRA_DIST = $(TESTS)
check_DATA = ratchet.so

ratchet.so:
	ln -sf ../src/.libs/ratchet.so .

CLEANFILES = ratchet.so

