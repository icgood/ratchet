include_HEADERS = ratchet.h

lib_LTLIBRARIES = libratchet.la
libratchet_la_SOURCES = ratchet.h ratchet.c misc.h misc.c \
			socket.c uri.c

if ENABLE_MODULE
lualibdir = @LUA_CPATH@
lualib_LTLIBRARIES = ratchet.la
ratchet_la_LIBADD = libratchet.la
ratchet_la_LDFLAGS = -module -avoid-version
endif
