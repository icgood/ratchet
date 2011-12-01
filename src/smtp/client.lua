
require "package"
local common = require "ratchet.smtp.common"

module("ratchet.smtp.client", package.seeall)
local class = getfenv()
__index = class

-- {{{ new()
function new(socket, send_size, iter_size)
    local self = {}
    setmetatable(self, class)

    self.io = common.smtp_io.new(socket, send_size)
    self.iter_size = iter_size
    self.extensions = common.smtp_extensions.new()

    self.recv_queue = {}

    return self
end
-- }}}

-- {{{ recv_batch()
local function recv_batch(self)
    self.io:flush_send()

    repeat
        local reply = table.remove(self.recv_queue, 1)
        if reply then
            reply:recv(self.io)
        end
    until not reply
end
-- }}}

-- {{{ get_banner()
function get_banner(self)
    local banner = common.smtp_reply.new("[BANNER]")
    table.insert(self.recv_queue, banner)

    recv_batch(self)

    return banner
end
-- }}}

-- {{{ ehlo()
function ehlo(self, ehlo_as)
    local ehlo = common.smtp_reply.new("EHLO")
    table.insert(self.recv_queue, ehlo)

    local command = "EHLO " .. ehlo_as
    self.io:send_command(command)

    recv_batch(self)
    if ehlo.code == "250" then
        self.extensions:reset()
        ehlo.message = self.extensions:parse_string(ehlo.message)
    end

    return ehlo
end
-- }}}

-- {{{ helo()
function helo(self, helo_as)
    local ehlo = common.smtp_reply.new("HELO")
    table.insert(self.recv_queue, ehlo)

    local command = "HELO " .. helo_as
    self.io:send_command(command)

    recv_batch(self)

    return ehlo
end
-- }}}

-- {{{ starttls()
function starttls(self)
    local starttls = common.smtp_reply.new("STARTTLS")
    table.insert(self.recv_queue, starttls)

    local command = "STARTTLS"
    self.io:send_command(command)

    recv_batch(self)

    return starttls
end
-- }}}

-- {{{ mailfrom()
function mailfrom(self, address, data_size)
    local mailfrom = common.smtp_reply.new("MAIL FROM")
    table.insert(self.recv_queue, mailfrom)

    local command = "MAIL FROM:<"..address..">"
    if data_size and self.extensions:has("SIZE") then
        command = command .. " SIZE=" .. data_size
    end
    self.io:send_command(command)

    if not self.extensions:has("PIPELINING") then
        recv_batch(self)
    end

    return mailfrom
end
-- }}}

-- {{{ rcptto()
function rcptto(self, address)
    local rcptto = common.smtp_reply.new("RCPT TO")
    table.insert(self.recv_queue, rcptto)

    local command = "RCPT TO:<"..address..">"
    self.io:send_command(command)

    if not self.extensions:has("PIPELINING") then
        recv_batch(self)
    end

    return rcptto
end
-- }}}

-- {{{ data()
function data(self)
    local data = common.smtp_reply.new("DATA")
    table.insert(self.recv_queue, data)

    local command = "DATA"
    self.io:send_command(command)

    recv_batch(self)

    return data
end
-- }}}

-- {{{ send_data()
function send_data(self, data)
    local send_data = common.smtp_reply.new("[MESSAGE CONTENTS]")
    table.insert(self.recv_queue, send_data)

    local data_sender = common.data_sender.new(data, self.iter_size)
    data_sender:send(self.io)

    recv_batch(self)

    return send_data
end
-- }}}

-- {{{ send_empty_data()
function send_empty_data(self)
    local send_data = common.smtp_reply.new("[MESSAGE CONTENTS]")
    table.insert(self.recv_queue, send_data)

    self.io:send_command(".")

    recv_batch(self)

    return send_data
end
-- }}}

-- {{{ custom_command()
function custom_command(self, command, arg)
    local custom = common.smtp_reply.new(command:upper())
    table.insert(self.recv_queue, custom)

    if arg then
        command = command .. " " .. arg
    end
    self.io:send_command(command)

    recv_batch(self)

    return custom
end
-- }}}

-- {{{ rset()
function rset(self)
    local rset = common.smtp_reply.new("RSET")
    table.insert(self.recv_queue, rset)

    local command = "RSET"
    self.io:send_command(command)

    recv_batch(self)

    return rset
end
-- }}}

-- {{{ quit()
function quit(self)
    local quit = common.smtp_reply.new("QUIT")
    table.insert(self.recv_queue, quit)

    local command = "QUIT"
    self.io:send_command(command)

    recv_batch(self)
    self.io:close()

    return quit
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
