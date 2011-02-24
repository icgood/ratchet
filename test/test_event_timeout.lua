require "ratchet"

function ctx1(where)
    local rec = ratchet.socket.parse_uri(where, dns, "ipv6", "ipv4")
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    socket:set_timeout(0.01)
    local client = socket:accept()
    assert(not client, "accept failed to timeout")
end

kernel = ratchet.new()
dns = ratchet.dns.new(kernel)
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
