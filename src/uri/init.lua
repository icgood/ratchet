
require "ratchet"

ratchet.uri = {}
local parse_handlers = {}

-- {{{ parse_handlers

-- {{{ parse_handlers.tcp()
function parse_handlers.tcp(data, command, dns_types)
    local parsers = {
        "^%/*%[(.-)%]%:(%d+)$",
        "^%/*([^%:]*)%:(%d+)$",
    }

    local host, port
    for i, pattern in ipairs(parsers) do
        host, port = data:match(pattern)
        if host then
            break
        end
    end

    local rec = ratchet.socket.prepare_tcp(host, port, dns_types)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if command == "connect" then
        socket:connect(rec.addr)
    elseif command == "listen" then
        socket.SO_REUSEADDR = true
        socket:bind(rec.addr)
        socket:listen()
    end

    return socket
end
-- }}}

-- {{{ parse_handlers.udp()
function parse_handlers.udp(data, command, dns_types)
    local parsers = {
        "^%/*%[(.-)%]%:(%d+)$",
        "^%/*([^%:]*)%:(%d+)$",
    }

    local host, port
    for i, pattern in ipairs(parsers) do
        host, port = data:match(pattern)
        if host then
            break
        end
    end

    local rec = ratchet.socket.prepare_udp(host, port, dns_types)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if command == "connect" then
        socket:connect(rec.addr)
    elseif command == "listen" then
        socket.SO_REUSEADDR = true
        socket:bind(rec.addr)
        socket:listen()
    end

    return socket
end
-- }}}

-- {{{ parse_handlers.unix()
function parse_handlers.unix(file, command)
    os.remove(file)
    local rec = ratchet.socket.prepare_unix(file)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if command == "connect" then
        socket:connect(rec.addr)
    elseif command == "listen" then
        socket:bind(rec.addr)
        socket:listen()
    end

    return socket
end
-- }}}

-- }}}

-- {{{ ratchet.uri.register()
function ratchet.uri.register(schema, handler)
    if not schema:match("^%w[%w%-]$") then
        error("Schema name invalid, alphanumerics and \"-\" only: " .. schema)
    end

    parse_handlers[schema] = handler
end
-- }}}

-- {{{ ratchet.uri.parse()
function ratchet.uri.parse(str, ...)
    local schema, data = str:match("^([%w%-]+)%:(.*)$")
    if not schema then
        error("Could not parse URI schema: " .. str)
    end

    local handler = parse_handlers[schema:lower()]
    if not handler then
        error("Unknown schema: " .. schema)
    end

    return handler(data, ...)
end
-- }}}

return ratchet.uri

-- vim:et:fdm=marker:sts=4:sw=4:ts=4
