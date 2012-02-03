
require "package"

local common = {}

-- {{{ common.data_reader

common.data_reader = {}
common.data_reader.__index = common.data_reader

-- {{{ common.data_reader.new()
function common.data_reader.new(io, max_size)
    local self = {}
    setmetatable(self, common.data_reader)

    self.io = io
    self.size = 0
    self.max_size = max_size

    self.lines = {""}
    self.i = 1

    return self
end
-- }}}

-- {{{ common.data_reader:from_recv_buffer()
function common.data_reader:from_recv_buffer()
    self:add_lines(self.io.recv_buffer)
    self.io.recv_buffer = ""
end
-- }}}

-- {{{ common.data_reader:strip_EOD_endline()
function common.data_reader:strip_EOD_endline()
    if self.EOD > 1 then
        local last_line = self.lines[self.EOD-1]
        self.lines[self.EOD-1] = last_line:match("(.-)%\r?%\n") or last_line
    end
end
-- }}}

-- {{{ common.data_reader:handle_finished_line()
function common.data_reader:handle_finished_line()
    local i = self.i
    local line = self.lines[i]

    -- Move internal trackers ahead.
    self.i = self.i + 1
    self.lines[self.i] = ""

    -- Only handle lines within the data.
    if not self.EOD then
        -- Check for the End-Of-Data marker.
        if line:match("^%.%\r?%\n") then
            self.EOD = i
            self:strip_EOD_endline()
            return

        -- Remove an initial period on non-EOD lines as per RFC 821 4.5.2.
        elseif line:sub(1, 1) == "." then
            line = line:sub(2)
            self.lines[i] = line
        end

        -- Add line length to total message size.
        self.size = self.size + #line
        
        -- If over message size, reset to effectively stop reading and prevent
        -- memory hemorrhaging by malicious/broken clients.
        if self.max_size and self.size > self.max_size then
            self.lines = {""}
            self.i = 1
            return
        end
    end
end
-- }}}

-- {{{ common.data_reader:add_lines()
function common.data_reader:add_lines(piece)
    for line in piece:gmatch("(.-%\n)") do
        self.lines[self.i] = self.lines[self.i] .. line
        self:handle_finished_line()        
    end
    self.lines[self.i] = self.lines[self.i] .. piece:match("([^%\n]*)$")
end
-- }}}

-- {{{ common.data_reader:recv_piece()
function common.data_reader:recv_piece()
    local piece = self.io.socket:recv()
    if piece == "" then
        self.connection_closed = true
        self.EOD = true
        return
    end

    self:add_lines(piece)
end
-- }}}

-- {{{ common.data_reader:return_all()
function common.data_reader:return_all()
    if self.connection_closed then
        return nil, "connection closed"
    end

    if self.max_size and self.size > self.max_size then
        return nil, "message breached size limit"
    end

    -- Save extra lines back on the recv_buffer.
    self.io.recv_buffer = table.concat(self.lines, "", self.EOD+1)

    -- Return the data as one big string.
    return table.concat(self.lines, "", 1, self.EOD-1)
end
-- }}}

-- {{{ common.data_reader:recv()
function common.data_reader:recv()
    self:from_recv_buffer()

    while not self.EOD do
        self:recv_piece()
    end

    return self:return_all()
end
-- }}}

-- }}}

-- {{{ common.data_sender

common.data_sender = {}
common.data_sender.__index = common.data_sender

-- {{{ common.data_sender.new()
function common.data_sender.new(data, iter_size)
    local self = {}
    setmetatable(self, common.data_sender)

    self.data = data
    self.iter_size = iter_size or 1024

    return self
end
-- }}}

-- {{{ iterator_func()
local function iterator_func(invariant, i)
    local message = invariant.full_message
    local len = invariant.send_size
    local last_part = invariant.last_part

    -- If we're done iterator, jump out.
    if invariant.done then
        return
    end

    local piece = message:sub(i, i+len-1)
    if piece == "" then
        -- We are done iterating over the message, but need to return
        -- the ".\r\n" to end DATA command.
        invariant.done = true
        local end_marker = ".\r\n"
        if #last_part > 0 and last_part ~= "\r\n" then
            end_marker = "\r\n" .. end_marker
        end
        return i, end_marker
    end

    piece = last_part .. piece

    piece = piece:gsub("%\r", "")
    piece = piece:gsub("%\n", "\r\n")
    piece = piece:gsub("%\n%.", "\n..")

    local delta = (2 * len) - (#piece - #last_part)
    piece = piece:sub(1+#last_part, len+#last_part)
    invariant.last_part = piece:sub(-2)

    return i + delta, piece
end
-- }}}

-- {{{ common.data_sender:iter()
function common.data_sender:iter()
    local invariant = {send_size = self.iter_size,
                       last_part = "",
                       done = false,
                       full_message = self.data}

    return iterator_func, invariant, 1
end
-- }}}

-- {{{ common.data_sender:send()
function common.data_sender:send(io)
    for i, piece in self:iter() do
        io:buffered_send(piece)
    end
end
-- }}}

-- }}}

-- {{{ common.smtp_extensions

common.smtp_extensions = {}
common.smtp_extensions.__index = common.smtp_extensions

-- {{{ common.smtp_extensions.new()
function common.smtp_extensions.new()
    local self = {}
    setmetatable(self, common.smtp_extensions)

    self.extensions = {}

    return self
end
-- }}}

-- {{{ common.smtp_extensions:reset()
function common.smtp_extensions:reset()
    self.extensions = {}
end
-- }}}

-- {{{ common.smtp_extensions:has()
function common.smtp_extensions:has(ext)
    return self.extensions[ext:upper()]
end
-- }}}

-- {{{ common.smtp_extensions:add()
function common.smtp_extensions:add(ext, param)
    if param and #param > 0 then
        self.extensions[ext:upper()] = param
    else
        self.extensions[ext:upper()] = true
    end
end
-- }}}

-- {{{ common.smtp_extensions:drop()
function common.smtp_extensions:drop(ext)
    self.extensions[ext:upper()] = nil
end
-- }}}

-- {{{ common.smtp_extensions:parse_string()
function common.smtp_extensions:parse_string(str)
    local pattern = "^%s*(%w[%w%-]*)%s*(.-)%s*$"
    local header
    str = str .. "\r\n" -- incoming strings will not have a final endline.
    for line in str:gmatch("(.-)%\r?%\n") do
        if not header then
            header = line
        else
            self:add(line:match(pattern))
        end
    end

    return header or str
end
-- }}}

-- {{{ common.smtp_extensions:build_string()
function common.smtp_extensions:build_string(header)
    local lines = {header}
    for k, v in pairs(self.extensions) do
        if v == true then
            table.insert(lines, k)
        else
            table.insert(lines, k.." "..v)
        end
    end
    return table.concat(lines, "\r\n")
end
-- }}}

-- }}}

-- {{{ common.smtp_io

common.smtp_io = {}
common.smtp_io.__index = common.smtp_io

-- {{{ common.smtp_io.new()
function common.smtp_io.new(socket, send_size)
    local self = {}
    setmetatable(self, common.smtp_io)

    self.socket = socket

    -- socket(7) option SO_SNDBUF returns double the desired value.
    self.send_size = send_size or (socket.SO_SNDBUF / 2)

    self.send_buffer = {}
    self.recv_buffer = ""

    return self
end
-- }}}

-- {{{ common.smtp_io:close()
function common.smtp_io:close()
    return self.socket:close()
end
-- }}}

-- {{{ common.smtp_io:buffered_recv()
function common.smtp_io:buffered_recv()
    local received = self.socket:recv()
    local done

    if not received then
        done = "timed out"
    elseif received == "" then
        done = "connection closed"
    else
        self.recv_buffer = self.recv_buffer .. received
    end

    return self.recv_buffer, done
end
-- }}}

-- {{{ common.smtp_io:buffered_send()
function common.smtp_io:buffered_send(data)
    table.insert(self.send_buffer, data)
end
-- }}}

-- {{{ common.smtp_io:flush_send()
function common.smtp_io:flush_send()
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

-- {{{ common.smtp_io:recv_reply()
function common.smtp_io:recv_reply()
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

-- {{{ common.smtp_io:recv_command()
function common.smtp_io:recv_command()
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

-- {{{ common.smtp_io:send_reply()
function common.smtp_io:send_reply(code, message)
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

-- {{{ common.smtp_io:send_command()
function common.smtp_io:send_command(command)
    return self:buffered_send(command.."\r\n")
end
-- }}}

-- }}}

-- {{{ common.smtp_reply

common.smtp_reply = {}
common.smtp_reply.__index = common.smtp_reply

-- {{{ common.smtp_reply.new()
function common.smtp_reply.new(command)
    local self = {}
    setmetatable(self, common.smtp_reply)

    self.command = command or ""

    return self
end
-- }}}

-- {{{ common.smtp_reply:recv()
function common.smtp_reply:recv(io)
    self.code, self.message = io:recv_reply()
end
-- }}}

-- {{{ common.smtp_reply:error()
function common.smtp_reply:error(description)
    return {
        command = self.command,
        code = tostring(self.code),
        message = self.message,
        description = description,
    }
end
-- }}}

-- }}}

return common

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
