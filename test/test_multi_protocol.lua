#!/usr/bin/lua

require "ratchet"

uri = ratchet.uri.new()
uri:register("tcp", ratchet.socket.parse_tcp_uri)
uri:register("zmq", ratchet.zmqsocket.parse_uri)

function tcpctx1(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:bind(rec.addr)
    socket:listen()

    r:attach(tcpctx2, r, "tcp://localhost:10025")

    local client = socket:accept()

    -- Portion being tested.
    --
    client:send("hi")
    local data = client:recv()
    assert(data == "there")
end

function tcpctx2(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hi")
    socket:send("there")
end

function zmqctx1(r, where)
    local t, e = uri(where)
    local socket = ratchet.zmqsocket.new(t)
    socket:bind(e)

    r:attach(zmqctx2, r, "zmq:rep:tcp://127.0.0.1:10026")

    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv()
    assert(data == "world")
end

function zmqctx2(r, where)
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
r:attach(tcpctx1, r, "tcp://*:10025")
r:attach(zmqctx1, r, "zmq:req:tcp://*:10026")
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
