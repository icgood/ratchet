#!/usr/bin/env lua

require "ratchet"

local function ctx2(r, t)
    r:unpause(t, "beep beep")
end

local function ctx1(r)
    local ret = r:pause()
    assert(ret == "beep beep")
end

local r = ratchet.new()

local t1 = r:attach(ctx1, r)
r:attach(ctx2, r, t1)

r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
