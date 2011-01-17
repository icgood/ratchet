#!/usr/bin/env lua

require "ratchet"

ctxs_hit = ""

function ctx2 (str)
    ctxs_hit = ctxs_hit .. "2"
    return "b" .. str
end

function ctx1 (r)
    ctxs_hit = ctxs_hit .. "1"
    local s = r:attach_wait(ctx2, "ooga")
    assert("booga" == s)
end

local r = ratchet.new()
r:attach(ctx1, r)
r:loop()
assert(ctxs_hit == "12")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
