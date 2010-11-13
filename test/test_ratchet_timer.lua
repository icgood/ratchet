#!/usr/bin/env lua

require "ratchet"

expected = "Hello...World!"
received = ""

r = ratchet(ratchet.epoll())
r:register_uri("timer", ratchet.timer, ratchet.timer.parse_timer)

-- {{{ timer_context: Manages a timer fd.
timer_context = r:new_context()
function timer_context:on_init()
    self.i = 1
end
function timer_context:on_recv()
    local n = self:recv()
    assert(n == 1, "Received timer overrun.")
    print(self.i, "Timer triggered.")
    self.i = self.i + 1
end
-- }}}

t1 = r:attach(timer_context, r:uri("timer:3:1"))
assert(t1:isinstance(timer_context))

r:run()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
