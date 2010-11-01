if HAVE_EPOLL
noinst_LTLIBRARIES = libratchetepoll.la
libratchetepoll_la_CFLAGS = -I$(top_srcdir)/misc \
			    -I$(top_srcdir)/makeclass
libratchetepoll_la_SOURCES = epoll.c epoll.h

if ENABLE_DEVEL
pkginclude_HEADERS = epoll.h
endif
endif
