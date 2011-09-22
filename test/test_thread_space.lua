require "ratchet"

local function check_ctx1_space(r)
    assert(r:thread_space().stuff == "data")
    assert(not r:thread_space().other_stuff)
end

function ctx1(r)
    r:thread_space().stuff = "data"
    check_ctx1_space(r)
end

local function check_ctx2_space(r)
    assert(r:thread_space().other_stuff == "other data")
    assert(not r:thread_space().stuff)
end

function ctx2(r)
    r:thread_space().other_stuff = "other data"
    check_ctx2_space(r)
end

local function check_ctx3_space(r)
    assert(r:thread_space().premade_stuff == "important")
end

function ctx3(r)
    r:thread_space({premade_stuff = "important"})
    check_ctx3_space(r)
end

local r = ratchet.new()
r:attach(ctx1, r)
r:attach(ctx2, r)
r:attach(ctx3, r)
r:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
