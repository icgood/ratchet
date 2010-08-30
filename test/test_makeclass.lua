#!/usr/bin/env lua

local makeclass = require("luah.makeclass")

testing = makeclass()

function testing:init(m1, m2)
	self.data = m1
	return m2
end
function testing:stuff(a1, a2)
	return '' .. a1 .. ' ' .. a2 .. ' ' .. self.data
end

t1, ret1 = testing('one', 'data1')
t2, ret2 = testing('two', 'data2')

assert("data1" == ret1)
assert("a b one" == t1:stuff("a", "b"))

assert("data2" == ret2)
assert("c d two" == t2:stuff("c", "d"))

