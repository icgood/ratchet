require "ratchet"

function tcpctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(tcpctx2, host, port)

    local client = socket:accept()

    -- Portion being tested.
    --
    client:send("hi")
    local data = client:recv()
    assert(data == "there")
end

function tcpctx2(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hi")
    socket:send("there")
end

function zmqctx1(where)
    local rec = ratchet.zmqsocket.prepare_uri(where)
    local socket = ratchet.zmqsocket.new(rec.type)
    socket:bind(rec.endpoint)

    ratchet.thread.attach(zmqctx2, "rep:tcp://127.0.0.1:10026")

    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv_all()
    assert(data == "world")
end

function zmqctx2(where)
    local rec = ratchet.zmqsocket.prepare_uri(where)
    local socket = ratchet.zmqsocket.new(rec.type)
    socket:connect(rec.endpoint)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hello")
    socket:send("wo", true)
    socket:send("rld")
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(tcpctx1, "localhost", 10025)
    ratchet.thread.attach(zmqctx1, "req:tcp://*:10026")
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
