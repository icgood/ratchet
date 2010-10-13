#!/usr/bin/env lua

require "ratchet"

assert(ratchet.makeclass, "Makeclass tool not available as submodule!");
assert(ratchet.epoll, "Epoll library not available as submodule!")
assert(ratchet.socket, "Socket library not available as submodule!")
assert(ratchet.dns, "Dns library not available as submodule!")
assert(ratchet.zmq, "ZMQ library not available as submodule!")

