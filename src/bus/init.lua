
require "ratchet.bus.samestate"
require "ratchet.bus.server"
require "ratchet.bus.client"

module("ratchet.bus", package.seeall)

-- {{{ new_local()
function new_local(...)
    return ratchet.bus.samestate.new(...)
end
-- }}}

-- {{{ new_server()
function new_server(...)
    return ratchet.bus.server.new(...)
end
-- }}}

-- {{{ new_client()
function new_client(...)
    return ratchet.bus.client.new(...)
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
