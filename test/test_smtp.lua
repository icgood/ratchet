require "ratchet"
require "ratchet.smtp.client"
require "ratchet.smtp.server"

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

-- {{{ server_handlers

server_handlers = {}

function server_handlers:BANNER(reply)
    reply.code = "220"
    reply.message = "test banner"
end

function server_handlers:EHLO(reply, ehlo_as)
    reply.code = "250"
    reply.message = "hello " .. ehlo_as
end

function server_handlers:MAIL(reply, address)
    reply.code = "250"
    reply.message = "test sender okay: " .. address
end

function server_handlers:RCPT(reply, address)
    reply.code = "250"
    reply.message = "test recipient okay: " .. address
end

function server_handlers:DATA(reply)
    reply.code = "354"
    reply.message = "ready for test data"
end

function server_handlers:HAVE_DATA(reply, data)
    reply.code = "250"
    reply.message = "test data received"
end

function server_handlers:SLIMTA(reply, arg)
    reply.code = "123"
    reply.message = "custom command okay"

    server_received_message = true
end

function server_handlers:QUIT(reply)
    reply.code = "221"
    reply.message = "test connection quit"
end

-- }}}

-- {{{ client_handler()
function client_handler(client)
    local banner = client:get_banner()
    assert(banner.code == "220" and banner.message == "test banner")

    local ehlo = client:ehlo("test")
    assert(ehlo.code == "250")

    local mailfrom = client:mailfrom("sender@slimta.org")
    local rcptto1 = client:rcptto("rcpt1@slimta.org")
    local rcptto2 = client:rcptto("rcpt2@slimta.org")
    local data = client:data()

    assert(mailfrom.code == "250" and mailfrom.message == "2.1.0 test sender okay: sender@slimta.org")
    assert(rcptto1.code == "250" and rcptto1.message == "2.1.5 test recipient okay: rcpt1@slimta.org")
    assert(rcptto2.code == "250" and rcptto2.message == "2.1.5 test recipient okay: rcpt2@slimta.org")
    assert(data.code == "354" and data.message == "ready for test data")

    local send_data = client:send_data("arbitrary test message data")
    assert(send_data.code == "250" and send_data.message == "2.6.0 test data received")

    local custom = client:custom_command("SLIMTA", "test")
    assert(custom.code == "123" and custom.message == "custom command okay")

    local quit = client:quit()
    assert(quit.code == "221" and quit.message == "2.0.0 test connection quit")

    client_sent_message = true
end
-- }}}

-- {{{ server_ctx()
function server_ctx(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port, "AF_INET")
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if debugging then socket:set_tracer(debug_print) end
    socket:setsockopt("SO_REUSEADDR", true)
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(client_ctx, host, port)

    local client, from = socket:accept()
    if debugging then client:set_tracer(debug_print) end

    local handler = ratchet.smtp.server.new(client, server_handlers)
    handler:handle()
end
-- }}}

-- {{{ client_ctx()
function client_ctx(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port, "AF_INET")
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    if debugging then socket:set_tracer(debug_print) end
    socket:connect(rec.addr)

    local handler = ratchet.smtp.client.new(socket)
    client_handler(handler)
end
-- }}}

kernel = ratchet.new(function ()
    ratchet.thread.attach(server_ctx, "localhost", 10025)
end)
kernel:loop()

assert(server_received_message)
assert(client_sent_message)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
