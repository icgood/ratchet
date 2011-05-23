
-- Create module, but keep global namespace.
local globals = _G
module("ratchet.http.client")
local http_client = globals.getfenv()
globals.setfenv(1, globals)

http_client.__index = http_client

-- {{{ http_client.new()
function http_client.new(socket, send_size)
    local self = {}
    setmetatable(self, http_client)

    self.socket = socket

    -- socket(7) option SO_SNDBUF returns double the desired value.
    self.send_size = send_size or (socket.SO_SNDBUF / 2)

    return self
end
-- }}}

-- {{{ http_client:build_header_string()
function http_client:build_header_string(headers)
    local ret = ""
    for name, value in pairs(headers) do
        for i, each in ipairs(value) do
            ret = ret .. name .. ": " .. tostring(each) .. "\r\n"
        end
    end
    return ret
end
-- }}}

-- {{{ http_client:parse_header_string()
function http_client:parse_header_string(data, start)
    local headers = {}
    repeat
        local name, value
        name, value, start = data:match("^(.-):%s+(.-)\r\n()", start)
        if name then
            local key = name:lower()
            if not headers[key] then
                headers[key] = {value}
            else
                table.insert(headers[key], value)
            end
        end
    until not name
    return headers, start
end
-- }}}

-- {{{ http_client:build_request_and_headers()
function http_client:build_request_and_headers(command, uri, headers)
    local ret = command:upper() .. " " .. uri .. " HTTP/1.0\r\n"
    if headers and #headers then
        ret = ret .. self:build_header_string(headers)
    end
    ret = ret .. "\r\n"
    return ret
end
-- }}}

-- {{{ http_client:slow_send()
function http_client:slow_send(socket, request, data)
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

-- {{{ http_client:send_request()
function http_client:send_request(socket, command, uri, headers, data)
    local request = self:build_request_and_headers(command, uri, headers, data)
    self:slow_send(socket, request, data)
    socket:shutdown("write")
end
-- }}}

-- {{{ http_client:parse_response()
function http_client:parse_response(socket)
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

    headers, lineend = self:parse_header_string(full_reply, lineend)

    lineend = full_reply:match("\r\n\r\n()", lineend)
    if lineend then
        data = full_reply:sub(lineend)
    end

    return tonumber(code), reason, headers, data
end
-- }}}

-- {{{ http_client:query()
function http_client:query(command, uri, headers, data)
    self:send_request(self.socket, command, uri, headers, data)
    return self:parse_response(self.socket)
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
