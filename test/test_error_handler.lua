#!/usr/bin/env lua

require "ratchet"

function ctx1 ()
    error("uh oh")
end

function on_error(err)
    assert("./test_error_handler.lua:6: uh oh" == tostring(err))
    error_happened = true
end

local r = ratchet.new()
r:set_error_handler(on_error)
r:attach(ctx1)
r:loop()
assert(error_happened)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
