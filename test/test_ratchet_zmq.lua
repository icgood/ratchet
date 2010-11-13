#!/usr/bin/env lua

require "ratchet"

expected = "Hello...Hello..."
received = ""

r = ratchet(ratchet.zmq.poll())
r:register_uri("zmq", ratchet.zmq.socket, ratchet.zmq.parse_uri)
r:register_uri("tcp", ratchet.socket, ratchet.socket.parse_tcp_uri)

done = 0

-- {{{ user_context: Handles the connection from the "outside world"
user_context = r:new_context()
function user_context:on_init()
    self:send("Hello...")
end
function user_context:on_send(data)
    self:close()
    done = done + 1
end
-- }}}

-- {{{ client_context: Handles the "server-side" of the connection from the "outside world"
client_context = r:new_context()
function client_context:on_recv()
    local data = self:recv()
    received = received .. data
    self:close()
    done = done + 1
end
-- }}}

-- {{{ server_context: Accepts and initializes new connections from the "outside world"
server_context = r:new_context()
function server_context:on_recv()
    return self:accept(client_context)
end
-- }}}

s1 = r:attach(client_context, r:listen_uri('zmq:pull:tcp://*:12345'))
s2 = r:attach(user_context, r:connect_uri('zmq:push:tcp://localhost:12345'))
s3 = r:attach(server_context, r:listen_uri('tcp://*:1234'))
s4 = r:attach(user_context, r:connect_uri('tcp://localhost:1234'))

assert(s1:isinstance(client_context))
assert(s2:isinstance(user_context))
assert(s3:isinstance(server_context))
assert(s4:isinstance(user_context))

r:run_until({timeout=0.1}, function () return done >= 4 end)

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
