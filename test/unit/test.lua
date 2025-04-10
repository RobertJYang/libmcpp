package.path = './temp/opt/bmc/lualib/?.lua;' .. package.path
local lu = require("luaunit")
print("--------------libmcpp test start--------------")
local result = os.execute("temp/bin/libmcpp_test")
print("test result:", result)
lu.assertEquals(result, 0)
print("--------------libmcpp test end--------------")