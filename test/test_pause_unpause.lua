require "ratchet"

local function ctx2()
    ratchet.unpause(t1, "beep beep")
    local ret = ratchet.pause()
    assert(ret == "boop boop")
end

local function ctx1()
    local ret = ratchet.pause()
    assert(ret == "beep beep")
    ratchet.unpause(t2, "boop boop")
end

local r = ratchet.new()

t1 = r:attach(ctx1)
t2 = r:attach(ctx2)

r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
