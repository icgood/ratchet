
require "ratchet"

ratchet.bus.samestate_transaction = {}
ratchet.bus.samestate_transaction.__index = ratchet.bus.samestate_transaction

-- {{{ ratchet.bus.samestate_transaction.new()
function ratchet.bus.samestate_transaction.new(request)
    local self = {}
    setmetatable(self, ratchet.bus.samestate_transaction)

    self.request = request

    return self
end
-- }}}

-- {{{ ratchet.bus.samestate_transaction:send_response()
function ratchet.bus.samestate_transaction:send_response(res)
    self.response = res

    if self.waiting_thread then
        ratchet.kernel.unpause(self.waiting_thread, res)
    end
end
-- }}}

-- {{{ ratchet.bus.samestate_transaction:recv_response()
function ratchet.bus.samestate_transaction:recv_response()
    if self.response then
        return self.response
    else
        self.waiting_thread = ratchet.kernel.running_thread()
        local ret = ratchet.kernel.pause()
        self.waiting_thread = nil
        return ret
    end
end
-- }}}

return ratchet.bus.samestate_transaction

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
