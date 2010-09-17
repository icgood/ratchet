noinst_LTLIBRARIES = libluahzmq.la
libluahzmq_la_CFLAGS = -I$(top_srcdir)/misc \
		       -I$(top_srcdir)/makeclass
libluahzmq_la_SOURCES = zmq_socket.h zmq_socket.c \
			zmq_poll.h zmq_poll.c \
			zmq_main.h zmq_main.c

if ENABLE_DEVEL
pkginclude_HEADERS = zmq_main.h zmq_socket.h zmq_poll.h
endif
