#!/usr/bin/env lua

require("luah.ratchet")

function testparser(extra)
	assert(extra == "//1.2.3.4")
	return "testparts"
end

t, d = luah.ratchet.prototype.parseuri("test://1.2.3.4", {test = testparser})
assert(t == "test")
assert(d == "testparts")

