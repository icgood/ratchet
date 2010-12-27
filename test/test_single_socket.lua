#!/usr/bin/env lua

require "ratchet"

function ctx (where)
    local s = ratchet.socket.new()
    local sa = s:resolve(where)
    s:connect(sa)
    s:send("hello")
    local r = s:recv()
    s:close()

    assert(r == "world")
end

u = ratchet.uri.new()
u:register("tcp", ratchet.socket.parse_tcp_uri)

r = ratchet.new()
r:attach(ctx, u("tcp://localhost:25"))
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
