
require "ratchet"

module("ratchet.bus.server_transaction", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(request, response_to_bus, socket_buffer, from)
    local self = {}
    setmetatable(self, class)

    self.request = request
    self.socket_buffer = socket_buffer
    self.response_to_bus = response_to_bus
    self.from = from

    return self
end
-- }}}

-- {{{ send_response()
function send_response(self, response)
    local response_data = self.response_to_bus(response)
    local response_size = ratchet.socket.hton(#response_data)

    self.socket_buffer:send(response_size, true)
    self.socket_buffer:send(response_data)

    self.socket_buffer:close()
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
