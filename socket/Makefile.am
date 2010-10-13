noinst_LTLIBRARIES = libratchetsocket.la
libratchetsocket_la_CFLAGS = -I$(top_srcdir)/misc \
			     -I$(top_srcdir)/makeclass
libratchetsocket_la_SOURCES = socket.c socket.h

if ENABLE_DEVEL
pkginclude_HEADERS = socket.h
endif
