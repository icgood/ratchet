libdir = `pkg-config lua --variable=INSTALL_CMOD`
lib_LTLIBRARIES = luah.la
include_HEADERS = luah.h
luah_la_CFLAGS = -I$(top_srcdir)/misc \
		 -I$(top_srcdir)/makeclass \
		 -I$(top_srcdir)/rlimit \
		 -I$(top_srcdir)/zmq \
		 -I$(top_srcdir)/epoll \
		 -I$(top_srcdir)/dns \
		 -I$(top_srcdir)/socket \
		 -I$(top_srcdir)/ratchet
luah_la_LIBADD = ../misc/libluahmisc.la \
		 ../makeclass/libluahmakeclass.la \
		 ../rlimit/libluahrlimit.la \
		 ../zmq/libluahzmq.la \
		 ../epoll/libluahepoll.la \
		 ../dns/libluahdns.la \
		 ../socket/libluahsocket.la \
		 ../ratchet/libluahratchet.la
luah_la_LDFLAGS = -module -avoid-version
luah_la_SOURCES = luah.c

