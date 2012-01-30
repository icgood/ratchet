require "ratchet"
require "ratchet.http.client"
require "ratchet.http.server"

debugging = false

-- {{{ debug_print()
local function debug_print(t, data)
    if t == "send" or t == "recv" then
        print(t .. ": [" .. data .. "]")
    else
        print(t)
    end
end
-- }}}

-- {{{ server_handle_command()
function server_handle_command(self, uri, headers, data, from)
    assert(uri == "/test/uri")
    assert(data == "command data")
    local header_values = headers["x-command-header"]
    assert(#header_values == 2)
    assert(header_values[1] == "command header value 1")
    assert(header_values[2] == "command header value 2")

    server_received_command = true
    return {
        code = 250,
        message = "Testing",
        headers = {
            ["X-Testing"] = {"check"},
        },
        data = "data!",
    }
end
-- }}}

-- {{{ client_handle_response()
function client_handle_response(code, reason, headers, data)
    assert(code == 250)
    assert(reason == "Testing")
    assert(data == "data!")
    local header_values = headers["x-testing"]
    assert(#header_values == 1)
    assert(header_values[1] == "check")
    local missing_header_values = headers['x-missing']
    assert(type(missing_header_values) == "table")
    assert(#missing_header_values == 0)

    client_received_response = true
end
-- }}}

-- {{{ server_ctx()
function server_ctx(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port, "AF_INET")
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if debugging then socket:set_tracer(debug_print) end
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(client_ctx, host, port)

    local client, from = socket:accept()
    if debugging then client:set_tracer(debug_print) end

    local server_handlers = {
        GET = server_handle_command,
    }
    local handler = ratchet.http.server.new(client, from, server_handlers)
    handler:handle()
end
-- }}}

-- {{{ client_ctx()
function client_ctx(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port, "AF_INET")
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if debugging then socket:set_tracer(debug_print) end
    socket:connect(rec.addr)

    local handler = ratchet.http.client.new(socket)
    local headers = {
        ["X-Command-Header"] = {
            "command header value 1",
            "command header value 2",
        },
        ["Content-Length"] = {12},
    }
    client_handle_response(handler:query("GET", "/test/uri", headers, "command data"))
end
-- }}}

kernel = ratchet.new(function ()
    ratchet.thread.attach(server_ctx, "localhost", 10080)
end)
kernel:loop()

assert(server_received_command)
assert(client_received_response)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
