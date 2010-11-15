#!/usr/bin/env lua

require("ratchet.zmq")

zmq = ratchet.zmq()

expected = "Hello...World!"
received = ""

s1 = zmq:listen('tcp://*:12345')
s2 = zmq:connect('tcp://localhost:12345')
poll = ratchet.zmq.poll()
poll:register(s1)

s2:sendsome('Hello...')
s2:send('World!')
for i, st, obj in poll:wait() do
    assert(obj == s1, "Received event on non-existent object")
    assert(st:readable(), "Received unknown event on object")
    received = received .. s1:recv()
    received = received .. s1:recv()
end

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
