require "ratchet"

function ctx1 (r)
    r:timer(5.0)
end

local r = ratchet.kernel.new()
r:attach(ctx1, r)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
