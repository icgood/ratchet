noinst_LTLIBRARIES = libratchetzmq.la
libratchetzmq_la_CFLAGS = -I$(top_srcdir)/misc \
			  -I$(top_srcdir)/makeclass
libratchetzmq_la_SOURCES = zmq_socket.h zmq_socket.c \
			   zmq_poll.h zmq_poll.c \
			   zmq_main.h zmq_main.c

if ENABLE_DEVEL
pkginclude_HEADERS = zmq_main.h zmq_socket.h zmq_poll.h
endif
