#!/usr/bin/env lua

require "ratchet"

expected = "Hello...World!"
received = ""

r = ratchet(ratchet.epoll())
r:register_uri('tcp', ratchet.socket, ratchet.socket.parse_tcp_uri)

-- {{{ user_context: Handles the connection from the "outside world"
user_context = r:new_context()
function user_context:on_init()
    self:send('Hello')
    self:send('...')
end
function user_context:on_recv()
    local data = self:recv()
    received = received .. data
    self:close()
end
-- }}}

-- {{{ client_context: Handles the "server-side" of the connection from the "outside world"
client_context = r:new_context()
function client_context:on_recv()
    local data = self:recv()
    received = received .. data
    self:send('World!')
end
function client_context:on_send(data)
    self:close()
end
-- }}}

-- {{{ server_context: Accepts and initializes new connections from the "outside world"
server_context = r:new_context()
function server_context:on_recv()
    return self:accept(client_context)
end
-- }}}

s1 = r:attach(server_context, r:listen_uri('tcp://localhost:1234'))
s2 = r:attach(user_context, r:connect_uri('tcp://localhost:1234'))

assert(s1:isinstance(server_context))
assert(s2:isinstance(user_context))

r:run{iterations=5, timeout=0.1, maxevents=1}

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
