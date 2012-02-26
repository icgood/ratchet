require "ratchet"

function error_thread()
    error("uh oh")
end

function waiting_thread(e)
    ratchet.thread.wait_all({e})
    set_after_error = true
end

kernel = ratchet.new(function ()
    local e = ratchet.thread.attach(error_thread)
    ratchet.thread.attach(waiting_thread, e)
end)

local worked, err = pcall(kernel.loop, kernel)

assert(not worked)
assert(err:match('uh oh'), "error did not match as expected.")
assert(not set_after_error)
assert(1 == kernel:get_num_threads())

kernel:loop()

assert(set_after_error == true)
assert(0 == kernel:get_num_threads())

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
