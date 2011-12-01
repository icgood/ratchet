
require "ratchet"

ratchet.bus = {}

require "ratchet.bus.samestate"
require "ratchet.bus.server"
require "ratchet.bus.client"

-- {{{ ratchet.bus.new_local()
function ratchet.bus.new_local(...)
    return ratchet.bus.samestate.new(...)
end
-- }}}

-- {{{ ratchet.bus.new_server()
function ratchet.bus.new_server(...)
    return ratchet.bus.server.new(...)
end
-- }}}

-- {{{ ratchet.bus.new_client()
function ratchet.bus.new_client(...)
    return ratchet.bus.client.new(...)
end
-- }}}

return ratchet.bus

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
