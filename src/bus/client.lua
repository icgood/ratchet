
require "ratchet"
require "ratchet.socketpad"
local client_transaction = require "ratchet.bus.client_transaction"

local client = {}
client.__index = client

-- {{{ client.new()
function client.new(socket, request_to_bus, response_from_bus, socket_from)
    local self = {}
    setmetatable(self, client)

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

-- {{{ client:send_request()
function client:send_request(request)
    local part_1, attachments = self.request_to_bus(request)

    local num_parts = 1 + (attachments and #attachments or 0)
    self.socket_buffer:send(ratchet.socket.hton16(num_parts), true)

    send_part(self.socket_buffer, part_1)
    if attachments then
        for i = 1, #attachments do
            send_part(self.socket_buffer, attachments[i])
        end
    end

    return client_transaction.new(request, self.response_from_bus, self.socket_buffer)
end
-- }}}

return client

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
