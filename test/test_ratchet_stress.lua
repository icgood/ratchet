#!/usr/bin/env lua

require "luah"

soft, hard = luah.rlimit.get(luah.rlimit.NOFILE)
luah.rlimit.set(luah.rlimit.NOFILE, hard, hard)

r = luah.ratchet()

-- {{{ user_context: Handles the connection from the "outside world"
user_context = r:new_context()
function user_context:on_init()
    self:send('Hello...')
end
function user_context:on_recv()
    local data = self:recv()
    self:close()
    done = done + 1
end
-- }}}

-- {{{ client_context: Handles the "server-side" of the connection from the "outside world"
client_context = r:new_context()
function client_context:on_recv()
    local data = self:recv()
    self:send('World!')
end
function client_context:on_send(data)
    self:close()
    done = done + 1
end
-- }}}

-- {{{ server_context: Accepts and initializes new connections from the "outside world"
server_context = r:new_context()
function server_context:on_recv()
    return self:accept(client_context)
end
-- }}}

s1 = r:listen('tcp://*:1234', server_context)

n = 10000
done = 0
for i=1,n do
    r:connect('tcp://127.0.0.1:1234', user_context)
end

i = 0
while done < n*2 do
    r:run{iterations=1, timeout=0.1, maxevents=10}
    i = i + 1
end
print("total iterations: " .. i)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
