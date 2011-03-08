require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    socket:set_timeout(0.0)
    local client = socket:accept()
    assert(not client, "accept failed to timeout")
end

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
