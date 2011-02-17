allsources = ratchet.h ratchet.c \
	     misc.h misc.c \
	     sockopt.h sockopt.c \
	     socket.c \
	     uri.c \
	     timerfd.c

if HAVE_ZMQ
allsources += zmq.c
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
