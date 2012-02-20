
require "ratchet"

ratchet.socketpad = {}
ratchet.socketpad.__index = ratchet.socketpad

-- {{{ ratchet.socketpad.new()
function ratchet.socketpad.new(socket, from)
    local self = {}
    setmetatable(self, ratchet.socketpad)

    self.socket = socket
    self.from = from
    self.data = {}

    self.recv_buffer = ""
    self.send_buffer = ""

    return self
end
-- }}}

-- {{{ recv_once()
local function recv_once(self)
    local data = self.socket:recv()

    self.recv_buffer = self.recv_buffer .. data
    return data
end
-- }}}

-- {{{ recv_until_bytes()
local function recv_until_bytes(self, bytes)
    local incomplete = nil
    while #self.recv_buffer < bytes do
        local data = recv_once(self)
        if data == '' then
            incomplete = true
            break
        end
    end

    local ret = self.recv_buffer:sub(1, bytes)
    self.recv_buffer = self.recv_buffer:sub(bytes+1)
    return ret, incomplete
end
-- }}}

-- {{{ recv_until_string()
local function recv_until_string(self, str)
    local start_i, end_i, incomplete
    while true do
        start_i, end_i = self.recv_buffer:find(str, 1, true)
        if end_i then
            end_i = end_i
            break
        end

        local data = recv_once(self)
        if data == "" then
            incomplete = true
            end_i = #self.recv_buffer
            break
        end
    end

    local ret = self.recv_buffer:sub(1, end_i)
    self.recv_buffer = self.recv_buffer:sub(end_i+1)
    return ret, incomplete
end
-- }}}

-- {{{ ratchet.socketpad:recv()
function ratchet.socketpad:recv(through)
    if type(through) == "string" then
        return recv_until_string(self, through)
    elseif type(through) == "number" then
        return recv_until_bytes(self, through)
    else
        return nil, "Argument must be a number or string."
    end
end
-- }}}

-- {{{ ratchet.socketpad:recv_remaining()
function ratchet.socketpad:recv_remaining()
    local ret = self.recv_buffer
    self.recv_buffer = ''

    return ret
end
-- }}}

-- {{{ ratchet.socketpad:peek()
function ratchet.socketpad:peek()
    return self.recv_buffer
end
-- }}}

-- {{{ ratchet.socketpad:update_and_peek()
function ratchet.socketpad:update_and_peek()
    local data, err = recv_once(self)
    if not data then
        return nil, err
    elseif data == '' then
        return self.recv_buffer, true
    end

    return self.recv_buffer
end
-- }}}

-- {{{ ratchet.socketpad:send_behavior()
function ratchet.socketpad:send_behavior(what, how)
    if what == "chunk_size" then
        self.chunk_size = tonumber(how)
    elseif what == "always_flush" then
        if type(how) == "nil" then
            self.always_flush = true
        else
            self.always_flush = how
        end
    else
        error("Unknown behavior type: " .. tostring(what))
    end
end
-- }}}

-- {{{ update_send_buffer()
local function update_send_buffer(self, data)
    if self.chunk_size and #self.send_buffer + #data > self.chunk_size then
        while data ~= '' do
            local extra = self.chunk_size - #self.send_buffer
            local for_now = data:sub(1, extra)
            data = data:sub(extra+1)
            self.send_buffer = self.send_buffer .. for_now
            self:flush()
        end
    else
        self.send_buffer = self.send_buffer .. data
    end
end
-- }}}

-- {{{ ratchet.socketpad:send()
function ratchet.socketpad:send(data, more)
    if more and (not data or data == '') then
        self:flush()
        return
    end

    update_send_buffer(self, data)

    if not more or self.always_flush then
        self:flush()
    end
end
-- }}}

-- {{{ ratchet.socketpad:flush()
function ratchet.socketpad:flush()
    local to_send = self.send_buffer
    self.send_buffer = ''

    repeat
        to_send = self.socket:send(to_send)
    until not to_send
end
-- }}}

-- {{{ ratchet.socketpad:close()
function ratchet.socketpad:close()
    self.socket:close()
end
-- }}}

return ratchet.socketpad

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
