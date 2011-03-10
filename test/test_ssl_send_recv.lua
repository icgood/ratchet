require "ratchet"

counter = 0

function ctx1(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket.SO_REUSEADDR = true
    socket:bind(rec.addr)
    socket:listen()

    kernel:attach(ctx2, "tcp://localhost:10025")

    local client = socket:accept()

    -- Portion being tested.
    --
    local data = client:recv()
    assert(data == "not encrypted")
    client:send("yet")

    local enc = client:encrypt(ssl)
    enc:accept()

    client:send("hello")
    local data = client:recv()
    assert(data == "world")

    local data = client:recv()
    assert(data == "foo")
    client:send("bar")

    counter = counter + 1
end

function ctx2(where)
    local rec = ratchet.socket.prepare_uri(where)
    local socket = ratchet.socket.new(rec.family, rec.socktype, rec.protocol)
    socket:connect(rec.addr)

    -- Portion being tested.
    --
    socket:send("not encrypted")
    local data = socket:recv()
    assert(data == "yet")

    local enc = socket:encrypt(ssl)
    enc:connect()
    enc:check_certificate_chain(rec.host)

    local data = socket:recv()
    assert(data == "hello")
    socket:send("world")

    socket:send("foo")
    local data = socket:recv()
    assert(data == "bar")

    counter = counter + 2
end

ssl = ratchet.ssl.new()
ssl:load_certs("cert.pem")
ssl:load_cas(nil, "cert.pem")
--ssl:load_randomness("/dev/urandom")
--ssl:load_dh_params("dh_param.pem")
--ssl:generate_tmp_rsa()

kernel = ratchet.new()
kernel:attach(ctx1, "tcp://localhost:10025")
kernel:loop()

assert(counter == 3)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
