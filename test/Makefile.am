TESTS = test_submodules.lua \
	test_makeclass.lua \
	test_parseuri.lua

if HAVE_ZMQ
TESTS += test_zmq.lua \
	 test_ratchet_zmq.lua
if HAVE_EPOLL
TESTS += test_ratchet_both.lua
endif
endif

if HAVE_EPOLL
TESTS += test_ratchet.lua \
	 test_epoll.lua
endif

EXTRA_DIST = $(TESTS)
check_DATA = ratchet.so

ratchet.so:
	ln -sf ../ratchet/.libs/ratchet.so .

CLEANFILES = ratchet.so

