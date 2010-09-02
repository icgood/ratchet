#!/usr/bin/env lua

require "luah"

assert(luah.makeclass, "Makeclass tool not available as submodule!");
assert(luah.ratchet, "Ratchet library not available as submodule!");
assert(luah.ratchet.epoll, "Epoll library not available as submodule!")
assert(luah.ratchet.socket, "Socket library not available as submodule!")

