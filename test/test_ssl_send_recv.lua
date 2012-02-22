require "ratchet"

counter = 0

function ctx1(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:setsockopt("SO_REUSEADDR", true)
    socket:bind(rec.addr)
    socket:listen()

    ratchet.thread.attach(ctx2, host, port)

    local client = socket:accept()

    -- Portion being tested.
    --
    local data = client:recv()
    assert(data == "not encrypted")
    client:send("yet")

    local enc = client:encrypt(ssl1)
    enc:server_handshake()

    assert("AES256-SHA" == enc:get_cipher())

    client:send("hello")
    local data = client:recv(5)
    assert(data == "world")

    local data = client:recv()
    assert(data == "foo")
    client:send("bar")

    enc:shutdown()
    client:close()

    counter = counter + 1
end

function ctx2(host, port)
    local rec = ratchet.socket.prepare_tcp(host, port)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    socket:send("not encrypted")
    local data = socket:recv()
    assert(data == "yet")

    local enc = socket:encrypt(ssl2)
    enc:client_handshake()

    local got_cert, verified, host_matched = enc:verify_certificate(rec.host)
    assert(got_cert and verified and host_matched)

    assert("AES256-SHA" == enc:get_cipher())
    assert("CN=localhost" == enc:get_rfc2253())

    local data = socket:recv(5)
    assert(data == "hello")
    socket:send("world")

    socket:send("foo")
    local data = socket:recv()
    assert(data == "bar")

    enc:shutdown()
    socket:close()

    counter = counter + 2
end

ssl1 = ratchet.ssl.new(ratchet.ssl.SSLv3_server)
ssl1:load_certs("cert.pem")

ssl2 = ratchet.ssl.new(ratchet.ssl.SSLv3_client)
ssl2:load_cas(nil, "cert.pem")

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1, "localhost", 10025)
end)
kernel:loop()

assert(counter == 3)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
