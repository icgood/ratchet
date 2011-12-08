require "ratchet"

local function ctx2()
    ratchet.thread.unpause(t1, "beep beep")
    local ret = ratchet.thread.pause()
    assert(ret == "boop boop")
end

local function ctx1()
    local ret = ratchet.thread.pause()
    assert(ret == "beep beep")
    ratchet.thread.unpause(t2, "boop boop")
end

local r = ratchet.new(function ()
    t1 = ratchet.thread.attach(ctx1)
    t2 = ratchet.thread.attach(ctx2)
end)

r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
