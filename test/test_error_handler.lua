require "ratchet"

function ctx1 ()
    error("uh oh")
end

function on_error(err)
    assert("uh oh" == tostring(err):sub(-5))
    error_happened = true
end

local r = ratchet.kernel.new()
r:set_error_handler(on_error)
r:attach(ctx1)
r:loop()
assert(error_happened)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
