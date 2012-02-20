
local data_reader = {}
data_reader.__index = data_reader

-- {{{ data_reader.new()
function data_reader.new(io, max_size)
    local self = {}
    setmetatable(self, data_reader)

    self.io = io
    self.size = 0
    self.max_size = max_size

    self.lines = {""}
    self.i = 1

    return self
end
-- }}}

-- {{{ data_reader:from_recv_buffer()
function data_reader:from_recv_buffer()
    self:add_lines(self.io.recv_buffer)
    self.io.recv_buffer = ""
end
-- }}}

-- {{{ data_reader:strip_EOD_endline()
function data_reader:strip_EOD_endline()
    if self.EOD > 1 then
        local last_line = self.lines[self.EOD-1]
        self.lines[self.EOD-1] = last_line:match("(.-)%\r?%\n") or last_line
    end
end
-- }}}

-- {{{ data_reader:handle_finished_line()
function data_reader:handle_finished_line()
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
    end
end
-- }}}

-- {{{ data_reader:add_lines()
function data_reader:add_lines(piece)
    for line in piece:gmatch("(.-%\n)") do
        self.lines[self.i] = self.lines[self.i] .. line
        self:handle_finished_line()        
    end
    self.lines[self.i] = self.lines[self.i] .. piece:match("([^%\n]*)$")
end
-- }}}

-- {{{ data_reader:recv_piece()
function data_reader:recv_piece()
    if self.EOD then
        return false
    end

    local piece = self.io.socket:recv()
    if piece == "" then
        local err = ratchet.error.new("Connection closed.", "ECONNCLOSED", "ratchet.smtp.data.recv()")
        error(err)
    end

    self.size = self.size + #piece
    if self.max_size and self.size > self.max_size then
        self.error = ratchet.error.new("Message breached size limit.", "MSGTOOBIG", "ratchet.smtp.data.recv()")
        self.EOD = self.i
        return false
    end

    self:add_lines(piece)
    return not self.EOD
end
-- }}}

-- {{{ data_reader:return_all()
function data_reader:return_all()
    if self.error then
        return nil, self.error
    end

    -- Save extra lines back on the recv_buffer.
    self.io.recv_buffer = table.concat(self.lines, "", self.EOD+1)

    -- Return the data as one big string.
    return table.concat(self.lines, "", 1, self.EOD-1)
end
-- }}}

-- {{{ data_reader:recv()
function data_reader:recv()
    self:from_recv_buffer()

    while self:recv_piece() do end

    return self:return_all()
end
-- }}}

return data_reader

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
