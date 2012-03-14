
require "ratchet"

kernel = ratchet.new(function ()
    ratchet.thread.alarm(0.0, function ()
        ran_alarm_function = true
    end)
    ratchet.thread.timer(1.0)

end, function (err, thread)
    got_alarm_error = ratchet.error.is(err, "ALARM")
end)
kernel:loop()

assert(ran_alarm_function)
assert(got_alarm_error)

-- vim:et:fdm=marker:sts=4:sw=4:ts=4
