require "ratchet"

local function ctx2()
    ratchet.kernel.unpause(t1, "beep beep")
    local ret = ratchet.kernel.pause()
    assert(ret == "boop boop")
end

local function ctx1()
    local ret = ratchet.kernel.pause()
    assert(ret == "beep beep")
    ratchet.kernel.unpause(t2, "boop boop")
end

local r = ratchet.kernel.new()

t1 = r:attach(ctx1)
t2 = r:attach(ctx2)

r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
