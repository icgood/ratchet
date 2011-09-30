require "ratchet"
require "ratchet.bus"

function ctx1(where)
    local s_bus, c_bus = ratchet.bus.new_bus()

    kernel:attach(server_socket, s_client, s_bus)
    kernel:attach(client_socket, c_socket, c_bus)
end

function server_socket(socket, bus)
    local request, response = bus:recv_request()
    assert("important" == data.data)
end

function client_socket(socket, bus)
end

kernel = ratchet.new()
kernel:attach(ctx1)
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
