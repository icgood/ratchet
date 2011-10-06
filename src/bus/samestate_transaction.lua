
module("ratchet.bus.samestate_transaction", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(ratchet_obj, request)
    local self = {}
    setmetatable(self, class)

    self.ratchet_obj = ratchet_obj
    self.request = request

    return self
end
-- }}}

-- {{{ send_response()
function send_response(self, res)
    self.response = res

    if self.waiting_thread then
        self.ratchet_obj:unpause(self.waiting_thread, res)
    end
end
-- }}}

-- {{{ recv_response()
function recv_response(self)
    if self.response then
        return self.response
    else
        self.waiting_thread = self.ratchet_obj:running_thread()
        local ret = self.ratchet_obj:pause()
        self.waiting_thread = nil
        return ret
    end
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
