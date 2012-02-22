require "ratchet"

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:setsockopt("SO_REUSEADDR", true)
    socket:bind(rec.addr)
    socket:listen()

    socket:set_timeout(0.0)
    local worked, err = pcall(socket.accept, socket)
    assert(not worked and ratchet.error.is(err, "ETIMEDOUT"), "accept failed to timeout")
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
