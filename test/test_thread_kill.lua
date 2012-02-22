require "ratchet"

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:setsockopt("SO_REUSEADDR", true)
    socket:bind(rec.addr)
    socket:listen()

    local paused = {
        ratchet.thread.attach(ctx3),
        ratchet.thread.attach(ctx3),
        ratchet.thread.attach(ctx3),
        ratchet.thread.attach(ctx3),
        ratchet.thread.attach(ctx3),
    }
    local self = ratchet.thread.self()
    ratchet.thread.attach(ctx2, self, paused)

    local client = socket:accept()
end

function ctx2(thread, paused)
    ratchet.thread.kill(thread)
    ratchet.thread.kill_all(paused)
end

function ctx3()
    ratchet.thread.pause()
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()
assert(0 == kernel:get_num_threads())

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
