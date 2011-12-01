
require "ratchet"

ratchet.bus.client_transaction = {}
ratchet.bus.client_transaction.__index = ratchet.bus.client_transaction

-- {{{ ratchet.bus.client_transaction.new()
function ratchet.bus.client_transaction.new(request, response_from_bus, socket_buffer)
    local self = {}
    setmetatable(self, ratchet.bus.client_transaction)

    self.request = request
    self.socket_buffer = socket_buffer
    self.response_from_bus = response_from_bus

    return self
end
-- }}}

-- {{{ recv_part()
local function recv_part(pad, parts, i)
    local size, incomplete = ratchet.socket.ntoh(pad:recv(4))
    if incomplete then
        pad:close()
        return
    end

    local data, incomplete = pad:recv(size)
    if incomplete then
        pad:close()
        return
    end

    parts[i] = data
end
-- }}}

-- {{{ ratchet.bus.client_transaction:recv_response()
function ratchet.bus.client_transaction:recv_response()
    local pad = self.socket_buffer
    local num_parts, incomplete = ratchet.socket.ntoh16(pad:recv(2))
    if incomplete then
        pad:close()
        return
    end

    local parts = {}
    for i = 1, num_parts do
        recv_part(pad, parts, i)
    end
    pad:close()

    return self.response_from_bus(table.remove(parts, 1), parts, pad.from)
end
-- }}}

return ratchet.bus.client_transaction

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
