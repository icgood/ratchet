#!/usr/bin/env lua

local luah = require("luah")

assert(luah.makeclass, "Makeclass tool not available as submodule!");
assert(luah.netlib, "Netlib library not available as submodule!");
assert(luah.netlib.epoll, "Epoll library not available as submodule!")
assert(luah.netlib.socket, "Socket library not available as submodule!")

