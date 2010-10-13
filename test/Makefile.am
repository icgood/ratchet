TESTS = test_submodules.lua \
	test_makeclass.lua \
	test_epoll.lua \
	test_zmq.lua \
	test_ratchet.lua \
	test_ratchet_zmq.lua \
	test_ratchet_both.lua \
	test_parseuri.lua

EXTRA_DIST = $(TESTS)
check_DATA = ratchet.so

ratchet.so:
	ln -sf ../ratchet/.libs/ratchet.so .

CLEANFILES = ratchet.so

