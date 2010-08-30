#!/usr/bin/env lua

local makeclass = require("luah.makeclass")

remaining = 0
testing = makeclass()

function testing:init(m1, m2)
	self.data = m1
	remaining = remaining + 1
	self.gcassert = 'aul'
	return m2
end
function testing:del()
	remaining = remaining - 1
	assert('aul' == self.gcassert, 'Garbage collecting unknown object')
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

-- Check for destructors
assert(2 == remaining)
t1 = nil
t2 = nil
collectgarbage('collect')
assert(0 == remaining, 'remaining: 0 != '..remaining)

