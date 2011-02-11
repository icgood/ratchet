#!/usr/bin/env lua

require "ratchet"

function ctx1 (r)
    local tfd = ratchet.timerfd.new()
    tfd:settime(1.0, 1.0)

    while true do
        local fired = tfd:read()
        print("fire!")
    end
end

local r = ratchet.new()
r:attach(ctx1, r)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
