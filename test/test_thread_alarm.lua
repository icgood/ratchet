
require "ratchet"

function ctx1()
    ratchet.thread.alarm(0.0, function ()
        ran_alarm_function = true
    end)
    ratchet.thread.timer(1.0)
end

function ctx2()
    ratchet.thread.alarm(0.0, function ()
        local err = ratchet.error.new("Test cleanup error", "CLEANUP")
        error(err)
    end)
    ratchet.thread.timer(1.0)
end

kernel = ratchet.new(function ()
    ratchet.thread.attach(ctx1)
    ratchet.thread.attach(ctx2)
    
end, function (err, thread)
    if ratchet.error.is(err, "ALARM") then
        got_alarm_error = true
    elseif ratchet.error.is(err, "CLEANUP") then
        got_cleanup_error = true
    end
end)
kernel:loop()

assert(ran_alarm_function)
assert(got_alarm_error)
assert(got_cleanup_error)

-- vim:et:fdm=marker:sts=4:sw=4:ts=4
