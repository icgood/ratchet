#!/usr/bin/env lua

require "luah"

expected = "Hello...Hello..."
received = ""

zmqr = luah.ratchet(luah.zmq.poll())
zmqr:register("zmq", luah.zmq.socket, luah.zmq.parse_uri)

epr = luah.ratchet(luah.epoll())
epr:register("tcp", luah.socket, luah.socket.parse_tcp_uri)

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

s1 = zmqr:attach(epr, epoll_context)
s2 = zmqr:listen('zmq:pull:tcp://*:12345', client_context)
s3 = zmqr:connect('zmq:push:tcp://localhost:12345', user_context)
s4 = epr:listen('tcp://*:1234', server_context)
s5 = epr:connect('tcp://localhost:1234', user_context)

zmqr:run_until({timeout=0.1}, function () return done >= 4 end)

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
