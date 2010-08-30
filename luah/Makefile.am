libdir = `pkg-config lua --variable=INSTALL_CMOD`
lib_LTLIBRARIES = luah.la
include_HEADERS = luah.h
luah_la_CFLAGS = -I$(top_srcdir)/misc \
		 -I$(top_srcdir)/makeclass \
		 -I$(top_srcdir)/epoll \
		 -I$(top_srcdir)/socket
luah_la_LIBADD = ../misc/libluahmisc.la \
		 ../makeclass/libluahmakeclass.la \
		 ../epoll/libluahepoll.la \
		 ../socket/libluahsocket.la
luah_la_LDFLAGS = -module -avoid-version
luah_la_SOURCES = luah.c

