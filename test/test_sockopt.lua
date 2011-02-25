require "ratchet"
require "test_config"

function ctx1(where)
    local rec = ratchet.socket.parse_uri(where, dns, dns_types)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    assert(socket.SO_ACCEPTCONN == true, "SO_ACCEPTCONN != true")

    assert(socket.SO_REUSEADDR == true, "SO_REUSEADDR != true")
    socket.SO_REUSEADDR = false
    assert(socket.SO_REUSEADDR == false, "SO_REUSEADDR != false")
    socket.SO_REUSEADDR = true
end

function ctx2(where)
    local rec = ratchet.socket.parse_uri(where, dns, dns_types)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)

    assert(socket.SO_ACCEPTCONN == false, "SO_ACCEPTCONN != false")

    -- Kernel doubles whatever value you set to SO_SND/RCVBUF.
    socket.SO_SNDBUF = 1024
    assert(socket.SO_SNDBUF == 2048, "SO_SNDBUF (" .. socket.SO_SNDBUF .. ") != 2048")
end

kernel = ratchet.new()
dns = ratchet.dns.new(kernel)
kernel:attach(ctx1, "tcp://127.0.0.1:10025")
kernel:attach(ctx2, "tcp://127.0.0.1:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
