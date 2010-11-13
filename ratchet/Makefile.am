if ENABLE_DEVEL
include_HEADERS = ratchet.h
pkginclude_HEADERS = context.h
endif

lib_LTLIBRARIES = libratchet.la
libratchet_la_CFLAGS = -I$(top_srcdir)/misc \
		       -I$(top_srcdir)/makeclass \
		       -I$(top_srcdir)/dns \
		       -I$(top_srcdir)/socket
libratchet_la_LIBADD = ../misc/libratchetmisc.la \
		       ../makeclass/libratchetmakeclass.la \
		       ../dns/libratchetdns.la \
		       ../socket/libratchetsocket.la
libratchet_la_SOURCES = context.c context.h \
			ratchet.c

# Add libraries that have dependencies.
if HAVE_ZMQ
libratchet_la_CFLAGS += -I$(top_dir)/zmq
libratchet_la_LIBADD += ../zmq/libratchetzmq.la
endif
if HAVE_EPOLL
libratchet_la_CFLAGS += -I$(top_dir)/epoll
libratchet_la_LIBADD += ../epoll/libratchetepoll.la
endif
if HAVE_LIBEVENT
libratchet_la_CFLAGS += -I$(top_dir)/libevent
libratchet_la_LIBADD += ../libevent/libratchetlibevent.la
endif
if HAVE_TIMERFD
libratchet_la_CFLAGS += -I$(top_dir)/timer
libratchet_la_LIBADD += ../timer/libratchettimer.la
endif

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_CFLAGS = $(libratchet_la_CFLAGS)
ratchet_la_LIBADD = libratchet.la
ratchet_la_LDFLAGS = -module -avoid-version
endif
