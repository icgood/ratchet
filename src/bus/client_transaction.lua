
require "ratchet"

module("ratchet.bus.client_transaction", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(request, response_from_bus, socket_buffer)
    local self = {}
    setmetatable(self, class)

    self.request = request
    self.socket_buffer = socket_buffer
    self.response_from_bus = response_from_bus

    return self
end
-- }}}

-- {{{ recv_response()
function recv_response(self)
    -- Receive the size "header" of the response transmission.
    local size, incomplete = ratchet.socket.ntoh(self.socket_buffer:recv(4))
    if incomplete then
        self.socket_buffer:close()
        return
    end

    -- Receive the actual content of the response transmission.
    local data, incomplete = self.socket_buffer:recv(size)
    if incomplete then
        self.socket_buffer:close()
        return
    end

    local response = self.response_from_bus(data)

    self.socket_buffer:close()

    return response
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
