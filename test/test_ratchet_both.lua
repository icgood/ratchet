#!/usr/bin/env lua

require "ratchet"

expected = "Hello...Hello..."
received = ""

zmqr = ratchet(ratchet.zmq.poll())
zmqr:register_uri("zmq", ratchet.zmq.socket, ratchet.zmq.parse_uri)

epr = ratchet(ratchet.epoll())
epr:register_uri("tcp", ratchet.socket, ratchet.socket.parse_tcp_uri)

done = 0

-- {{{ epoll_context: Handles events from the epoll-based ratchet.
epoll_context = zmqr:new_context()
function epoll_context:on_recv()
    return epr:run{iterations=1, timeout=0.0, maxevents=5}
end
-- }}}

-- {{{ user_context: Handles the connection from the "outside world"
user_context = zmqr:new_context()
function user_context:on_init()
    self:send("Hello...")
end
function user_context:on_send(data)
    self:close()
    done = done + 1
end
-- }}}

-- {{{ client_context: Handles the "server-side" of the connection from the "outside world"
client_context = zmqr:new_context()
function client_context:on_recv()
    local data = self:recv()
    received = received .. data
    self:close()
    done = done + 1
end
-- }}}

-- {{{ server_context: Accepts and initializes new connections from the "outside world"
server_context = zmqr:new_context()
function server_context:on_recv()
    return self:accept(client_context)
end
-- }}}

s1 = zmqr:attach(epoll_context, epr)
s2 = zmqr:attach(client_context, zmqr:listen_uri('zmq:pull:tcp://*:12345'))
s3 = zmqr:attach(user_context, zmqr:connect_uri('zmq:push:tcp://localhost:12345'))
s4 = epr:attach(server_context, epr:listen_uri('tcp://*:1234'))
s5 = epr:attach(user_context, epr:connect_uri('tcp://localhost:1234'))

zmqr:run_until({timeout=0.1}, function () return done >= 4 end)

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
