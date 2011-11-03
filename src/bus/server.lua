
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

    self.request_from_bus = request_from_bus or tostring
    self.response_to_bus = response_to_bus or tostring
    self.queue = {}

    self.sockets = {server = socket}

    return self
end
-- }}}

-- {{{ build_socket_array()
local function build_socket_array(sockets)
    local ret = {}
    for k, v in pairs(sockets) do
        if k == 'server' then
            table.insert(ret, v)
        else
            table.insert(ret, k)
        end
    end
    return ret
end
-- }}}

-- {{{ receive_connections_and_data()
local function receive_connections_and_data(self)
    local ready, err = ratchet.block_on(build_socket_array(self.sockets))
    if not ready then
        return nil, err
    end

    if ready == self.sockets.server then
        local pad = ratchet.socketpad.new(ready:accept())
        self.sockets[pad.socket] = pad
    else
        local pad = self.sockets[ready]
        pad:update_and_peek()
        self.updated = pad
    end

    return true
end
-- }}}

-- {{{ pop_full_request()
local function pop_full_request(self, pad)
    local peek = pad:peek()

    if #peek >= 4 then
        local size = ratchet.socket.ntoh(peek:sub(1, 4))
        if #peek >= 4+size then
            pad:recv(4)
            local data = pad:recv(size)

            -- Convert request to object, as per user-provided transformation.
            return self.request_from_bus(data, pad.from)
        end
    end
end
-- }}}

-- {{{ check_for_full_requests()
local function check_for_full_requests(self)
    if self.updated then
        local request = pop_full_request(self, self.updated)

        if request then
            self.sockets[self.updated.socket] = nil

            local transaction = ratchet.bus.server_transaction.new(
                request,
                self.response_to_bus,
                self.updated,
                self.updated.from
            )
            return request, transaction
        end
    end
end
-- }}}

-- {{{ recv_request()
function recv_request(self)
    local request, transaction
    repeat
        local okay, err = receive_connections_and_data(self)
        if not okay then
            return nil, err
        end

        request, transaction = check_for_full_requests(self)
    until request

    return transaction, request
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
