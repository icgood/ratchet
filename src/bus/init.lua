
require "ratchet"

ratchet.bus = {}

local samestate = require "ratchet.bus.samestate"
local server = require "ratchet.bus.server"
local client = require "ratchet.bus.client"

-- {{{ ratchet.bus.new_local()
function ratchet.bus.new_local(...)
    return samestate.new(...)
end
-- }}}

-- {{{ ratchet.bus.new_server()
function ratchet.bus.new_server(...)
    return server.new(...)
end
-- }}}

-- {{{ ratchet.bus.new_client()
function ratchet.bus.new_client(...)
    return client.new(...)
end
-- }}}

return ratchet.bus

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
