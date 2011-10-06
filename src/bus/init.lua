
require "ratchet.bus.samestate"

module("ratchet.bus", package.seeall)

-- {{{ new_local()
function new_local(...)
    return ratchet.bus.samestate.new(...)
end
-- }}}

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
