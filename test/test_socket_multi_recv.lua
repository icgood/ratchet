require "ratchet"

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(client1, host, port)
    ratchet.thread.attach(client2, host, port)

    local c1 = socket:accept()
    local c2 = socket:accept()

    local dataA, socketA = assert(ratchet.socket.multi_recv {c1, c2})
    assert((dataA == "data1" and socketA == c1) or (dataA == "data2" and socketB == c2))
    local dataB, socketB = assert(ratchet.socket.multi_recv {c1, c2})
    assert((dataB == "data1" and socketB == c1) or (dataB == "data2" and socketB == c2))
    assert(dataA ~= dataB)

    c1:send("sync")
    c2:send("sync")

    local dataC, socketC = assert(ratchet.socket.multi_recv {c1, c2})
    assert(dataC == "data3" and socketC == c1)

    c1:send("sync")
    c2:send("sync")

    assert("" == ratchet.socket.multi_recv {c1, c2})
    assert("" == ratchet.socket.multi_recv {c1, c2})
    c1:close()
    c2:close()
end

function client1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    socket:send("data1")
    assert("sync" == socket:recv())

    socket:send("data3")
    assert("sync" == socket:recv())

    socket:close()
end

function client2(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    socket:send("data2")
    assert("sync" == socket:recv())

    assert("sync" == socket:recv())

    socket:close()
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
