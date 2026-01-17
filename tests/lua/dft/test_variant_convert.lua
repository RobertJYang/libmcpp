--[[
Copyright (c) 2026 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
]]

-- variant_convert 测试用例

local lu = require('luaunit')

-- 使用由 init.lua 加载的 C 模块
-- dft 已经在 init.lua 中设为全局变量
local dft = _G.dft

-- 性能测试配置
local PERF_ITERATIONS = 1000  -- 每个测试执行的次数，取平均值

-- 性能阈值配置（单位：微秒）
-- 基于实际测试基准数据（1000次平均值），设置为基准最大值的 3 倍作为安全边界
-- 3 倍可以容忍正常的性能波动，同时能够及时发现性能回退问题
local PERF_THRESHOLDS = {
    basic_type = 0.15,       -- 基本类型：nil, bool, int, float (实测: 0.00-0.03 us)
    simple_string = 3.0,     -- 简单字符串 (实测: 0.02-0.80 us)
    simple_table = 15.0,     -- 简单 table，< 10 个元素的纯数组或纯字典 (实测: 0.17-4.33 us)
    mixed_table = 80.0,      -- 混合 table，同时包含数组和字典部分 (实测: 24.93 us)
    nested_table = 90.0,     -- 嵌套 table (实测: 9.92-28.39 us)
    medium_table = 650.0,    -- 中等 table，10-100 个元素 (实测: 203.10 us)
    large_table = 800.0,     -- 大型 table，> 100 个元素 (实测: 248.50 us)
}

-- 辅助函数：执行多次测试并返回平均值

local function run_perf_test(test_func, iterations)
    iterations = iterations or PERF_ITERATIONS
    local sum_t1, sum_t2 = 0, 0
    local result
    
    for i = 1, iterations do
        local t1, t2, res = test_func()
        sum_t1 = sum_t1 + t1
        sum_t2 = sum_t2 + t2
        result = res
    end
    
    return sum_t1 / iterations, sum_t2 / iterations, result
end

-- 辅助函数：检查性能是否在阈值内
local function assert_performance(t1, t2, threshold, test_name)
    local total = t1 + t2
    
    -- 时间应该是非负数
    lu.assertTrue(t1 >= 0, string.format("%s: lua->variant 平均耗时应为非负数", test_name))
    lu.assertTrue(t2 >= 0, string.format("%s: variant->lua 平均耗时应为非负数", test_name))
    
    -- 总耗时应小于阈值
    lu.assertTrue(total < threshold, 
        string.format("%s: 平均总耗时 %.2f us 超过阈值 %.2f us", test_name, total, threshold))
end

-- ============================================================================
-- 测试类：基本功能测试
-- ============================================================================
TestVariantConvertBasic = {}

function TestVariantConvertBasic:test_no_arguments_should_error()
    -- 测试无参数调用应该报错
    local status, err = pcall(function()
        dft.convert_between_variant_lua()
    end)
    lu.assertFalse(status)
    lu.assertNotNil(string.find(err, "需要至少1个参数"))
end

function TestVariantConvertBasic:test_single_nil()
    -- 测试单个 nil 参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(nil)
    end)
    
    -- 验证返回值数量和类型
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsNil(result)
    
    -- 验证性能
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_nil")
end

function TestVariantConvertBasic:test_single_boolean_true()
    -- 测试单个 boolean (true) 参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(true)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertTrue(result)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_boolean_true")
end

function TestVariantConvertBasic:test_single_boolean_false()
    -- 测试单个 boolean (false) 参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(false)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertFalse(result)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_boolean_false")
end

function TestVariantConvertBasic:test_single_integer()
    -- 测试单个整数参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(42)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, 42)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_integer")
end

function TestVariantConvertBasic:test_single_negative_integer()
    -- 测试单个负整数参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(-100)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, -100)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_negative_integer")
end

function TestVariantConvertBasic:test_single_float()
    -- 测试单个浮点数参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(3.14159)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertAlmostEquals(result, 3.14159, 0.00001)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_single_float")
end

function TestVariantConvertBasic:test_single_string()
    -- 测试单个字符串参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua("hello world")
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, "hello world")
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_string, "test_single_string")
end

function TestVariantConvertBasic:test_single_empty_string()
    -- 测试单个空字符串参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua("")
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, "")
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_string, "test_single_empty_string")
end

-- ============================================================================
-- 测试类：Table 转换测试
-- ============================================================================
TestVariantConvertTable = {}

function TestVariantConvertTable:test_single_empty_table()
    -- 测试单个空 table 参数
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua({})
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证是空 table
    local count = 0
    for _ in pairs(result) do
        count = count + 1
    end
    lu.assertEquals(count, 0)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_single_empty_table")
end

function TestVariantConvertTable:test_single_array()
    -- 测试单个数组 table 参数
    local input = {1, 2, 3, 4, 5}
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证数组内容
    lu.assertEquals(#result, 5)
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], 2)
    lu.assertEquals(result[3], 3)
    lu.assertEquals(result[4], 4)
    lu.assertEquals(result[5], 5)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_single_array")
end

function TestVariantConvertTable:test_single_dict()
    -- 测试单个字典 table 参数
    local input = {a = 1, b = "test", c = true}
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证字典内容
    lu.assertEquals(result.a, 1)
    lu.assertEquals(result.b, "test")
    lu.assertTrue(result.c)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_single_dict")
end

function TestVariantConvertTable:test_single_nested_table()
    -- 测试单个嵌套 table 参数
    local input = {
        name = "test",
        nested = {
            value = 42,
            inner = {
                flag = true
            }
        },
        array = {1, 2, 3}
    }
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证嵌套结构
    lu.assertEquals(result.name, "test")
    lu.assertIsTable(result.nested)
    lu.assertEquals(result.nested.value, 42)
    lu.assertIsTable(result.nested.inner)
    lu.assertTrue(result.nested.inner.flag)
    lu.assertIsTable(result.array)
    lu.assertEquals(#result.array, 3)
    lu.assertEquals(result.array[1], 1)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.nested_table, "test_single_nested_table")
end

function TestVariantConvertTable:test_single_mixed_table()
    -- 测试单个混合（数组+字典）table 参数
    local input = {1, 2, 3, name = "test", value = 42}
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证混合内容
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[2], 2)
    lu.assertEquals(result[3], 3)
    lu.assertEquals(result.name, "test")
    lu.assertEquals(result.value, 42)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.mixed_table, "test_single_mixed_table")
end

-- ============================================================================
-- 测试类：多参数转换测试
-- ============================================================================
TestVariantConvertMultiple = {}

function TestVariantConvertMultiple:test_two_arguments()
    -- 测试两个参数
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(42, "hello")
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证返回数组
    lu.assertEquals(#results, 2)
    lu.assertEquals(results[1], 42)
    lu.assertEquals(results[2], "hello")
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_two_arguments")
end

function TestVariantConvertMultiple:test_three_arguments_mixed_types()
    -- 测试三个不同类型的参数
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(123, "test", true)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证返回数组
    lu.assertEquals(#results, 3)
    lu.assertEquals(results[1], 123)
    lu.assertEquals(results[2], "test")
    lu.assertTrue(results[3])
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_three_arguments_mixed_types")
end

function TestVariantConvertMultiple:test_multiple_with_nil()
    -- 测试包含 nil 的多参数
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(1, nil, "test")
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证返回数组
    lu.assertEquals(#results, 3)
    lu.assertEquals(results[1], 1)
    lu.assertIsNil(results[2])
    lu.assertEquals(results[3], "test")
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_multiple_with_nil")
end

function TestVariantConvertMultiple:test_multiple_with_tables()
    -- 测试包含 table 的多参数
    local input1 = {a = 1, b = 2}
    local input2 = {10, 20, 30}
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(input1, input2, "end")
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证返回数组
    lu.assertEquals(#results, 3)
    
    lu.assertIsTable(results[1])
    lu.assertEquals(results[1].a, 1)
    lu.assertEquals(results[1].b, 2)
    
    lu.assertIsTable(results[2])
    lu.assertEquals(#results[2], 3)
    lu.assertEquals(results[2][1], 10)
    lu.assertEquals(results[2][2], 20)
    lu.assertEquals(results[2][3], 30)
    
    lu.assertEquals(results[3], "end")
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_multiple_with_tables")
end

function TestVariantConvertMultiple:test_many_arguments()
    -- 测试大量参数（10个）
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证返回数组
    lu.assertEquals(#results, 10)
    for i = 1, 10 do
        lu.assertEquals(results[i], i)
    end
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_many_arguments")
end

-- ============================================================================
-- 测试类：边界情况和特殊值测试
-- ============================================================================
TestVariantConvertEdgeCases = {}

function TestVariantConvertEdgeCases:test_zero()
    -- 测试零值
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(0)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, 0)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_zero")
end

function TestVariantConvertEdgeCases:test_large_integer()
    -- 测试大整数
    local large_num = 2147483647  -- INT32_MAX
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(large_num)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, large_num)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_large_integer")
end

function TestVariantConvertEdgeCases:test_very_small_float()
    -- 测试非常小的浮点数
    local small_num = 0.0000001
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(small_num)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertAlmostEquals(result, small_num, 0.00000001)
    assert_performance(t1, t2, PERF_THRESHOLDS.basic_type, "test_very_small_float")
end

function TestVariantConvertEdgeCases:test_unicode_string()
    -- 测试 Unicode 字符串
    local unicode_str = "你好世界 🌍"
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(unicode_str)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, unicode_str)
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_string, "test_unicode_string")
end

function TestVariantConvertEdgeCases:test_long_string()
    -- 测试长字符串（1000 个字符）
    local long_str = string.rep("abcdefghij", 100)  -- 1000 个字符
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(long_str)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertEquals(result, long_str)
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_string, "test_long_string")
end

function TestVariantConvertEdgeCases:test_deep_nested_table()
    -- 测试深度嵌套的 table
    local deep_table = {
        level1 = {
            level2 = {
                level3 = {
                    level4 = {
                        value = "deep"
                    }
                }
            }
        }
    }
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(deep_table)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    lu.assertEquals(result.level1.level2.level3.level4.value, "deep")
    assert_performance(t1, t2, PERF_THRESHOLDS.nested_table, "test_deep_nested_table")
end

function TestVariantConvertEdgeCases:test_table_with_many_keys()
    -- 测试包含大量键的 table（100 个键）
    local big_table = {}
    for i = 1, 100 do
        big_table["key" .. i] = i
    end
    
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(big_table)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证部分键值
    lu.assertEquals(result.key1, 1)
    lu.assertEquals(result.key50, 50)
    lu.assertEquals(result.key100, 100)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.medium_table, "test_table_with_many_keys")
end

-- ============================================================================
-- 测试类：性能相关测试
-- ============================================================================
TestVariantConvertPerformance = {}

function TestVariantConvertPerformance:test_timing_values_are_reasonable()
    -- 测试返回的时间值是合理的
    local input = {a = 1, b = 2, c = 3}
    local t1, t2, _ = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_timing_values_are_reasonable")
end

function TestVariantConvertPerformance:test_repeated_conversions()
    -- 测试重复转换的稳定性
    local input = {x = 10, y = 20, z = 30}
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(input)
    end)
    
    -- 验证转换结果一致
    lu.assertEquals(result.x, 10)
    lu.assertEquals(result.y, 20)
    lu.assertEquals(result.z, 30)
    
    -- 验证平均性能在阈值内
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_repeated_conversions")
end

function TestVariantConvertPerformance:test_large_data_conversion()
    -- 测试大数据量转换（1000 个元素）
    -- 大数据量测试减少执行次数以避免测试时间过长
    local large_array = {}
    for i = 1, 1000 do
        large_array[i] = i
    end
    
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(large_array)
    end, 100)  -- 只执行 100 次，避免测试时间过长
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    lu.assertEquals(#result, 1000)
    
    -- 验证部分数据
    lu.assertEquals(result[1], 1)
    lu.assertEquals(result[500], 500)
    lu.assertEquals(result[1000], 1000)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.large_table, "test_large_data_conversion")
end

-- ============================================================================
-- 测试类：实际使用场景测试
-- ============================================================================
TestVariantConvertRealWorld = {}

function TestVariantConvertRealWorld:test_config_like_structure()
    -- 测试类似配置文件的结构
    local config = {
        server = {
            host = "127.0.0.1",
            port = 8080,
            enabled = true
        },
        database = {
            host = "localhost",
            port = 5432,
            name = "mydb",
            credentials = {
                user = "admin",
                password = "secret"
            }
        },
        features = {"feature1", "feature2", "feature3"}
    }
    
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(config)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证结构完整性
    lu.assertEquals(result.server.host, "127.0.0.1")
    lu.assertEquals(result.server.port, 8080)
    lu.assertTrue(result.server.enabled)
    lu.assertEquals(result.database.host, "localhost")
    lu.assertEquals(result.database.credentials.user, "admin")
    lu.assertEquals(#result.features, 3)
    lu.assertEquals(result.features[1], "feature1")
    
    assert_performance(t1, t2, PERF_THRESHOLDS.nested_table, "test_config_like_structure")
end

function TestVariantConvertRealWorld:test_api_response_like_structure()
    -- 测试类似 API 响应的结构
    local response = {
        status = 200,
        message = "success",
        data = {
            users = {
                {id = 1, name = "Alice", active = true},
                {id = 2, name = "Bob", active = false},
                {id = 3, name = "Charlie", active = true}
            },
            total = 3,
            page = 1
        }
    }
    
    local t1, t2, result = run_perf_test(function()
        return dft.convert_between_variant_lua(response)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(result)
    
    -- 验证结构
    lu.assertEquals(result.status, 200)
    lu.assertEquals(result.message, "success")
    lu.assertEquals(result.data.total, 3)
    lu.assertEquals(#result.data.users, 3)
    lu.assertEquals(result.data.users[1].name, "Alice")
    lu.assertTrue(result.data.users[1].active)
    lu.assertEquals(result.data.users[2].name, "Bob")
    lu.assertFalse(result.data.users[2].active)
    
    assert_performance(t1, t2, PERF_THRESHOLDS.nested_table, "test_api_response_like_structure")
end

function TestVariantConvertRealWorld:test_multiple_function_arguments()
    -- 测试模拟函数多参数调用
    local function_name = "process_data"
    local arg1 = {input = "data"}
    local arg2 = 42
    local arg3 = true
    
    local t1, t2, results = run_perf_test(function()
        return dft.convert_between_variant_lua(function_name, arg1, arg2, arg3)
    end)
    
    lu.assertIsNumber(t1)
    lu.assertIsNumber(t2)
    lu.assertIsTable(results)
    
    -- 验证参数数组
    lu.assertEquals(#results, 4)
    lu.assertEquals(results[1], "process_data")
    lu.assertIsTable(results[2])
    lu.assertEquals(results[2].input, "data")
    lu.assertEquals(results[3], 42)
    lu.assertTrue(results[4])
    
    assert_performance(t1, t2, PERF_THRESHOLDS.simple_table, "test_multiple_function_arguments")
end

-- 返回测试模块
return {
    TestVariantConvertBasic = TestVariantConvertBasic,
    TestVariantConvertTable = TestVariantConvertTable,
    TestVariantConvertMultiple = TestVariantConvertMultiple,
    TestVariantConvertEdgeCases = TestVariantConvertEdgeCases,
    TestVariantConvertPerformance = TestVariantConvertPerformance,
    TestVariantConvertRealWorld = TestVariantConvertRealWorld
}
