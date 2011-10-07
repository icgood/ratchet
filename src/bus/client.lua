
require "ratchet"
require "ratchet.socketpad"
require "ratchet.bus.client_transaction"

module("ratchet.bus.client", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(socket, request_to_bus, response_from_bus)
    local self = {}
    setmetatable(self, class)

    self.socket = socket
    self.request_to_bus = request_to_bus or tostring
    self.response_from_bus = response_from_bus or tostring

    return self
end
-- }}}

-- {{{ send_request()
function send_request(self, request)
    local request_data = self.request_to_bus(request)
    local request_size = ratchet.socket.hton(#request_data)

    local pad = ratchet.socketpad.new(self.socket)
    pad:send(request_size, true)
    pad:send(request_data)

    local transaction = ratchet.bus.client_transaction.new(request, self.response_from_bus, pad)
    return transaction
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
