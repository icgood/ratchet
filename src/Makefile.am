allsources = ratchet.h ratchet.c \
	     misc.h misc.c

if HAVE_SOCKET
allsources += sockopt.h sockopt.c socket.c
endif

if HAVE_ZMQ
allsources += zmq.c
endif

if HAVE_TIMERFD
allsources += timerfd.c
endif

if HAVE_OPENSSL
allsources += ssl.c
endif

if ENABLE_DEVEL
include_HEADERS = ratchet.h

lib_LTLIBRARIES = libratchet.la
else
noinst_LTLIBRARIES = libratchet.la
endif

if HAVE_DNS
libratchet_la_LIBADD = dns/libdns.la
endif
libratchet_la_CFLAGS = -std=gnu99 -Wall -Wno-unused-function -Wno-unused-label
libratchet_la_LDFLAGS = 
if HAVE_OPENSSL
libratchet_la_CFLAGS += $(OPENSSL_INCLUDES)
libratchet_la_LDFLAGS += $(OPENSSL_LDFLAGS) $(OPENSSL_LIBS)
endif
libratchet_la_SOURCES = $(allsources)

if ENABLE_MODULE
lualibdir = @MODULE_PATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_LDFLAGS = -module -avoid-version -shared
ratchet_la_LIBADD = libratchet.la
ratchet_la_SOURCES =
endif

SUBDIRS = uri

if HAVE_SOCKET
SUBDIRS += socketpad
endif

if HAVE_DNS
SUBDIRS += dns
endif

if ENABLE_HTTP
SUBDIRS += http
if ENABLE_BUS
SUBDIRS += bus
endif
endif

if ENABLE_SMTP
SUBDIRS += smtp
endif

