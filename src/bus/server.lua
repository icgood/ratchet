
require "ratchet"
require "ratchet.socketpad"
require "ratchet.bus.server_transaction"

module("ratchet.bus.server", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(socket, request_from_bus, response_to_bus)
    local self = {}
    setmetatable(self, class)

    self.socket = socket
    self.request_from_bus = request_from_bus or tostring
    self.response_to_bus = response_to_bus or tostring

    return self
end
-- }}}

-- {{{ recv_request()
function recv_request(self)
    local pad = ratchet.socketpad.new(self.socket)

    -- Receive the size "header" of the transmission.
    local size, incomplete = ratchet.socket.ntoh(pad:recv(4))
    if incomplete then
        client:close()
        return
    end

    -- Receive the actual content of the transmission.
    local data, incomplete = pad:recv(size)
    if incomplete then
        client:close()
        return
    end

    local request = self.request_from_bus(data)

    local transaction = ratchet.bus.server_transaction.new(request, self.response_to_bus, pad)
    return transaction, request
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
