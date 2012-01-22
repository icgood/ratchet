require "ratchet"
require "ratchet.uri"

function ctx1(where)
    local socket = ratchet.uri.parse(where, "listen", {"ipv4"})

    connect_client(where)

    local client, from = socket:accept()

    local ptr = ratchet.dns.query(from, "ptr")
    assert(ptr and ptr[1] == "localhost.")
end

function connect_client(where)
    local socket = ratchet.uri.parse(where, "connect", {"ipv4"})
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "tcp://localhost:10025")
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
