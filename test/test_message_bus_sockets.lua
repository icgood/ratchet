require "ratchet"
require "ratchet.bus"

local obj = {
    data = "important",

}


function ctx1(where)
    local s_rec = ratchet.socket.prepare_uri(where)
    local s_socket = ratchet.socket.new(s_rec.family, s_rec.socktype, s_rec.protocol)
    s_socket.SO_REUSEADDR = true
    s_socket:bind(s_rec.addr)
    s_socket:listen()

    local c_rec = ratchet.socket.prepare_uri(where)
    local c_socket = ratchet.socket.new(c_rec.family, c_rec.socktype, c_rec.protocol)
    c_socket:connect(c_rec.addr)

    local s_client = s_socket:accept()

    local s_bus = ratchet.bus.new_bus()
    local c_bus = ratchet.bus.new_bus(s_bus)

    kernel:attach(server_socket, s_client, s_bus)
    kernel:attach(client_socket, c_socket, c_bus)
end

function server_socket(socket, bus)
    bus:use_socket(socket)

end

function client_socket(socket, bus)
    bus:use_socket(socket)
end

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
