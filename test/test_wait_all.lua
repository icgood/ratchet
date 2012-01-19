require "ratchet"

count = 0

local function ctx1(n)
    count = count + n
end

local function ctx2()
    local t1 = ratchet.thread.attach(ctx1, 1)
    local t2 = ratchet.thread.attach(ctx1, 2)
    local t3 = ratchet.thread.attach(ctx1, 3)
    local t4 = ratchet.thread.attach(ctx1, 4)
    local t5 = ratchet.thread.attach(ctx1, 5)

    ratchet.thread.wait_all({t1, t2, t3, t4, t5})
    assert(count == 15)
    count = count + 6
end

local r = ratchet.new(function ()
    ratchet.thread.attach(ctx2)
end)
r:loop()

assert(count == 21, count.." != 21")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
