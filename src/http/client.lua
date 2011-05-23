
require "package"
local common = require "ratchet.http.common"

module("ratchet.http.client", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(socket, send_size)
    local self = {}
    setmetatable(self, class)

    self.socket = socket

    -- socket(7) option SO_SNDBUF returns double the desired value.
    self.send_size = send_size or (socket.SO_SNDBUF / 2)

    return self
end
-- }}}

-- {{{ build_request_and_headers()
function build_request_and_headers(self, command, uri, headers)
    local ret = command:upper() .. " " .. uri .. " HTTP/1.0\r\n"
    if headers and #headers then
        ret = ret .. common.build_header_string(headers)
    end
    ret = ret .. "\r\n"
    return ret
end
-- }}}

-- {{{ slow_send()
function slow_send(self, socket, request, data)
    -- The purpose of this function is to avoid concatenating with data.

    while #request > self.send_size do
        local to_send = request:sub(1, self.send_size)
        socket:send(to_send)
        request = request:sub(self.send_size+1)
    end
    if not data then
        socket:send(request)
    else
        local to_send = request .. data:sub(1, self.send_size - #request)
        socket:send(to_send)
        data = data:sub(self.send_size - #request + 1)
        repeat
            to_send = data:sub(1, self.send_size)
            socket:send(to_send)
            data = data:sub(self.send_size+1)
        until data == ""
    end
end
-- }}}

-- {{{ send_request()
function send_request(self, socket, command, uri, headers, data)
    local request = self:build_request_and_headers(command, uri, headers, data)
    self:slow_send(socket, request, data)
    socket:shutdown("write")
end
-- }}}

-- {{{ parse_response()
function parse_response(self, socket)
    local full_reply = ""
    repeat
        local data = socket:recv()
        if #data > 0 then
            full_reply = full_reply .. data
        end
    until data == ""

    socket:close()

    local code, reason, lineend = full_reply:match("^HTTP%/%d%.%d (%d%d%d) (.-)\r\n()")
    local headers, data

    if not code then
        return
    end

    headers, lineend = common.parse_header_string(full_reply, lineend)

    lineend = full_reply:match("\r\n\r\n()", lineend)
    if lineend then
        data = full_reply:sub(lineend)
    end

    return tonumber(code), reason, headers, data
end
-- }}}

-- {{{ query()
function query(self, command, uri, headers, data)
    self:send_request(self.socket, command, uri, headers, data)
    return self:parse_response(self.socket)
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
