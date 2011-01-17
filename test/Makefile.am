TESTS = test_uri.lua \
	test_connect.lua \
	test_attach_wait.lua

EXTRA_DIST = $(TESTS)
check_DATA = ratchet.so

ratchet.so:
	ln -sf ../src/.libs/ratchet.so .

CLEANFILES = ratchet.so

