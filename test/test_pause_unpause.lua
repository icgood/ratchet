require "ratchet"

local function ctx2(t)
    ratchet.unpause(t, "beep beep")
end

local function ctx1()
    local ret = ratchet.pause()
    assert(ret == "beep beep")
end

local r = ratchet.new()

local t1 = r:attach(ctx1)
r:attach(ctx2, t1)

r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
