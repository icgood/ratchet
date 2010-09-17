lualibdir = `pkg-config lua --variable=INSTALL_CMOD`
lualib_LTLIBRARIES = luah.la
luah_la_CFLAGS = -I$(top_srcdir)/misc \
		 -I$(top_srcdir)/makeclass \
		 -I$(top_srcdir)/rlimit \
		 -I$(top_srcdir)/zmq \
		 -I$(top_srcdir)/epoll \
		 -I$(top_srcdir)/dns \
		 -I$(top_srcdir)/socket \
		 -I$(top_srcdir)/xml \
		 -I$(top_srcdir)/ratchet
luah_la_LIBADD = ../misc/libluahmisc.la \
		 ../makeclass/libluahmakeclass.la \
		 ../rlimit/libluahrlimit.la \
		 ../zmq/libluahzmq.la \
		 ../epoll/libluahepoll.la \
		 ../dns/libluahdns.la \
		 ../socket/libluahsocket.la \
		 ../xml/libluahxml.la \
		 ../ratchet/libluahratchet.la
luah_la_LDFLAGS = -module -shared -avoid-version
luah_la_SOURCES = luah.c

if ENABLE_DEVEL
lib_LTLIBRARIES = libluah.la
include_HEADERS = luah.h
endif
libluah_la_CFLAGS = $(luah_la_CFLAGS)
libluah_la_LIBADD = $(luah_la_LIBADD)
libluah_la_SOURCES = $(luah_la_SOURCES)

