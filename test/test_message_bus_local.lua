require "ratchet"
require "ratchet.bus"

function server_bus(bus)
    local test_response_1 = {data = "important answer!"}
    local test_response_2 = "yes"

    local transaction_1, request_1 = bus:recv_request()
    ratchet.thread.attach(client_bus_2, bus)
    local transaction_2, request_2 = bus:recv_request()

    assert("important question!" == request_1.data)
    assert(1337 == request_2)

    transaction_1:send_response(test_response_1)
    transaction_2:send_response(test_response_2)
end

function client_bus_1(bus)
    local test_request = {data = "important question!"}

    local transaction = bus:send_request(test_request)
    local response = transaction:recv_response()
    assert("important answer!" == response.data)
end

function client_bus_2(bus)
    local test_request = 1337

    local transaction = bus:send_request(test_request)
    local response = transaction:recv_response()
    assert("yes" == response)
end

kernel = ratchet.new(function ()
    local bus = ratchet.bus.new_local(kernel)

    ratchet.thread.attach(server_bus, bus)
    ratchet.thread.attach(client_bus_1, bus)
end)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
