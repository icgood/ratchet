#!/usr/bin/lua

require "ratchet"

uri = ratchet.uri.new()
uri:register("tcp", ratchet.socket.parse_tcp_uri)

function ctx1(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:bind(rec.addr)
    socket:listen()

    r:attach_wait(ctx2, r, "tcp://localhost:10025")
end

function ctx2(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)
end

local r = ratchet.new()
r:attach(ctx1, r, "tcp://*:10025")
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
