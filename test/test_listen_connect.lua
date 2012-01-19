require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where, {"ipv4"})
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ctx2("tcp://localhost:10025")

    local client, from = socket:accept()

    local ptr = ratchet.dns.query(from, "ptr")
    assert(ptr and ptr[1] == "localhost.")
end

function ctx2(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "tcp://*:10025")
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
