#!/usr/bin/env lua

require "luah"

assert(luah.makeclass, "Makeclass tool not available as submodule!");
assert(luah.ratchet, "Ratchet library not available as submodule!");
assert(luah.epoll, "Epoll library not available as submodule!")
assert(luah.socket, "Socket library not available as submodule!")
assert(luah.dns, "Dns library not available as submodule!")
assert(luah.rlimit, "Rlimit library not available as submodule!")
assert(luah.zmq, "ZMQ library not available as submodule!")
assert(luah.xml, "XML library not available as submodule!")

