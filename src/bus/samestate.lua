
require "ratchet"
local samestate_transaction = require "ratchet.bus.samestate_transaction"

local samestate = {}
samestate.__index = samestate

-- {{{ samestate.new()
function samestate.new()
    local self = {}
    setmetatable(self, samestate)

    self.queue = {}

    return self, self
end
-- }}}

-- {{{ wait_for_transaction()
local function wait_for_transaction(self)
    if not self.queue[1] then
        self.waiting_thread = ratchet.thread.self()
        local ret = ratchet.thread.pause()
        self.waiting_thread = nil
        return ret
    else
        return table.remove(self.queue, 1)
    end
end
-- }}}

-- {{{ samestate:recv_request()
function samestate:recv_request()
    local transaction = wait_for_transaction(self)
    return transaction, transaction and transaction.request
end
-- }}}

-- {{{ send_or_queue_transaction()
local function send_or_queue_transaction(self, data)
    if self.waiting_thread then
        ratchet.thread.unpause(self.waiting_thread, data)
    else
        table.insert(self.queue, data)
    end
end
-- }}}

-- {{{ samestate:send_request()
function samestate:send_request(req)
    local transaction = samestate_transaction.new(req)

    send_or_queue_transaction(self, transaction)
    return transaction
end
-- }}}

return samestate

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
