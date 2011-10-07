
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
    self.request_from_bus = request_from_bus
    self.response_to_bus = response_to_bus

    return self
end
-- }}}

-- {{{ recv_request()
function recv_request(self)
    local client, from = self.socket:accept()

    local pad = ratchet.socketpad.new(client)
    local size = ratchet.socket.ntoh(pad:recv(4))
    local data = pad:recv(size)

    local request = self.request_from_bus(data)

    local transaction = ratchet.bus.server_transaction.new(request, self.response_to_bus, pad, from)
    return transaction, request
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
