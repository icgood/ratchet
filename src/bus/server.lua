
require "ratchet"
require "ratchet.socketpad"
local server_transaction = require "ratchet.bus.server_transaction"

local server = {}
server.__index = server

-- {{{ server.new()
function server.new(socket, request_from_bus, response_to_bus)
    local self = {}
    setmetatable(self, server)

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
    local ready, err = ratchet.thread.block_on(build_socket_array(self.sockets))
    if not ready then
        return nil, err
    end

    if ready == self.sockets.server then
        local pad = ratchet.socketpad.new(ready:accept())
        self.sockets[pad.socket] = pad
    else
        local pad = self.sockets[ready]
        local _, closed = pad:update_and_peek()
        if not closed then
            self.updated = pad
        end
    end

    return true
end
-- }}}

-- {{{ pop_num_parts()
local function pop_num_parts(pad)
    local peek = pad:peek()
    if #peek >= 2 then
        pad.data.num_parts = ratchet.socket.ntoh16(pad:recv(2))
        pad.data.parts = {}
        pad.data.part_lens = {}

        return true
    end
end
-- }}}

-- {{{ pop_part_len()
local function pop_part_len(pad, i)
    local peek = pad:peek()
    if #peek >= 4 then
        pad.data.part_lens[i] = ratchet.socket.ntoh(pad:recv(4))
        return true
    end
end
-- }}}

-- {{{ pop_part_data()
local function pop_part_data(pad, i)
    local peek = pad:peek()
    local len = pad.data.part_lens[i]
    if #peek >= len then
        pad.data.parts[i] = pad:recv(len)
        return true
    end
end
-- }}}

-- {{{ pop_full_request()
local function pop_full_request(self, pad)
    if not pad.data.num_parts and not pop_num_parts(pad) then
        return
    end

    if pad.data.num_parts == 0 then
        return self.request_from_bus('', pad.data.parts, pad.from)
    end

    for i = 1, pad.data.num_parts  do
        if not pad.data.part_lens[i] and not pop_part_len(pad, i) then
            return
        end

        if not pad.data.parts[i] and not pop_part_data(pad, i) then
            return
        end
    end

    return self.request_from_bus(table.remove(pad.data.parts, 1), pad.data.parts, pad.from)
end
-- }}}

-- {{{ check_for_full_requests()
local function check_for_full_requests(self)
    if self.updated then
        local request = pop_full_request(self, self.updated)

        if request then
            self.sockets[self.updated.socket] = nil

            local transaction = server_transaction.new(
                request,
                self.response_to_bus,
                self.updated,
                self.updated.from
            )
            return transaction, request
        end
    end
end
-- }}}

-- {{{ server:recv_request()
function server:recv_request()
    local transaction, request

    repeat
        local okay, err = receive_connections_and_data(self)
        if not okay then
            return nil, err
        end

        transaction, request = check_for_full_requests(self)
    until transaction

    return transaction, request
end
-- }}}

return server

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
