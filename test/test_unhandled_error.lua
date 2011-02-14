#!/usr/bin/env lua

require "ratchet"

function ctx1 ()
    error("uh oh")
end

local r = ratchet.new()
r:attach(ctx1)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
