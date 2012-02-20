
require "ratchet"

local samestate_transaction = {}
samestate_transaction.__index = samestate_transaction

-- {{{ samestate_transaction.new()
function samestate_transaction.new(request)
    local self = {}
    setmetatable(self, samestate_transaction)

    self.request = request

    return self
end
-- }}}

-- {{{ samestate_transaction:send_response()
function samestate_transaction:send_response(res)
    self.response = res

    if self.waiting_thread then
        ratchet.thread.unpause(self.waiting_thread, res)
        self.waiting_thread = nil
    end
end
-- }}}

-- {{{ samestate_transaction:recv_response()
function samestate_transaction:recv_response()
    if self.response then
        return self.response
    else
        self.waiting_thread = ratchet.thread.self()
        return ratchet.thread.pause()
    end
end
-- }}}

return samestate_transaction

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
