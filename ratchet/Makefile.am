if ENABLE_DEVEL
include_HEADERS = ratchet.h
pkginclude_HEADERS = context.h
endif

lib_LTLIBRARIES = libratchet.la
libratchet_la_CFLAGS = -I$(top_srcdir)/misc \
		       -I$(top_srcdir)/makeclass \
		       -I$(top_srcdir)/epoll \
		       -I$(top_srcdir)/dns \
		       -I$(top_srcdir)/zmq \
		       -I$(top_srcdir)/socket
libratchet_la_LIBADD = ../misc/libratchetmisc.la \
		       ../makeclass/libratchetmakeclass.la \
		       ../zmq/libratchetzmq.la \
		       ../epoll/libratchetepoll.la \
		       ../dns/libratchetdns.la \
		       ../socket/libratchetsocket.la
libratchet_la_SOURCES = context.c context.h \
			ratchet.c

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_CFLAGS = $(libratchet_la_CFLAGS)
ratchet_la_LIBADD = libratchet.la
ratchet_la_LDFLAGS = -module -avoid-version
endif
