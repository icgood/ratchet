#!/usr/bin/env lua

require "luah"

expected = "Hello...World!"
received = ""

r = luah.ratchet(luah.epoll())
r:register('tcp', luah.socket, luah.socket.parse_tcp_uri)

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

s1 = r:listen('tcp://localhost:1234', server_context)
s2 = r:connect('tcp://localhost:1234', user_context)

assert(s1:isinstance(server_context))
assert(s2:isinstance(user_context))

r:run{iterations=5, timeout=0.1, maxevents=1}

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
