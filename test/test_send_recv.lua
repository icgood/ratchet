require "ratchet"

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:setsockopt("SO_REUSEADDR", true)
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(ctx3, host, port)

    local client = socket:accept()
    ratchet.thread.attach(ctx2, client)
end

function ctx2(socket)
    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv(5)
    assert(data == "world")

    local data = socket:recv()
    assert(data == "foo")
    socket:send("bar")
end

function ctx3(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv(5)
    assert(data == "hello")
    socket:send("world")

    socket:send("foo")
    local data = socket:recv()
    assert(data == "bar")
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
