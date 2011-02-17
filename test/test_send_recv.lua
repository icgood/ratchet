require "ratchet"

uri = ratchet.uri.new()
uri:register("tcp", ratchet.socket.parse_tcp_uri)

function ctx1(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    r:attach(ctx3, r, "tcp://localhost:10025")

    local client = socket:accept()
    r:attach(ctx2, r, client)
end

function ctx2(r, socket)
    -- Portion being tested.
    --
    socket:send("hello")
    local data = socket:recv()
    assert(data == "world")

    local data = socket:recv()
    assert(data == "foo")
    socket:send("bar")

    --socket:close()
end

function ctx3(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    local data = socket:recv()
    assert(data == "hello")
    socket:send("world")

    socket:send("foo")
    local data = socket:recv()
    assert(data == "bar")


    --socket:close()
end

local r = ratchet.new()
r:attach(ctx1, r, "tcp://localhost:10025")
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
