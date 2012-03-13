
require "ratchet"

local data_sender = require "ratchet.smtp.data_sender"
local smtp_io = require "ratchet.smtp.smtp_io"
local smtp_extensions = require "ratchet.smtp.smtp_extensions"
local smtp_reply = require "ratchet.smtp.smtp_reply"

ratchet.smtp = ratchet.smtp or {}
ratchet.smtp.client = {}
ratchet.smtp.client.__index = ratchet.smtp.client

-- {{{ratchet.smtp.client.new()
function ratchet.smtp.client.new(socket, iter_size)
    local self = {}
    setmetatable(self, ratchet.smtp.client)

    self.io = smtp_io.new(socket)
    self.iter_size = iter_size
    self.extensions = smtp_extensions.new()

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

-- {{{ratchet.smtp.client:get_banner()
function ratchet.smtp.client:get_banner()
    local banner = smtp_reply.new("[BANNER]")
    table.insert(self.recv_queue, banner)

    recv_batch(self)

    return banner
end
-- }}}

-- {{{ratchet.smtp.client:ehlo()
function ratchet.smtp.client:ehlo(ehlo_as, flush)
    local ehlo = smtp_reply.new("EHLO")
    table.insert(self.recv_queue, ehlo)

    local command = "EHLO " .. ehlo_as
    self.io:send_command(command)

    recv_batch(self)
    if ehlo.code == "250" then
        self.extensions:reset()
        ehlo.message = self.extensions:parse_string(ehlo.message)
    end

    if flush then
        recv_batch(self)
    end

    return ehlo
end
-- }}}

-- {{{ratchet.smtp.client:helo()
function ratchet.smtp.client:helo(helo_as)
    local ehlo = smtp_reply.new("HELO")
    table.insert(self.recv_queue, ehlo)

    local command = "HELO " .. helo_as
    self.io:send_command(command)

    recv_batch(self)

    return ehlo
end
-- }}}

-- {{{ratchet.smtp.client:starttls()
function ratchet.smtp.client:starttls()
    local starttls = smtp_reply.new("STARTTLS")
    table.insert(self.recv_queue, starttls)

    local command = "STARTTLS"
    self.io:send_command(command)

    recv_batch(self)

    return starttls
end
-- }}}

-- {{{ratchet.smtp.client:mailfrom()
function ratchet.smtp.client:mailfrom(address, data_size)
    local mailfrom = smtp_reply.new("MAIL FROM")
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

-- {{{ratchet.smtp.client:rcptto()
function ratchet.smtp.client:rcptto(address)
    local rcptto = smtp_reply.new("RCPT TO")
    table.insert(self.recv_queue, rcptto)

    local command = "RCPT TO:<"..address..">"
    self.io:send_command(command)

    if not self.extensions:has("PIPELINING") then
        recv_batch(self)
    end

    return rcptto
end
-- }}}

-- {{{ratchet.smtp.client:data()
function ratchet.smtp.client:data()
    local data = smtp_reply.new("DATA")
    table.insert(self.recv_queue, data)

    local command = "DATA"
    self.io:send_command(command)

    recv_batch(self)

    return data
end
-- }}}

-- {{{ratchet.smtp.client:send_data()
function ratchet.smtp.client:send_data(data)
    local send_data = smtp_reply.new("[MESSAGE CONTENTS]")
    table.insert(self.recv_queue, send_data)

    local data_sender = data_sender.new(data, self.iter_size)
    data_sender:send(self.io)

    recv_batch(self)

    return send_data
end
-- }}}

-- {{{ratchet.smtp.client:send_empty_data()
function ratchet.smtp.client:send_empty_data()
    local send_data = smtp_reply.new("[MESSAGE CONTENTS]")
    table.insert(self.recv_queue, send_data)

    self.io:send_command(".")

    recv_batch(self)

    return send_data
end
-- }}}

-- {{{ratchet.smtp.client:custom_command()
function ratchet.smtp.client:custom_command(command, arg)
    local custom = smtp_reply.new(command:upper())
    table.insert(self.recv_queue, custom)

    if arg then
        command = command .. " " .. arg
    end
    self.io:send_command(command)

    recv_batch(self)

    return custom
end
-- }}}

-- {{{ratchet.smtp.client:rset()
function ratchet.smtp.client:rset()
    local rset = smtp_reply.new("RSET")
    table.insert(self.recv_queue, rset)

    local command = "RSET"
    self.io:send_command(command)

    recv_batch(self)

    return rset
end
-- }}}

-- {{{ratchet.smtp.client:quit()
function ratchet.smtp.client:quit()
    local quit = smtp_reply.new("QUIT")
    table.insert(self.recv_queue, quit)

    local command = "QUIT"
    self.io:send_command(command)

    recv_batch(self)
    self.io:close()

    return quit
end
-- }}}

-- {{{ratchet.smtp.client:close()
function ratchet.smtp.client:close()
    self.io:close()
end
-- }}}

return ratchet.smtp.client

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
