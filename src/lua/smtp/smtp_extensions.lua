
local smtp_extensions = {}
smtp_extensions.__index = smtp_extensions

-- {{{ smtp_extensions.new()
function smtp_extensions.new()
    local self = {}
    setmetatable(self, smtp_extensions)

    self.extensions = {}

    return self
end
-- }}}

-- {{{ smtp_extensions:reset()
function smtp_extensions:reset()
    self.extensions = {}
end
-- }}}

-- {{{ smtp_extensions:has()
function smtp_extensions:has(ext)
    return self.extensions[ext:upper()]
end
-- }}}

-- {{{ smtp_extensions:add()
function smtp_extensions:add(ext, param)
    if param then
        self.extensions[ext:upper()] = param
    else
        self.extensions[ext:upper()] = true
    end
end
-- }}}

-- {{{ smtp_extensions:drop()
function smtp_extensions:drop(ext)
    self.extensions[ext:upper()] = nil
end
-- }}}

-- {{{ smtp_extensions:parse_string()
function smtp_extensions:parse_string(str)
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

-- {{{ smtp_extensions:build_string()
function smtp_extensions:build_string(header)
    local lines = {header}
    for k, v in pairs(self.extensions) do
        if v == true then
            table.insert(lines, k)
        else
            table.insert(lines, k.." "..tostring(v))
        end
    end
    return table.concat(lines, "\r\n")
end
-- }}}

return smtp_extensions

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
