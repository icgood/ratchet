require "ratchet"

function ctx1(where)
    local rec = ratchet.zmqsocket.prepare_uri(where)
    local socket = ratchet.zmqsocket.new(rec.type)
    socket:bind(rec.endpoint)

    kernel:attach(ctx2, "zmq:rep:tcp://127.0.0.1:10025")

    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv_all()
    assert(data == "world")
end

function ctx2(where)
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

kernel = ratchet.kernel.new()
kernel:attach(ctx1, "zmq:req:tcp://127.0.0.1:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
