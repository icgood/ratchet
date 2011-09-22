require "ratchet"

sacred = "important"

function ctx1 ()
    volatile = "trash"
    assert(sacred == "important")
end

local r = ratchet.new()
r:attach(ctx1)
r:loop()

assert(not volatile)
assert(sacred == "important")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
