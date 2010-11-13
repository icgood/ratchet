if HAVE_TIMERFD
noinst_LTLIBRARIES = libratchettimer.la
libratchettimer_la_CFLAGS = -I$(top_srcdir)/misc \
			     -I$(top_srcdir)/makeclass
libratchettimer_la_SOURCES = timer.c timer.h

if ENABLE_DEVEL
pkginclude_HEADERS = timer.h
endif
endif
