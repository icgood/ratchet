
require "ratchet"

local encode = ratchet.base64.encode
local decode = ratchet.base64.decode

local all_256 = {}
for i=0, 255 do
    table.insert(all_256, string.char(i))
end

local translation_table = {
    [""] = "",

    ["A"] = "QQ==",

    ["AB"] = "QUI=",

    ["ABC"] = "QUJD",

    ["ABCD"] = "QUJDRA==",

    [string.rep("ABC", 1000)] = string.rep("QUJD", 1000),

    [table.concat(all_256)] = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==",

}

for f, t in pairs(translation_table) do
    assert(t == encode(f), ("[\"%s\"] == encode(\"%s\") (expected [\"%s\"])"):format(encode(f), f, t))
    assert(f == decode(t), ("[\"%s\"] == decode(\"%s\") (expected [\"%s\"])"):format(decode(t), t, f))
end

-- vim:et:fdm=marker:sts=4:sw=4:ts=4
