require "ratchet"

local function check_ctx1_space(r)
    assert(ratchet.thread.space().stuff == "data")
    assert(not ratchet.thread.space().other_stuff)
end

function ctx1(r)
    ratchet.thread.space().stuff = "data"
    check_ctx1_space(r)
end

local function check_ctx2_space(r)
    assert(ratchet.thread.space().other_stuff == "other data")
    assert(not ratchet.thread.space().stuff)
end

function ctx2(r)
    ratchet.thread.space().other_stuff = "other data"
    check_ctx2_space(r)
end

local function check_ctx3_space(r)
    assert(ratchet.thread.space().premade_stuff == "important")
end

function ctx3(r)
    ratchet.thread.space({premade_stuff = "important"})
    check_ctx3_space(r)
end

local r = ratchet.new(function ()
    ratchet.thread.attach(ctx1, r)
    ratchet.thread.attach(ctx2, r)
    ratchet.thread.attach(ctx3, r)
end)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
