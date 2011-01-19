#!/usr/bin/lua

require "ratchet"

uri = ratchet.uri.new()
uri:register("zmq", ratchet.zmqsocket.parse_uri)

function ctx1(r, where)
    local t, e = uri(where)
    local socket = ratchet.zmqsocket.new(t)
    socket:bind(e)

    r:attach(ctx2, r, "zmq:rep:tcp://127.0.0.1:10025")

    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv()
    assert(data == "world")
end

function ctx2(r, where)
    local t, e = uri(where)
    local socket = ratchet.zmqsocket.new(t)
    socket:connect(e)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hello")
    socket:send("wo", true)
    socket:send("rld")
end

local r = ratchet.new()
r:attach(ctx1, r, "zmq:req:tcp://127.0.0.1:10025")
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
