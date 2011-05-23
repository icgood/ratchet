
-- {{{ find_outside_quotes()
local function find_outside_quotes(haystack, needle, i)
    local needle_where = haystack:find(needle, i, true)
    if not needle_where then
        return
    end

    local start_quote = haystack:find("\"", i, true)
    if not start_quote or start_quote > needle_where then
        return needle_where
    end

    local end_quote = haystack:find("\"", start_quote+1, true)
    if end_quote then
        if end_quote > needle_where then
            return find_outside_quotes(haystack, needle, end_quote+1)
        else
            return find_outside_quotes(haystack, needle, needle_where+1)
        end
    end
end
-- }}}

require "package"
require "ratchet"
local common = require "ratchet.smtp.common"

module("ratchet.smtp.server", package.seeall)
local class = getfenv()
__index = class
commands = {}

-- {{{ new()
function new(socket, handlers, send_size)
    local self = {}
    setmetatable(self, class)

    self.handlers = handlers
    self.io = common.smtp_io.new(socket, send_size)

    self.extensions = common.smtp_extensions.new()
    self.extensions:add("8BITMIME")
    self.extensions:add("PIPELINING")
    self.extensions:add("ENHANCEDSTATUSCODES")

    return self
end
-- }}}

-- {{{ send_ESC_reply()
function send_ESC_reply(self, reply)
    local code, message, esc = reply.code, reply.message, reply.enhanced_status_code
    local code_class = code:sub(1, 1)

    if code_class == "2" or code_class == "4" or code_class == "5" then
        -- If no Enhanced-Status-Code was given, use empty string.
        if not esc then
            esc = ""
        end

        -- Match E-S-C, and set defaults on non-match.
        local subject, detail = esc:match("^[245]%.(%d%d?%d?)%.(%d%d?%d?)$")
        if not subject then
            subject, detail = "0", "0"
        end

        -- Build E-S-C.
        esc = code_class .. "." .. subject .. "." .. detail

        -- Prefix E-S-C at the beginning of contiguous lines.
        message = esc .. " " .. message:gsub("(%\r?%\n)", "%0"..esc.." ")
    end

    self.io:send_reply(code, message)
end
-- }}}

-- {{{ get_message_data()
function get_message_data(self)
    local max_size = tonumber(self.extensions:has("SIZE"))
    local reader = common.data_reader.new(self.io, max_size)

    local data, err = reader:recv()

    local reply = {
        code = "250",
        message = "Message Accepted for Delivery",
        enhanced_status_code = "2.6.0"
    }

    if self.handlers.HAVE_DATA then
        self.handlers:HAVE_DATA(reply, data, err)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ close()
function close(self)
    if self.handlers.CLOSE then
        self.handlers:CLOSE()
    end
    self.io:close()
end
-- }}}

-- {{{ Generic error responses

-- {{{ unknown_command()
function unknown_command(self, command, arg, message)
    local reply = {
        code = "500",
        message = message or "Syntax error, command unrecognized",
        enhanced_status_code = "5.5.2",
    }
    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ unknown_parameter()
function unknown_parameter(self, command, arg, message)
    local reply = {
        code = "504",
        message = message or "Command parameter not implemented",
        enhanced_status_code = "5.5.4",
    }
    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ bad_sequence()
function bad_sequence(self, command, arg, message)
    local reply = {
        code = "503",
        message = message or "Bad sequence of commands",
        enhanced_status_code = "5.5.1",
    }
    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ bad_arguments()
function bad_arguments(self, command, arg, message)
    local reply = {
        code = "501",
        message = message or "Syntax error in parameters or arguments",
        enhanced_status_code = "5.5.4",
    }
    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- }}}

-- {{{ Built-in Commands

-- {{{ commands.BANNER()
function commands.BANNER(self)
    local reply = {
        code = "220",
        message = "ESMTP ratchet SMTP server " .. ratchet.version,
    }

    if self.handlers.BANNER then
        self.handlers:BANNER(reply)
    end

    self.io:send_reply(reply.code, reply.message)
    self.io:flush_send()
end
-- }}}

-- {{{ commands.EHLO()
function commands.EHLO(self, ehlo_as)
    local reply = {
        code = "250",
        message = "Hello " .. ehlo_as,
    }

    if self.handlers.EHLO then
        self.handlers:EHLO(reply, ehlo_as)
    end

    -- Add extensions, if success code.
    if reply.code == "250" then
        reply.message = self.extensions:build_string(reply.message)
    end

    self.io:send_reply(reply.code, reply.message)
    self.io:flush_send()

    if reply.code == "250" then
        self.have_mailfrom = nil
        self.have_rcptto = nil

        self.ehlo_as = ehlo_as
    end
end
-- }}}

-- {{{ commands.HELO()
function commands.HELO(self, ehlo_as)
    local reply = {
        code = "250",
        message = "Hello " .. ehlo_as,
    }

    if self.handlers.EHLO then
        self.handlers:EHLO(reply, ehlo_as)
    end

    self.io:send_reply(reply.code, reply.greeting)
    self.io:flush_send()

    if reply.code == "250" then
        self.have_mailfrom = nil
        self.have_rcptto = nil

        self.extensions:reset()
        self.ehlo_as = ehlo_as
    end
end
-- }}}

-- {{{ commands.STARTTLS()
function commands.STARTTLS(self, arg)
    if not self.extensions:has("STARTTLS") then
        return self:unknown_command("STARTTLS", arg)
    end

    if not self.ehlo_as then
        return self:bad_sequence("STARTTLS", arg)
    end

    local reply = {
        code = "220",
        message = "Go ahead",
        enhanced_status_code = "2.7.0"
    }

    if self.handlers.STARTTLS then
        self.handlers:STARTTLS(reply)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    if reply.code == "220" then
        local enc = self.io.socket:encrypt(ssl_server.SSLv3)
        enc:server_handshake()

        self.extensions:drop("STARTTLS")
        self.ehlo_as = nil
    end
end
-- }}}

-- {{{ commands.MAIL()
function commands.MAIL(self, arg)
    -- Ensure the syntax of the FROM:<address> arg portion.
    local start_address = arg:match("^[fF][rR][oO][mM]%:%s*%<()")
    if not start_address then
        return self:bad_arguments("MAIL", arg)
    end

    local end_address = find_outside_quotes(arg, ">", start_address)
    if not end_address then
        return self:bad_arguments("MAIL", arg)
    end

    local address = arg:sub(start_address, end_address-1)

    -- Ensure after an EHLO/HELO.
    if not self.ehlo_as then
        return self:bad_sequence("MAIL", arg)
    end

    -- Ensure not already in a mail transaction.
    if self.have_mailfrom then
        return self:bad_sequence("MAIL", arg)
    end

    -- Check for SIZE=NNNN parameter, and process if SIZE extension is enabled.
    local size = arg:match("%s[sS][iI][zZ][eE]%=(%d+)", end_address+1)
    if size then
        local max_size = self.extensions:has("SIZE")
        if max_size then
            if tonumber(size) > tonumber(max_size) then
                local reply = {
                    code = "552",
                    message = "Message size exceeds "..max_size.." limit",
                    enhanced_status_code = "5.3.4"
                }
                self:send_ESC_reply(reply)
                self.io:flush_send()
                return
            end
        else
            return self:unknown_parameter("MAIL", arg)
        end
    end

    -- Normal handling of command based on address.
    local reply = {
        code = "250",
        message = "Sender <"..address.."> Ok",
        enhanced_status_code = "2.1.0"
    }

    if self.handlers.MAIL then
        self.handlers:MAIL(reply, address)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    self.have_mailfrom = self.have_mailfrom or (reply.code == "250")
end
-- }}}

-- {{{ commands.RCPT()
function commands.RCPT(self, arg)
    -- Ensure the syntax of the TO:<address> arg portion.
    local start_address = arg:match("^[tT][oO]%:%s*%<()")
    if not start_address then
        return self:bad_arguments("RCPT", arg)
    end

    local end_address = find_outside_quotes(arg, ">", start_address)
    if not end_address then
        return self:bad_arguments("RCPT", arg)
    end

    local address = arg:sub(start_address, end_address-1)

    -- Ensure already in a mail transaction.
    if not self.have_mailfrom then
        return self:bad_sequence("RCPT", arg)
    end

    -- Normal handling of command based on address.
    local reply = {
        code = "250",
        message = "Recipient <"..address.."> Ok",
        enhanced_status_code = "2.1.5"
    }

    if self.handlers.RCPT then
        self.handlers:RCPT(reply, address)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    self.have_rcptto = self.have_rcptto or (reply.code == "250")
end
-- }}}

-- {{{ commands.DATA()
function commands.DATA(self, arg)
    if #arg > 0 then
        return self:bad_arguments("DATA", arg)
    end

    if not self.have_mailfrom then
        return self:bad_sequence("DATA", arg, "No valid sender given")
    elseif not self.have_rcptto then
        return self:bad_sequence("DATA", arg, "No valid recipients given")
    end

    local reply = {
        code = "354",
        message = "Start mail input; end with <CRLF>.<CRLF>",
    }

    if self.handlers.DATA then
        self.handlers:DATA(reply)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    if reply.code == "354" then
        self:get_message_data()
    end
end
-- }}}

-- {{{ commands.RSET()
function commands.RSET(self, arg)
    if #arg > 0 then
        return self:bad_arguments("RSET", arg)
    end

    local reply = {
        code = "250",
        message = "Ok",
    }

    if self.handlers.RSET then
        self.handlers:RSET(reply)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    if reply.code == "250" then
        self.have_mailfrom = nil
        self.have_rcptto = nil
    end
end
-- }}}

-- {{{ commands.NOOP()
function commands.NOOP(self)
    local reply = {
        code = "250",
        message = "Ok",
    }

    if self.handlers.NOOP then
        self.handlers:NOOP(reply)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ commands.QUIT()
function commands.QUIT(self, arg)
    if #arg > 0 then
        return self:bad_arguments("QUIT", arg)
    end

    local reply = {
        code = "221",
        message = "Bye",
    }

    if self.handlers.QUIT then
        self.handlers:QUIT(reply)
    end

    self:send_ESC_reply(reply)
    self.io:flush_send()

    self:close()
end
-- }}}

-- }}}

-- {{{ custom_command()
function custom_command(self, command, arg)
    local reply = {
        code = "250",
        message = "Ok",
    }

    self.handlers[command](self.handlers, reply, arg, io)

    self:send_ESC_reply(reply)
    self.io:flush_send()
end
-- }}}

-- {{{ handle()
function handle(self)
    self.commands.BANNER(self)

    repeat
        local command, arg = self.io:recv_command()
        if self.commands[command] then
            self.commands[command](self, arg)
        elseif self.handlers[command] then
            self:custom_command(command, arg)
        else
            self:unknown_command(command, arg)
        end
    until command == "QUIT"
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et: