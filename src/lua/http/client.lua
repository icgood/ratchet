
require "ratchet"
local common = require "ratchet.http.common"

ratchet.http = ratchet.http or {}
ratchet.http.client = {}
ratchet.http.client.__index = ratchet.http.client

-- {{{ ratchet.http.client.new()
function ratchet.http.client.new(socket)
    local self = {}
    setmetatable(self, ratchet.http.client)

    self.socket = socket

    return self
end
-- }}}

-- {{{ build_request_and_headers()
local function build_request_and_headers(command, uri, headers)
    local ret = command:upper() .. " " .. uri .. " HTTP/1.0\r\n"
    if headers and #headers then
        ret = ret .. common.build_header_string(headers)
    end
    ret = ret .. "\r\n"
    return ret
end
-- }}}

-- {{{ send_request()
local function send_request(self, command, uri, headers, data)
    local request = build_request_and_headers(command, uri, headers, data)
    local remaining = request .. data
    repeat
        remaining = self.socket:send(remaining)
    until not remaining
    self.socket:shutdown("write")
end
-- }}}

-- {{{ parse_response()
local function parse_response(socket)
    local full_reply = ""
    repeat
        local data = socket:recv()
        if #data > 0 then
            full_reply = full_reply .. data
        end
    until data == ""

    local enc = socket:get_encryption()
    if enc then
        enc:shutdown()
    end
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

-- {{{ ratchet.http.client:query()
function ratchet.http.client:query(command, uri, headers, data)
    send_request(self, command, uri, headers, data)
    return parse_response(self.socket)
end
-- }}}

return ratchet.http.client

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
