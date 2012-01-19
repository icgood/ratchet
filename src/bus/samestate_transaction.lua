
require "ratchet"

module("ratchet.bus.samestate_transaction", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(request)
    local self = {}
    setmetatable(self, class)

    self.request = request

    return self
end
-- }}}

-- {{{ send_response()
function send_response(self, res)
    self.response = res

    if self.waiting_thread then
        ratchet.thread.unpause(self.waiting_thread, res)
    end
end
-- }}}

-- {{{ recv_response()
function recv_response(self)
    if self.response then
        return self.response
    else
        self.waiting_thread = ratchet.thread.self()
        local ret = ratchet.thread.pause()
        self.waiting_thread = nil
        return ret
    end
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
