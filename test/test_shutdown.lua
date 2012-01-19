require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(ctx2, "tcp://localhost:10025")

    local client = socket:accept()

    -- Portion being tested.
    --
    client:shutdown("write")
    local data = client:recv()
    assert(data == "ooga")
end

function ctx2(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "")
    socket:send("ooga")
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "tcp://localhost:10025")
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
