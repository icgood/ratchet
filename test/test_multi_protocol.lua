require "ratchet"

function tcpctx1(where)
    local rec = ratchet.socket.parse_uri(where, dns)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    kernel:attach(tcpctx2, "tcp://localhost:10025")

    local client = socket:accept()

    -- Portion being tested.
    --
    client:send("hi")
    local data = client:recv()
    assert(data == "there")
end

function tcpctx2(where)
    local rec = ratchet.socket.parse_uri(where, dns)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hi")
    socket:send("there")
end

function zmqctx1(where)
    local t, e = ratchet.zmqsocket.parse_uri(where)
    local socket = ratchet.zmqsocket.new(t)
    socket:bind(e)

    r:attach(zmqctx2, "zmq:rep:tcp://127.0.0.1:10026")

    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv_all()
    assert(data == "world")
end

function zmqctx2(where)
    local t, e = ratchet.zmqsocket.parse_uri(where)
    local socket = ratchet.zmqsocket.new(t)
    socket:connect(e)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hello")
    socket:send("wo", true)
    socket:send("rld")
end

kernel = ratchet.new()
dns = ratchet.dns.new(kernel)
kernel:attach(tcpctx1, "tcp://*:10025")
kernel:attach(zmqctx1, "zmq:req:tcp://*:10026")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
