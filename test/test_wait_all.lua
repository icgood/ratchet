require "ratchet"

count = 0

local function ctx1(n)
    _globals.count = _globals.count + n
end

local function ctx2(r)
    local t1 = r:attach(ctx1, 1)
    local t2 = r:attach(ctx1, 2)
    local t3 = r:attach(ctx1, 3)
    local t4 = r:attach(ctx1, 4)
    local t5 = r:attach(ctx1, 5)

    r:wait_all({t1, t2, t3, t4, t5})
    assert(_globals.count == 15)
    _globals.count = _globals.count + 6
end

local r = ratchet.new()

r:attach(ctx2, r)

r:loop()
assert(count == 21, count.." != 21")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
