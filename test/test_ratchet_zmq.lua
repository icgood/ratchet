#!/usr/bin/env lua

require("luah.ratchet")

expected = "Hello...Hello..."
received = ""

r = luah.ratchet()

-- {{{ user_context: Handles the connection from the "outside world"
user_context = r:new_context()
function user_context:on_init()
    self:send('Hello...')
end
function user_context:on_send(data)
    self:close()
end
-- }}}

-- {{{ client_context: Handles the "server-side" of the connection from the "outside world"
client_context = r:new_context()
function client_context:on_recv()
    local data = self:recv()
    received = received .. data
    self:close()
end
-- }}}

-- {{{ server_context: Accepts and initializes new connections from the "outside world"
server_context = r:new_context()
function server_context:on_recv()
    return self:accept(client_context)
end
-- }}}

s1 = r:listen('zmq:pull:tcp://*:12345', client_context)
s2 = r:connect('zmq:push:tcp://localhost:12345', user_context)
s3 = r:listen('tcp://*:1234', server_context)
s4 = r:connect('tcp://localhost:1234', user_context)

assert(s1:isinstance(client_context))
assert(s2:isinstance(user_context))
assert(s3:isinstance(server_context))
assert(s4:isinstance(user_context))

r:run{iterations=4, timeout=0.1, maxevents=1}

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
