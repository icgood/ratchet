
local smtp_io = {}
smtp_io.__index = smtp_io

-- {{{ smtp_io.new()
function smtp_io.new(socket, send_size)
    local self = {}
    setmetatable(self, smtp_io)

    self.socket = socket

    -- socket(7) option SO_SNDBUF returns double the desired value.
    self.send_size = send_size or (socket.SO_SNDBUF / 2)

    self.send_buffer = {}
    self.recv_buffer = ""

    return self
end
-- }}}

-- {{{ smtp_io:close()
function smtp_io:close()
    return self.socket:close()
end
-- }}}

-- {{{ smtp_io:buffered_recv()
function smtp_io:buffered_recv()
    local received = self.socket:recv()
    local done

    if received == "" then
        done = "connection closed"
    else
        self.recv_buffer = self.recv_buffer .. received
    end

    return self.recv_buffer, done
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

    while #send_buffer > self.send_size do
        local to_send = send_buffer:sub(1, self.send_size)
        self.socket:send(to_send)
        send_buffer = send_buffer:sub(self.send_size+1)
    end

    if #send_buffer > 0 then
        self.socket:send(send_buffer)
    end
end
-- }}}

-- {{{ smtp_io:recv_reply()
function smtp_io:recv_reply()
    local pattern
    local code, message_lines = nil, {}
    local bad_line_pattern = "^(.-)%\r?%\n()"
    local incomplete = true
    local input, done = self.recv_buffer

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

        -- Handle timeouts and premature closure.
        if done then
            return nil, done
        end

        -- Check if we need to receive more data.
        if incomplete then
            input, done = self:buffered_recv()
        end
    end

    return code, table.concat(message_lines, "\r\n")
end
-- }}}

-- {{{ smtp_io:recv_command()
function smtp_io:recv_command()
    local input, done = self.recv_buffer

    while true do
        local line, end_i = input:match("^(.-)%\r?%\n()")
        if line then
            self.recv_buffer = self.recv_buffer:sub(end_i)

            local command, extra = line:match("^(%a+)%s*(.-)%s*$")
            if command then
                return command:upper(), extra
            else
                return line
            end
        end

        -- Handle timeouts and premature closure.
        if done then
            return nil, done
        end

        input, done = self:buffered_recv()
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
