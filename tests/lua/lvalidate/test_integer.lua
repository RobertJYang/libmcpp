local lvalidate = require("lvalidate")
local lu = require('luaunit')

TestIntegerValidation = {}

function TestIntegerValidation:setUp()
end

function TestIntegerValidation:tearDown()
end

-- 测试类型检查
function TestIntegerValidation:testValidInteger()
    -- 有效整数测试
    local success = pcall(lvalidate.check_integer, "Prop1", 42, 0, 100, false)
    assert(success, "有效整数应该通过验证")
    
    -- 边界值
    success = pcall(lvalidate.check_integer, "Prop1", 0, 0, 100, false)
    assert(success, "边界最小值应该通过验证")
    
    success = pcall(lvalidate.check_integer, "Prop1", 100, 0, 100, false)
    assert(success, "边界最大值应该通过验证")
end

function TestIntegerValidation:testNonInteger()
    -- 浮点数（非整数）
    local success, err = pcall(lvalidate.check_integer, "Prop1", 3.14, 0, 10, false)
    assert(not success, "浮点数应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
    
    -- 浮点数（非整数）但有小数部分
    success, err = pcall(lvalidate.check_integer, "Prop1", 5.5, 0, 10, false)
    assert(not success, "有小数部分的浮点数应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
end

function TestIntegerValidation:testNonNumber()
    -- 字符串
    local success, err = pcall(lvalidate.check_integer, "Prop1", "abc", 0, 10, false)
    assert(not success, "字符串应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
    
    -- 字符串数字
    success, err = pcall(lvalidate.check_integer, "Prop1", "42", 0, 100, false)
    assert(not success, "字符串数字应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
    
    -- nil
    success, err = pcall(lvalidate.check_integer, "Prop1", nil, 0, 10, false)
    assert(not success, "nil应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
    
    -- 布尔值
    success, err = pcall(lvalidate.check_integer, "Prop1", true, 0, 10, false)
    assert(not success, "布尔值应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
    
    -- 表
    success, err = pcall(lvalidate.check_integer, "Prop1", {}, 0, 10, false)
    assert(not success, "表应该抛出类型错误")
    assert(string.find(err, "PropertyValueTypeError"), "应该是类型错误")
end

function TestIntegerValidation:testOutOfRange()
    -- 超出范围
    local success, err = pcall(lvalidate.check_integer, "Prop1", 150, 0, 100, false)
    assert(not success, "超出最大值应该抛出范围错误")
    assert(string.find(err, "PropertyValueOutOfRange"), "应该是范围错误")
    
    success, err = pcall(lvalidate.check_integer, "Prop1", -10, 0, 100, false)
    assert(not success, "小于最小值应该抛出范围错误")
    assert(string.find(err, "PropertyValueOutOfRange"), "应该是范围错误")
end

function TestIntegerValidation:testInvalidRange()
    -- min和max不是数字
    local success, err = pcall(lvalidate.check_integer, "Prop1", 50, "abc", 100, false)
    assert(not success, "min不是数字应该抛出范围错误")
    assert(string.find(err, "PropertyValueOutOfRange"), "应该是范围错误")
    
    success, err = pcall(lvalidate.check_integer, "Prop1", 50, 0, "xyz", false)
    assert(not success, "max不是数字应该抛出范围错误")
    assert(string.find(err, "PropertyValueOutOfRange"), "应该是范围错误")
end

function TestIntegerValidation:testNeedConvert()
    -- need_convert为true的测试
    local success, err = pcall(lvalidate.check_integer, "Prop1", 150, 0, 100, true)
    assert(not success, "超出范围应该抛出范围错误")
    -- 检查错误信息中是否包含转换后的格式
    assert(string.find(err, "%%Prop1:150"), "错误信息应该包含转换后的值")
    assert(string.find(err, "%%Prop1"), "错误信息应该包含转换后的属性名")
end

function TestIntegerValidation:testEdgeCases()
    -- 负范围
    local success = pcall(lvalidate.check_integer, "Prop1", -5, -10, 0, false)
    assert(success, "负范围内的值应该通过验证")
    
    success = pcall(lvalidate.check_integer, "Prop1", 0, -10, 10, false)
    assert(success, "跨0范围应该通过验证")
    
    -- 相等边界
    success = pcall(lvalidate.check_integer, "Prop1", 5, 5, 5, false)
    assert(success, "相等边界应该通过验证")
end

function TestIntegerValidation:testMinGreaterThanMax()
    -- 如果min > max，任何值都不在范围内
    local success, err = pcall(lvalidate.check_integer, "Prop1", 5, 10, 0, false)
    assert(not success, "当min > max时，任何值都应该超出范围")
    assert(string.find(err, "PropertyValueOutOfRange"), "应该是范围错误")
end

-- 运行测试
os.exit(lu.LuaUnit.run())