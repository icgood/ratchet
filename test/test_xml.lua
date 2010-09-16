#!/usr/bin/env lua

require "luah.xml"

bits = 0
state = {}
function state:start(name, attrs)
	assert(self == state)
	assert(name == 'tag')
	assert(attrs.one == 'two')
	assert(bits == 0)
	bits = bits + 1
end
function state:end_(name)
	assert(self == state)
	assert(name == 'tag')
	assert(bits == 3)
	bits = bits + 4
end
function state:data(d)
	assert(self == state)
	assert(d == 'stuff in the tag')
	assert(bits == 1)
	bits = bits + 2
end

p = luah.xml{state=state, startelem=state.start, endelem=state.end_, elemdata=state.data}
p:parse('<tag one="two">stuff in the tag</tag>')
assert(bits == 7)

