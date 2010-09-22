if ENABLE_DEVEL
include_HEADERS = luah.h
endif
lib_LTLIBRARIES = libluah.la
libluah_la_CFLAGS = -I$(top_srcdir)/misc \
		    -I$(top_srcdir)/makeclass \
		    -I$(top_srcdir)/rlimit \
		    -I$(top_srcdir)/zmq \
		    -I$(top_srcdir)/epoll \
		    -I$(top_srcdir)/dns \
		    -I$(top_srcdir)/socket \
		    -I$(top_srcdir)/xml \
		    -I$(top_srcdir)/ratchet
libluah_la_LIBADD = ../misc/libluahmisc.la \
		    ../makeclass/libluahmakeclass.la \
		    ../rlimit/libluahrlimit.la \
		    ../zmq/libluahzmq.la \
		    ../epoll/libluahepoll.la \
		    ../dns/libluahdns.la \
		    ../socket/libluahsocket.la \
		    ../xml/libluahxml.la \
		    ../ratchet/libluahratchet.la
libluah_la_SOURCES = luah.c

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = luah.la
luah_la_CFLAGS = $(libluah_la_CFLAGS)
luah_la_LIBADD = libluah.la
luah_la_LDFLAGS = -module -avoid-version
endif

