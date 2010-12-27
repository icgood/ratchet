if ENABLE_DEVEL
include_HEADERS = ratchet.h
endif

if ENABLE_DEVEL
lib_LTLIBRARIES = libratchet.la
else
noinst_LTLIBRARIES = libratchet.la
endif
libratchet_la_SOURCES = ratchet.h ratchet.c misc.h misc.c \
			socket.c uri.c

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_CFLAGS = $(libratchet_la_CFLAGS)
ratchet_la_LIBADD = libratchet.la
ratchet_la_LDFLAGS = -module -avoid-version
endif
