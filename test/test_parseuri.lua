#!/usr/bin/env lua

require("ratchet")

function testparser(extra)
	assert(extra == "//1.2.3.4")
	return "testparts"
end

t, d = ratchet.prototype.parseuri("test://1.2.3.4", {test = testparser})
assert(t == "test")
assert(d == "testparts")

