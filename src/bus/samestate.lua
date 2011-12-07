
require "ratchet"
require "ratchet.bus.samestate_transaction"

ratchet.bus.samestate = {}
ratchet.bus.samestate.__index = ratchet.bus.samestate

-- {{{ ratchet.bus.samestate.new()
function ratchet.bus.samestate.new()
    local self = {}
    setmetatable(self, ratchet.bus.samestate)

    self.queue = {}

    return self, self
end
-- }}}

-- {{{ wait_for_transaction()
local function wait_for_transaction(self)
    if not self.queue[1] then
        self.waiting_thread = ratchet.running_thread()
        local ret = ratchet.pause()
        self.waiting_thread = nil
        return ret
    else
        return table.remove(self.queue, 1)
    end
end
-- }}}

-- {{{ ratchet.bus.samestate:recv_request()
function ratchet.bus.samestate:recv_request()
    local transaction = wait_for_transaction(self)
    return transaction, transaction and transaction.request
end
-- }}}

-- {{{ send_or_queue_transaction()
local function send_or_queue_transaction(self, data)
    if self.waiting_thread then
        ratchet.unpause(self.waiting_thread, data)
    else
        table.insert(self.queue, data)
    end
end
-- }}}

-- {{{ ratchet.bus.samestate:send_request()
function ratchet.bus.samestate:send_request(req)
    local transaction = ratchet.bus.samestate_transaction.new(req)

    send_or_queue_transaction(self, transaction)
    return transaction
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
