require "ratchet"

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    local self = ratchet.thread.self()
    ratchet.thread.attach(ctx2, self)

    local client = socket:accept()
end

function ctx2(thread)
    ratchet.thread.kill(thread)
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()
assert(0 == kernel:get_num_threads())

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
