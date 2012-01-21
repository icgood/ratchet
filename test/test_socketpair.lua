require "ratchet"

function ctx1()
    local socket_a, socket_b = ratchet.socket.new_pair()

    ratchet.thread.attach(ctx2, socket_b)

    socket_a:send("hello")
    local data = socket_a:recv(5)
    assert(data == "world")

    local data = socket_a:recv()
    assert(data == "foo")
    socket_a:send("bar")
end

function ctx2(socket_b)
    local data = socket_b:recv(5)
    assert(data == "hello")
    socket_b:send("world")

    socket_b:send("foo")
    local data = socket_b:recv()
    assert(data == "bar")
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1)
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
