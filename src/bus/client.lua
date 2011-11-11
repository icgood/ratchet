
require "ratchet"
require "ratchet.socketpad"
require "ratchet.bus.client_transaction"

module("ratchet.bus.client", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(socket, request_to_bus, response_from_bus, socket_from)
    local self = {}
    setmetatable(self, class)

    self.socket_buffer = ratchet.socketpad.new(socket, socket_from)
    self.request_to_bus = request_to_bus or tostring
    self.response_from_bus = response_from_bus or tostring

    return self
end
-- }}}

-- {{{ send_part()
local function send_part(pad, part)
    local part_size = ratchet.socket.hton(#part)

    pad:send(part_size, true)
    pad:send(part)
end
-- }}}

-- {{{ send_request()
function send_request(self, request)
    local attachments = {}
    local part_1 = self.request_to_bus(request, attachments)

    local num_parts = 1 + #attachments
    self.socket_buffer:send(ratchet.socket.hton16(num_parts), true)

    send_part(self.socket_buffer, part_1)
    if attachments then
        for i = 1, #attachments do
            send_part(self.socket_buffer, attachments[i])
        end
    end

    return ratchet.bus.client_transaction.new(request, self.response_from_bus, self.socket_buffer)
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
