
local smtp_io = {}
smtp_io.__index = smtp_io

-- {{{ smtp_io.new()
function smtp_io.new(socket)
    local self = {}
    setmetatable(self, smtp_io)

    self.socket = socket

    self.send_buffer = {}
    self.recv_buffer = ""

    return self
end
-- }}}

-- {{{ smtp_io:close()
function smtp_io:close()
    local enc = self.socket:get_encryption()
    if enc then
        enc:shutdown()
    end
    return self.socket:close()
end
-- }}}

-- {{{ smtp_io:buffered_recv()
function smtp_io:buffered_recv()
    local received = self.socket:recv()

    if received == "" then
        local err = ratchet.error.new("Connection closed.", "ECONNCLOSED", "ratchet.smtp.io.recv()")
        error(err)
    end

    self.recv_buffer = self.recv_buffer .. received

    return self.recv_buffer
end
-- }}}

-- {{{ smtp_io:buffered_send()
function smtp_io:buffered_send(data)
    table.insert(self.send_buffer, data)
end
-- }}}

-- {{{ smtp_io:flush_send()
function smtp_io:flush_send()
    local send_buffer = table.concat(self.send_buffer)
    self.send_buffer = {}

    repeat
        send_buffer = self.socket:send(send_buffer)
    until not send_buffer
end
-- }}}

-- {{{ smtp_io:recv_reply()
function smtp_io:recv_reply()
    local pattern
    local code, message_lines = nil, {}
    local bad_line_pattern = "^(.-)%\r?%\n()"
    local incomplete = true
    local input = self.recv_buffer

    while incomplete do
        -- Build the full reply pattern once we know the code.
        if not pattern then
            code = input:match("^%d%d%d")
            if code then
                pattern = "^" .. code .. "([% %\t%-])(.-)%\r?%\n()"
            else
                local bad_line, end_i = input:match(bad_line_pattern)
                if bad_line then
                    self.recv_buffer = self.recv_buffer:sub(end_i)
                    return nil, bad_line
                end
            end
        end

        -- Check for lines that match the pattern.
        if pattern then
            local start_i = 1
            repeat
                local splitter, line, end_i = input:match(pattern, start_i)
                if line then
                    table.insert(message_lines, line)
                    self.recv_buffer = self.recv_buffer:sub(end_i)

                    if splitter ~= "-" then
                        incomplete = false
                        start_i = nil
                    else
                        start_i = end_i
                    end
                else
                    local bad_line, end_i = input:match(bad_line_pattern)
                    if bad_line then
                        self.recv_buffer = self.recv_buffer:sub(end_i)
                        return nil, bad_line
                    else
                        start_i = nil
                    end
                end
            until not start_i
        end

        -- Check if we need to receive more data.
        if incomplete then
            input = self:buffered_recv()
        end
    end

    return code, table.concat(message_lines, "\r\n")
end
-- }}}

-- {{{ smtp_io:recv_line()
function smtp_io:recv_line()
    local input = self.recv_buffer

    while true do
        local line, end_i = input:match("^(.-)%\r?%\n()")
        if line then
            self.recv_buffer = self.recv_buffer:sub(end_i)
            return line
        end

        input = self:buffered_recv()
    end
end
-- }}}

-- {{{ smtp_io:recv_command()
function smtp_io:recv_command()
    local line = self:recv_line()
    local command, extra = line:match("^(%a+)%s*(.-)%s*$")
    if command then
        return command:upper(), extra
    else
        return line
    end
end
-- }}}

-- {{{ smtp_io:send_reply()
function smtp_io:send_reply(code, message)
    local lines = {}
    message = message .. "\r\n"
    for line in message:gmatch("(.-)%\r?%\n") do
        table.insert(lines, line)
    end
    local num_lines = #lines

    if num_lines == 0 then
        local to_send = code .. " " .. message .. "\r\n"
        return self:buffered_send(to_send)
    else
        local to_send = ""
        for i=1,(num_lines-1) do
            to_send = to_send .. code .. "-" .. lines[i] .. "\r\n"
        end
        to_send = to_send .. code .. " " .. lines[num_lines] .. "\r\n"
        return self:buffered_send(to_send)
    end
end
-- }}}

-- {{{ smtp_io:send_command()
function smtp_io:send_command(command)
    return self:buffered_send(command.."\r\n")
end
-- }}}

return smtp_io

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
