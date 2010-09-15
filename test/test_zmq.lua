#!/usr/bin/env lua

require("luah.zmq")

zmq = luah.zmq()

expected = "Hello...World!"
received = ""

s1 = zmq:listen('tcp://*:12345')
s2 = zmq:connect('tcp://localhost:12345')
poll = luah.zmq.poll()
poll:register(s1)

s2:send('Hello...World!')
for i, e, o in poll:wait(0.1) do
    assert(o == s1, "Received event on non-existent object")
    assert(poll.happened(e, poll.IN), "Received unknown event on object")
    received = received .. s1:recv()
end

assert(expected == received, "Expected: [" .. expected .. "] Received: [" .. received .. "]")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
