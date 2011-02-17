require "ratchet"

function uri_parser_1(uri)
    return uri:reverse()
end

function uri_parser_2(uri)
    return {uri = uri}
end

u1 = ratchet.uri.new()
u2 = ratchet.uri.new("test")

u1:register("schema", uri_parser_1)
u2:register("test", uri_parser_2)

assert("agoo" == u1("schema:ooga"))

ret = u2("test:beep")
assert("beep" == ret.uri)

succeeded, msg = pcall(u2, "schema:bang")
assert(not succeeded)

-- vim:foldmethod=marker:sw=4:ts=4:sts=4:et:
