package.path = './temp/opt/bmc/lualib/?.lua;' .. package.path
local lu = require("luaunit")

TestLibmcpp ={}
function TestLibmcpp:setupClass()
end
function TestLibmcpp:teardwonClass()
end
function TestLibmcpp:test()
    local result = os.execute("temp/bin/libmcpp_test")
    if result == true then
        result = 0
    end
    lu.assertEquals(result, 0)
end

os.exit(lu.run())
