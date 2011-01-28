#!/usr/bin/lua

require "ratchet"

uri = ratchet.uri.new()
uri:register("tcp", ratchet.socket.parse_tcp_uri)

function ctx1(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    assert(socket.SO_ACCEPTCONN == true, "SO_ACCEPTCONN != true")

    assert(socket.SO_REUSEADDR == true, "SO_REUSEADDR != true")
    socket.SO_REUSEADDR = false
    assert(socket.SO_REUSEADDR == false, "SO_REUSEADDR != false")
    socket.SO_REUSEADDR = true
end

function ctx2(r, where)
    local rec = r:resolve_dns(uri(where))
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)

    assert(socket.SO_ACCEPTCONN == false, "SO_ACCEPTCONN != false")

    -- Kernel doubles whatever value you set to SO_SND/RCVBUF.
    socket.SO_SNDBUF = 1024
    assert(socket.SO_SNDBUF == 2048, "SO_SNDBUF (" .. socket.SO_SNDBUF .. ") != 2048")
end

local r = ratchet.new()
r:attach(ctx1, r, "tcp://localhost:10025")
r:attach(ctx2, r, "tcp://localhost:10025")
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
