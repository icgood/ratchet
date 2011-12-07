require "ratchet"

function ctx1 ()
    r:set_error_handler(on_thread_error, "error thread")
    error("uh oh")
end

function on_thread_error(str, err)
    assert("error thread" == str)
    assert("uh oh" == tostring(err):sub(-5))
    error_happened = true
end

function on_error(err)
    error("thread error handler did not override global one.")
end

r = ratchet.kernel.new()
r:set_error_handler(on_error)
r:attach(ctx1)
r:loop()
assert(error_happened)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
