require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    kernel:attach(ctx3, "tcp://localhost:10025")

    local client = socket:accept()
    kernel:attach(ctx2, client)
end

function ctx2(socket)
    -- Portion being tested.
    --
    local bytestr = socket:recv()
    assert(4 == #bytestr)
    assert(1333337 == ratchet.socket.ntoh(bytestr))

    bytestr = ratchet.socket.hton(73, true)
    socket:send(bytestr)
end

function ctx3(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local bytestr = ratchet.socket.hton(1333337)
    socket:send(bytestr)

    bytestr = socket:recv()
    assert(2 == #bytestr)
    assert(73 == ratchet.socket.ntoh(bytestr))
end

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
