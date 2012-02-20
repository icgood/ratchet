
local data_sender = {}
data_sender.__index = data_sender

-- {{{ data_sender.new()
function data_sender.new(data, iter_size)
    local self = {}
    setmetatable(self, data_sender)

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

-- {{{ data_sender:iter()
function data_sender:iter()
    local invariant = {send_size = self.iter_size,
                       last_part = "",
                       done = false,
                       full_message = self.data}

    return iterator_func, invariant, 1
end
-- }}}

-- {{{ data_sender:send()
function data_sender:send(io)
    for i, piece in self:iter() do
        io:buffered_send(piece)
    end
end
-- }}}

return data_sender

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
