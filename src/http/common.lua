
require "package"

module("ratchet.http.common", package.seeall)

-- {{{ build_header_string()
function build_header_string(headers)
    local ret = ""
    for name, value in pairs(headers) do
        for i, each in ipairs(value) do
            ret = ret .. name .. ": " .. tostring(each) .. "\r\n"
        end
    end
    return ret
end
-- }}}

-- {{{ parse_header_string()
function parse_header_string(data, start)
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

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
