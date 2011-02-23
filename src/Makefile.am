allsources = ratchet.h ratchet.c \
	     misc.h misc.c

if HAVE_SOCKET
allsources += sockopt.h sockopt.c socket.c
endif

if HAVE_ZMQ
allsources += zmq.c
endif

if HAVE_UDNS
allsources += dns.c
endif

if HAVE_TIMERFD
allsources += timerfd.c
endif

if ENABLE_DEVEL
include_HEADERS = ratchet.h

lib_LTLIBRARIES = libratchet.la
libratchet_la_SOURCES = $(allsources)
endif

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_SOURCES = $(allsources)
ratchet_la_LDFLAGS = -module -avoid-version
endif
