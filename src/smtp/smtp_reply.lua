
local smtp_reply = {}
smtp_reply.__index = smtp_reply

-- {{{ smtp_reply.new()
function smtp_reply.new(command)
    local self = {}
    setmetatable(self, smtp_reply)

    self.command = command or ""

    return self
end
-- }}}

-- {{{ smtp_reply:recv()
function smtp_reply:recv(io)
    self.code, self.message = io:recv_reply()
end
-- }}}

-- {{{ smtp_reply:error()
function smtp_reply:error(description)
    return {
        command = self.command,
        code = tostring(self.code),
        message = self.message,
        description = description,
    }
end
-- }}}

return smtp_reply

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
