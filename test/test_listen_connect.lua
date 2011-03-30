require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where, {"ipv4"})
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ctx2("tcp://localhost:10025")

    local client, from = socket:accept()
    assert(from == "127.0.0.1")
end

function ctx2(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)
end

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://*:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
