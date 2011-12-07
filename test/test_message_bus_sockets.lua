require "ratchet"
require "ratchet.bus"

-- {{{ request_to_bus()
local function request_to_bus(obj)
    return tostring(obj.id), {tostring(obj.stuff)}
end
-- }}}

-- {{{ request_from_bus()
local function request_from_bus(data, attachments)
    return {id = data, stuff = attachments[1]}
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

    kernel:attach(server_bus, s_socket, c2_socket)
    kernel:attach(client_bus_1, c1_socket)
end

function server_bus(socket, c2_socket)
    local bus = ratchet.bus.new_server(socket, request_from_bus, response_to_bus)

    local transaction1, request1 = bus:recv_request()
    assert(request1.id == "operation falcon")
    assert(request1.stuff == "important")

    kernel:attach(client_bus_2, c2_socket)

    local transaction2, request2 = bus:recv_request()
    assert(request2.id == "operation condor")
    assert(request2.stuff == "confidential")

    transaction1:send_response(1337)
    transaction2:send_response(7357)
end

function client_bus_1(socket)
    local bus = ratchet.bus.new_client(socket, request_to_bus, response_from_bus)

    local test_request = {id = "operation falcon", stuff = "important"}

    local transaction = bus:send_request(test_request)
    local response = transaction:recv_response()
    assert(1337 == response)
end

function client_bus_2(socket)
    local bus = ratchet.bus.new_client(socket, request_to_bus, response_from_bus)

    local test_request = {id = "operation condor", stuff = "confidential"}

    local transaction = bus:send_request(test_request)
    local response = transaction:recv_response()
    assert(7357 == response)
end

kernel = ratchet.kernel.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
