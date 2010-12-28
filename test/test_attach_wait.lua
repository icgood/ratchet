#!/usr/bin/env lua

require "ratchet"

function ctx2 (str)
    return "b" .. str
end

function ctx1 (r)
    local s = r:attach_wait(ctx2, "ooga")
    assert("booga" == s)
end

local r = ratchet.new()
r:attach(ctx1, r)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
