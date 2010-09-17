noinst_LTLIBRARIES = libluahepoll.la
libluahepoll_la_CFLAGS = -I$(top_srcdir)/misc \
			 -I$(top_srcdir)/makeclass
libluahepoll_la_SOURCES = epoll.c epoll.h

if ENABLE_DEVEL
pkginclude_HEADERS = epoll.h
endif
