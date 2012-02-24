
http_sources = http/client.lua \
	       http/server.lua \
	       http/common.lua

smtp_sources = smtp/client.lua \
	       smtp/server.lua \
	       smtp/data_reader.lua \
	       smtp/data_sender.lua \
	       smtp/smtp_extensions.lua \
	       smtp/smtp_io.lua \
	       smtp/smtp_reply.lua \
	       smtp/smtp_auth.lua

bus_sources = bus/init.lua \
	      bus/samestate.lua \
	      bus/samestate_transaction.lua \
	      bus/client.lua \
	      bus/client_transaction.lua \
	      bus/server.lua \
	      bus/server_transaction.lua

socketpad_sources = socketpad/init.lua

if ENABLE_HTTP
httpdir = @LUA_LPATH@/ratchet/http
dist_http_DATA = $(http_sources)
endif

if ENABLE_SMTP
smtpdir = @LUA_LPATH@/ratchet/smtp
dist_smtp_DATA = $(smtp_sources)
endif

if ENABLE_BUS
busdir = @LUA_LPATH@/ratchet/bus
dist_bus_DATA = $(bus_sources)
endif

if ENABLE_SOCKETPAD
socketpaddir = @LUA_LPATH@/ratchet/socketpad
dist_socketpad_DATA = $(socketpad_sources)
endif

