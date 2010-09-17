noinst_LTLIBRARIES = libluahratchet.la
libluahratchet_la_CFLAGS = -I$(top_srcdir)/misc \
			   -I$(top_srcdir)/makeclass \
			   -I$(top_srcdir)/epoll \
			   -I$(top_srcdir)/dns \
			   -I$(top_srcdir)/zmq \
			   -I$(top_srcdir)/socket
libluahratchet_la_SOURCES = ratchet.c ratchet.h \
			    parseuri.c parseuri.h \
			    context.c context.h
