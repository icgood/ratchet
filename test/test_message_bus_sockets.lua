require "ratchet"
require "ratchet.bus"

-- {{{ request_to_bus()
local function request_to_bus(obj)
    return tostring(obj.id) .. "||" .. tostring(obj.stuff)
end
-- }}}

-- {{{ request_from_bus()
local function request_from_bus(data)
    local obj = {}
    obj.id, obj.stuff = data:match("^([^%|]*)%|%|(.*)$")
    return obj
end
-- }}}

-- {{{ response_to_bus()
function response_to_bus(obj)
    return tostring(obj)
end
-- }}}

-- {{{ response_from_bus()
function response_from_bus(data)
    return tonumber(data)
end
-- }}}

function ctx1(where)
    local s_rec = ratchet.socket.prepare_uri(where)
    local s_socket = ratchet.socket.new(s_rec.family, s_rec.socktype, s_rec.protocol)
    s_socket.SO_REUSEADDR = true
    s_socket:bind(s_rec.addr)
    s_socket:listen()

    local c1_rec = ratchet.socket.prepare_uri(where)
    local c1_socket = ratchet.socket.new(c1_rec.family, c1_rec.socktype, c1_rec.protocol)
    c1_socket:connect(c1_rec.addr)

    local c2_rec = ratchet.socket.prepare_uri(where)
    local c2_socket = ratchet.socket.new(c2_rec.family, c2_rec.socktype, c2_rec.protocol)
    c2_socket:connect(c2_rec.addr)

    kernel:attach(server_socket, s_socket)
    kernel:attach(client_socket, c1_socket)
    kernel:attach(client_socket, c2_socket)
end

function server_socket(socket)
    local bus = ratchet.bus.new_server(socket, request_from_bus, response_to_bus)

    local test_response = 1337

    local transaction1, request1 = bus:recv_request()
    assert(request1.id == "operation falcon")
    assert(request1.stuff == "important")

    local transaction2, request2 = bus:recv_request()
    assert(request2.id == "operation falcon")
    assert(request2.stuff == "important")

    transaction1:send_response(test_response)
    transaction2:send_response(test_response)
end

function client_socket(socket)
    local bus = ratchet.bus.new_client(socket, request_to_bus, response_from_bus)

    local test_request = {id = "operation falcon", stuff = "important"}

    local transaction = bus:send_request(test_request)
    local response = transaction:recv_response()
    assert(1337 == response)
end

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
