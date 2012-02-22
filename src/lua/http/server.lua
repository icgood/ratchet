
require "ratchet"
local common = require "ratchet.http.common"

ratchet.http = ratchet.http or {}
ratchet.http.server = {}
ratchet.http.server.__index = ratchet.http.server

-- {{{ ratchet.http.server.new()
function ratchet.http.server.new(socket, from, handlers, send_size)
    local self = {}
    setmetatable(self, ratchet.http.server)

    self.socket = socket
    self.from = from
    self.handlers = handlers

    return self
end
-- }}}

-- {{{ build_response_and_headers()
local function build_response_and_headers(response)
    local ret = "HTTP/1.0 " .. response.code .. " " .. response.message .. "\r\n"
    if response.headers and #response.headers then
        ret = ret .. common.build_header_string(response.headers)
    end
    ret = ret .. "\r\n"
    return ret
end
-- }}}

-- {{{ send_response()
local function send_response(self, response)
    local response_str = build_response_and_headers(response)
    local remaining = response_str .. response.data
    repeat
        remaining = self.socket:send(remaining)
    until not remaining
    self.socket:shutdown("both")
    self.socket:close()
end
-- }}}

-- {{{ parse_request_so_far()
local function parse_request_so_far(so_far, unparsed_i, request)
    local i

    if not request.command or not request.uri then
        local cmd_pattern = "^(.-)%s+(.-)%s+[hH][tT][tT][pP]%/([%d%.]+)\r\n()"
        request.command, request.uri, request.http_ver, i = so_far:match(cmd_pattern, unparsed_i)
        if i then
            unparsed_i = i
        else
            if so_far:match("^.-\r\n", unparsed_i) then
                error("Malformed HTTP session.")
            end
            return false, unparsed_i
        end
    end

    if not request.headers then
        local hdr_pattern = "^(.-\r\n)\r\n()"
        local hdrs, i = so_far:match(hdr_pattern, unparsed_i)
        if i then
            request.headers = common.parse_header_string(hdrs)
            unparsed_i = i
        else
            hdr_pattern = "^\r\n()"
            i = so_far:match(hdr_pattern, unparsed_i)
            if i then
                request.headers = {}
                unparsed_i = i
            else
                return false, unparsed_i
            end
        end
    end

    if not request.data then
        if not request.headers['content-length'] then
            return true
        end
        local content_len = tonumber(request.headers['content-length'][1])
        if not content_len then
            return true
        end

        local recved_len = #so_far - unparsed_i + 1
        if recved_len >= content_len then
            request.data = so_far:sub(unparsed_i, unparsed_i+content_len-1)
            return true
        else
            return false, unparsed_i
        end
    end
end
-- }}}

-- {{{ get_request()
local function get_request(socket)
    local request = {}
    local so_far = ""
    local unparsed_i = 1
    local done = false

    while not done do
        local data = socket:recv()
        so_far = so_far .. data
        done, unparsed_i = parse_request_so_far(so_far, unparsed_i, request)
        if data == "" then
            break
        end
    end
    socket:shutdown("read")

    --return request.command, request.uri, request.headers, request.data
    return request
end
-- }}}

-- {{{ ratchet.http.server:handle()
function ratchet.http.server:handle()
    local req = get_request(self.socket)

    local cmd_handler
    if req.command then
        cmd_handler = self.handlers[req.command:upper()]
    end
    local response = {code = 501, message = "Not Implemented"}
    if req.http_ver ~= "1.0" then
        response = {code = 505, message = "Version Not Supported"}
    end
    if cmd_handler then
        response = cmd_handler(self.handlers, req.uri, req.headers, req.data, self.from)
    end

    send_response(self, response)
end
-- }}}

ratchet.http.server.__call = ratchet.http.server.handle

return ratchet.http.server

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
