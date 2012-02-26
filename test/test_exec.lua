require "ratchet"

tests = 0

function hello_world()
    local p = ratchet.exec.new({"echo", "hello", "world"})
    p:start()
    p:stdin():close()
    local line = p:stdout():read()
    line = line:gsub("%\r?%\n$", "")
    assert("hello world" == line)
    assert(0 == p:wait())

    tests = tests + 2
end

function cat_test()
    local p = ratchet.exec.new({"cat"})
    p:start()
    p:stdin():write("test\n")
    p:stdin():close()
    local line = p:stdout():read()
    line = line:gsub("%\r?%\n$", "")
    assert("test" == line)
    assert(0 == p:wait())

    tests = tests + 1
end

function communicate_test()
    local p = ratchet.exec.new({"cat"})
    local out, err, status = p:communicate("testing")
    assert("testing" == out)
    assert("" == err)
    assert(0 == status)

    tests = tests + 4
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(hello_world)
    ratchet.thread.attach(hello_world)
    ratchet.thread.attach(cat_test)
    ratchet.thread.attach(communicate_test)
end)
kernel:loop()

assert(tests == 9)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
