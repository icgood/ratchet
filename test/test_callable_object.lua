require "ratchet"

local function call_it(self)
    assert(self.data == "beep beep")
    run = true
end

local obj = {data = "beep beep"}
setmetatable(obj, {__call = call_it})

local r = ratchet.kernel.new()
r:attach(obj)
r:loop()

assert(run, "object did not get called")

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
