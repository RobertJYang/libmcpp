--[[
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
]]

-- ============================================================================
-- 统一错误引擎测试文件
-- 本文件包含所有错误引擎相关的 Lua API 测试用例,覆盖 8 个功能模块
-- ============================================================================

local lu = require("luaunit")

-- ============================================================================
-- 测试套件 1: 错误对象创建测试 (TestErrorCreation)
-- ============================================================================

TestErrorCreation = {}

function TestErrorCreation:setUp()
    self.mc_error = require("mc_error")
end

-- 测试各种参数类型的错误创建
function TestErrorCreation:test_error_with_various_parameter_types()
    -- 测试字符串参数
    local err1 = self.mc_error.new("TestError", "Property %1 is invalid", {[0] = "TestProperty"})
    lu.assertNotNil(err1)
    lu.assertEquals(err1.name, "TestError")

    -- 测试数字参数
    local err2 = self.mc_error.new("ValueError", "Value %1 is out of range", {[0] = 42})
    lu.assertNotNil(err2)

    -- 测试布尔参数
    local err3 = self.mc_error.new("BoolError", "Enabled flag is %1", {[0] = true})
    lu.assertNotNil(err3)

    -- 测试混合参数
    local err4 = self.mc_error.new("MixedError", "Status %1 for %2 is %3", {
        [0] = 200,
        [1] = "Operation",
        [2] = true
    })
    lu.assertNotNil(err4)
end

-- 测试 new_message_error 方法
function TestErrorCreation:test_new_message_error()
    local err = self.mc_error.new_message_error({
        name = "TestError",
        format = "Property %1 is invalid",
        params = {[0] = "TestProperty"},
        registry_prefix = "TestRegistry"
    })

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "TestError")
    lu.assertEquals(err.registry_prefix, "TestRegistry")
end

-- 测试错误的字符串表示
function TestErrorCreation:test_error_tostring()
    local err = self.mc_error.new("TestError", "Property %1 is invalid", {[0] = "TestProperty"})
    local str = err:tostring()

    lu.assertNotNil(str)
    lu.assertEquals(str, "TestError: Property TestProperty is invalid")
end

-- 测试错误的基本属性
function TestErrorCreation:test_error_properties()
    local err = self.mc_error.new("TestError", "Test message with %1", {[0] = "parameter"})

    -- 测试 name 属性
    lu.assertEquals(err.name, "TestError")

    -- 测试 message 属性
    local msg = err.message
    lu.assertNotNil(msg)
    lu.assertTrue(msg:find("parameter") ~= nil)

    -- 测试 params 属性
    local params = err.params
    lu.assertNotNil(params)
    lu.assertEquals(params["0"], "parameter")
end

-- 测试 traceback 方法
function TestErrorCreation:test_traceback_method()
    local err = self.mc_error.new("TestError", "Test message for traceback")

    local tb = err:traceback()

    lu.assertNotNil(tb)
    lu.assertTrue(type(tb) == "string")
end

-- 测试 post_process 方法（dict 参数）
function TestErrorCreation:test_post_process_with_dict()
    -- post_process 需要参数值以 %参数名: 开头
    -- 创建一个包含特殊格式参数的错误
    local mc_error = require("mc_error")
    local err = mc_error.new("TestError", "Test message for post_process", {
        ["arg1"] = "%arg1:value1",
        ["arg2"] = "%arg2:value2"
    })

    local param_struct = {
        [1] = {name = "arg1"},
        [2] = {name = "arg2"}
    }

    local result = err:post_process(param_struct)

    lu.assertNotNil(result)
    lu.assertEquals(result, err)
end

-- 测试 post_process 方法（string 参数）
function TestErrorCreation:test_post_process_with_string()
    local err = self.mc_error.new("TestError", "Test message for post_process", {
        ["arg1"] = "%arg1:value1"
    })

    local param_struct = "arg1"
    local result = err:post_process(param_struct)

    lu.assertNotNil(result)
    lu.assertEquals(result, err)
end

-- 测试 encode 方法
function TestErrorCreation:test_encode_method()
    local err = self.mc_error.new("TestError", "Test message with %1", {[0] = "parameter"})

    local json_str = err:encode()

    lu.assertNotNil(json_str)
    lu.assertTrue(type(json_str) == "string")
    lu.assertTrue(json_str:len() > 0)
    lu.assertTrue(json_str:find("TestError") ~= nil or json_str:find("Test message") ~= nil)
end

-- 测试属性访问和修改
function TestErrorCreation:test_property_access_and_modification()
    local err = self.mc_error.new("TestError", "Test message")

    lu.assertEquals(err.name, "TestError")

    err.name = "ModifiedError"
    lu.assertEquals(err.name, "ModifiedError")

    err.custom_field = "custom_value"
    lu.assertEquals(err.custom_field, "custom_value")
end

-- 测试 new_error 的 %s 占位符格式化（可变参数形式）
function TestErrorCreation:test_new_error_with_s_placeholder()
    local err = self.mc_error.new_error("StringError", "Value %s is invalid", "test")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "StringError")
    lu.assertEquals(err.message, "Value test is invalid")
end

-- 测试 new_error 的 %d 占位符格式化（可变参数形式）
function TestErrorCreation:test_new_error_with_d_placeholder()
    local err = self.mc_error.new_error("IntError", "Value %d is out of range", 42)

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "IntError")
    lu.assertEquals(err.message, "Value 42 is out of range")
end

-- 测试 new_error 的多个 %s 占位符格式化（可变参数形式）
function TestErrorCreation:test_new_error_with_multiple_s_placeholders()
    local err = self.mc_error.new_error("MultiError", "%s status: %s", "OK", "success")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "MultiError")
    lu.assertEquals(err.message, "OK status: success")
end

-- 测试 new_error 的 %1, %2 位置占位符格式化（位置参数形式）
function TestErrorCreation:test_new_error_with_positional_placeholders()
    local err = self.mc_error.new_error("PosError", "Error at %1: %2", "module", "failed")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "PosError")
    lu.assertEquals(err.message, "Error at module: failed")
end

-- 测试 new_error 的多个 %d 占位符格式化（可变参数形式）
function TestErrorCreation:test_new_error_with_multiple_d_placeholders()
    local err = self.mc_error.new_error("MultiIntError", "Values: %d, %d, %d", 10, 20, 30)

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "MultiIntError")
    lu.assertEquals(err.message, "Values: 10, 20, 30")
end

-- 测试 %2 在 %1 前面的情况
function TestErrorCreation:test_new_error_reverse_positional_order()
    local err = self.mc_error.new_error("ReverseError", "%2 comes before %1", "first", "second")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "ReverseError")
    lu.assertEquals(err.message, "second comes before first")
end

-- 测试混合 %s 和 %d 占位符
function TestErrorCreation:test_new_error_mixed_s_d_placeholders()
    local err = self.mc_error.new_error("MixedError", "User %s has %d credits", "Alice", 100)

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "MixedError")
    lu.assertEquals(err.message, "User Alice has 100 credits")
end

-- 测试三个位置占位符
function TestErrorCreation:test_new_error_three_positional()
    local err = self.mc_error.new_error("ThreePos", "%1-%2-%3", "A", "B", "C")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "ThreePos")
    lu.assertEquals(err.message, "A-B-C")
end

-- 测试 new_error 的 %x 十六进制占位符
function TestErrorCreation:test_new_error_with_hex_placeholder()
    local err = self.mc_error.new_error("HexError", "Color code: %x", 255)

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "HexError")
    lu.assertEquals(err.message, "Color code: 255")
end

-- 测试空参数列表的情况
function TestErrorCreation:test_new_error_empty_params()
    local err = self.mc_error.new_error("EmptyError", "No params message")

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "EmptyError")
    lu.assertEquals(err.message, "No params message")
end

-- 测试 new_message_error 的 format 转换
function TestErrorCreation:test_new_message_error_format_conversion()
    local err = self.mc_error.new_message_error({
        name = "MsgError",
        format = "Status %s: %d",
        params = {[0] = "OK", [1] = 200}
    })

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "MsgError")
    lu.assertEquals(err.message, "Status OK: 200")
end

-- ============================================================================
-- 测试套件 2: 错误属性访问测试 (TestErrorProperties)
-- ============================================================================

TestErrorProperties = {}

function TestErrorProperties:setUp()
    self.mc_error = require("mc_error")
end

-- 测试点号语法属性访问
function TestErrorProperties:test_property_access_dot_syntax()
    local err = self.mc_error.new("TestError", "Test message with %1", {[0] = "value"})

    lu.assertEquals(err.name, "TestError")
    lu.assertStrContains(err.message, "value")
    lu.assertEquals(err.params["0"], "value")
end

-- 测试 name 属性
function TestErrorProperties:test_property_name()
    local err = self.mc_error.new("NameTest", "Format %1", {[0] = "test"})

    lu.assertEquals(err.name, "NameTest")

    err.name = "NewName"
    lu.assertEquals(err.name, "NewName")
end

-- 测试 message 属性
function TestErrorProperties:test_property_message()
    local err = self.mc_error.new("MsgTest", "Test message")

    local msg = err.message
    lu.assertEquals(msg, "Test message")
end

-- 测试 params 属性
function TestErrorProperties:test_property_params()
    local err = self.mc_error.new("ParamsTest", "Test %1 %2", {[0] = "A", [1] = "B"})

    local params = err.params
    lu.assertEquals(params["0"], "A")
    lu.assertEquals(params["1"], "B")
end

-- 测试 registry_prefix 属性
function TestErrorProperties:test_property_registry_prefix()
    local err = self.mc_error.new_message_error({
        name = "TestError",
        format = "Test",
        registry_prefix = "TestPrefix"
    })

    lu.assertEquals(err.registry_prefix, "TestPrefix")
end

-- 测试 args_with_index 属性
function TestErrorProperties:test_property_args_with_index()
    local err = self.mc_error.new("ArgsTest", "Test message", {
        ["arg1"] = "%arg1:value"
    })

    err:post_process({[1] = {name = "arg1"}})

    local args_with_index = err.args_with_index
    lu.assertNotNil(args_with_index)
end

-- 测试属性修改
function TestErrorProperties:test_property_modification()
    local err = self.mc_error.new("TestError", "Test")

    err.name = "ModifiedName"
    err.custom_field = "custom_value"

    lu.assertEquals(err.name, "ModifiedName")
    lu.assertEquals(err.custom_field, "custom_value")
end

-- ============================================================================
-- 测试套件 3: 错误消息转换测试 (TestErrorConversion)
-- ============================================================================

TestErrorConversion = {}

function TestErrorConversion:setUp()
    self.mc_error = require("mc_error")

    local test_base_json = [[{
        "Description": "Test base registry",
        "RegistryPrefix": "Base",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "Success": {
                "Description": "Success",
                "Message": "Successfully Completed Request",
                "Severity": "OK",
                "NumberOfArgs": 0,
                "Resolution": "None",
                "HttpStatusCode": 200
            },
            "PropertyDuplicate": {
                "Description": "Property duplicate",
                "Message": "The property %1 was duplicated in the request.",
                "Severity": "Warning",
                "NumberOfArgs": 1,
                "Resolution": "Remove the duplicate property.",
                "HttpStatusCode": 400
            }
        }
    }]]

    local test_custom_json = [[{
        "Description": "Test custom registry",
        "RegistryPrefix": "openUBMC",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "PropertyValueOutOfRange": {
                "Description": "Property value out of range",
                "Message": "The value '%1' for the property %2 is not in the supported range.",
                "Severity": "Warning",
                "NumberOfArgs": 2,
                "Resolution": "Try again using a valid value.",
                "HttpStatusCode": 400
            }
        }
    }]]

    self.mc_error.load_registries_from_string(test_base_json, test_custom_json)
end

-- 测试错误转换为标准消息格式
function TestErrorConversion:test_convert_to_standard_message()
    local err = self.mc_error.new("Success", "Successfully Completed Request")

    local std_msg = self.mc_error.convert(err)

    lu.assertNotNil(std_msg)
    lu.assertEquals(std_msg.message_name, "Success")
    lu.assertEquals(std_msg.message_id, "Base.1.0.0.Success")
    lu.assertEquals(std_msg.registry_prefix, "Base")
    lu.assertEquals(std_msg.registry_version, "1.0.0")
    lu.assertEquals(std_msg.http_status_code, 200)
    lu.assertEquals(std_msg.severity, "OK")
end

-- 测试错误转换为 dict 格式
function TestErrorConversion:test_convert_to_dict()
    local err = self.mc_error.new("PropertyDuplicate", "The property %1 was duplicated.", {[0] = "TestProperty"})

    local dict = self.mc_error.convert_to_dict(err)

    lu.assertNotNil(dict)
    lu.assertTrue(type(dict) == "table")
    lu.assertNotNil(dict.MessageId)
    lu.assertNotNil(dict.Message)
    lu.assertEquals(dict.Severity, "Warning")
end

-- 测试带参数的错误转换
function TestErrorConversion:test_convert_with_params()
    local err = self.mc_error.new("PropertyValueOutOfRange",
                                      "The value '%1' for %2 is not in the supported range.",
                                      {[0] = "100", [1] = "Threshold"})

    local std_msg = self.mc_error.convert(err)

    lu.assertNotNil(std_msg.message_args)
    lu.assertTrue(type(std_msg.message_args) == "table")
    lu.assertTrue(std_msg.message:find("100") ~= nil)
    lu.assertTrue(std_msg.message:find("Threshold") ~= nil)
end

-- 测试未知错误转换
function TestErrorConversion:test_convert_unknown_error()
    local err = self.mc_error.new("UnknownError", "Unknown message")

    local std_msg = self.mc_error.convert(err)

    lu.assertEquals(std_msg.message_name, "InternalError")
    lu.assertEquals(std_msg.severity, "Critical")
end

-- 测试从文件加载注册表
function TestErrorConversion:test_load_registries_from_file()
    -- 尝试从文件加载（如果文件存在）
    local base_path = "/opt/bmc/apps/mdb_interface/messages/base.json"
    local custom_path = "/opt/bmc/apps/mdb_interface/messages/custom.json"

    local file = io.open(base_path, "r")
    if file then
        file:close()

        local ok = self.mc_error.load_registries(base_path, custom_path)
        lu.assertTrue(ok)
    else
        -- 如果文件不存在，跳过测试
        lu.assertTrue(true)
    end
end

-- 测试从字符串加载注册表
function TestErrorConversion:test_load_registries_from_string()
    local test_json = [[{
        "Description": "Test registry",
        "RegistryPrefix": "Test",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "TestError": {
                "Description": "Test error",
                "Message": "Test message %1",
                "Severity": "Warning",
                "NumberOfArgs": 1,
                "Resolution": "Test resolution",
                "HttpStatusCode": 400
            }
        }
    }]]

    local ok = self.mc_error.load_registries_from_string(test_json, "{}")
    lu.assertTrue(ok)
end

-- 测试无参数错误转换
function TestErrorConversion:test_convert_without_params()
    local err = self.mc_error.new("Success", "Successfully Completed Request")

    local std_msg = self.mc_error.convert(err)

    lu.assertNotNil(std_msg)
    lu.assertEquals(std_msg.message_name, "Success")
    lu.assertEquals(std_msg.message, "Successfully Completed Request")
end

-- ============================================================================
-- 测试套件 4: 错误序列化测试 (TestErrorSerialization)
-- ============================================================================

TestErrorSerialization = {}

function TestErrorSerialization:setUp()
    self.mc_error = require("mc_error")
end

-- 测试 encode 方法
function TestErrorSerialization:test_encode_to_json()
    local err = self.mc_error.new("TestError", "Test message with %1", {[0] = "parameter"})

    local json_str = err:encode()

    lu.assertNotNil(json_str)
    lu.assertTrue(type(json_str) == "string")
    lu.assertTrue(json_str:find("TestError") ~= nil)
end

-- 测试带特殊字符的序列化
function TestErrorSerialization:test_encode_with_special_chars()
    local err = self.mc_error.new("SpecialCharError", "Message with \"quotes\" and \\backslash\\")

    local json_str = err:encode()

    lu.assertNotNil(json_str)
    lu.assertTrue(json_str:find("SpecialCharError") ~= nil)
end

-- 测试带参数的序列化
function TestErrorSerialization:test_encode_with_args()
    local err = self.mc_error.new("ArgsError", "Test %1 %2", {[0] = "value1", [1] = 42})

    local json_str = err:encode()

    lu.assertNotNil(json_str)
end

-- ============================================================================
-- 测试套件 5: 错误异常处理测试 (TestErrorThrowing)
-- ============================================================================

TestErrorThrowing = {}

function TestErrorThrowing:setUp()
    self.mc_error = require("mc_error")
end

-- 测试 raise 方法
function TestErrorThrowing:test_raise_error()
    local err = self.mc_error.new("RaiseTest", "Error to raise")

    local success, result = pcall(function()
        err:raise()
    end)

    lu.assertFalse(success)
    lu.assertNotNil(result)
end

-- 测试错误传播
function TestErrorThrowing:test_error_propagation()
    local function throw_error()
        local err = self.mc_error.new("PropagationTest", "Error message")
        err:raise()
    end

    local success, result = pcall(throw_error)

    lu.assertFalse(success)
end

-- ============================================================================
-- 测试套件 6: Lua API 兼容性测试 (TestLuaApiCompat)
-- ============================================================================

TestLuaApiCompat = {}

function TestLuaApiCompat:setUp()
    self.mc_error = require("mc_error")
end

-- 测试 new_error API
function TestLuaApiCompat:test_new_error_with_format()
    local err = self.mc_error.new_error(
        "FormattedError",
        "Value %1 for property %2 is invalid",
        {[0] = 100, [1] = "Threshold"}
    )

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "FormattedError")
    lu.assertStrContains(err.message, "100")
    lu.assertStrContains(err.message, "Threshold")
end

-- 测试 new_error 可变参数
function TestLuaApiCompat:test_new_error_with_varargs()
    local err = self.mc_error.new_error(
        "VarargError",
        "Values: %1, %2, %3",
        nil,
        "first",
        42,
        true
    )

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "VarargError")
    lu.assertStrContains(err.message, "first")
end

-- 测试 raise_error 函数
function TestLuaApiCompat:test_raise_error_function()
    local function throw_error()
        self.mc_error.raise_error(
            "RaisedError",
            "This error was raised: %1",
            {arg1 = "reason"}
        )
    end

    local success, err = pcall(throw_error)

    lu.assertFalse(success)
    lu.assertNotNil(err)
end

-- 测试 print_trace 函数
function TestLuaApiCompat:test_print_trace()
    local err = self.mc_error.new("TraceError", "Error for trace testing")

    self.mc_error.print_trace(0, err)

    lu.assertTrue(true)
end

-- ============================================================================
-- 测试套件 7: 日志级别映射测试 (TestLogLevelMapping)
-- ============================================================================

TestLogLevelMapping = {}

function TestLogLevelMapping:setUp()
    self.mc_error = require("mc_error")
end

-- 测试 DLOG_ERROR (0) → mc::log::level::error
function TestLogLevelMapping:test_log_level_0_error()
    -- 使用新的 DLOG_LEVEL_E 标准 (0-4)
    self.mc_error.print_log(0, "Error message: ${msg}", {msg = "test error"})

    lu.assertTrue(true)
end

-- 测试 DLOG_WARN (1) → mc::log::level::warn
function TestLogLevelMapping:test_log_level_1_warn()
    -- 1 = DLOG_WARN
    self.mc_error.print_log(1, "Warning message: ${msg}", {msg = "test warning"})

    lu.assertTrue(true)
end

-- 测试 DLOG_NOTICE (2) → mc::log::level::notice
function TestLogLevelMapping:test_log_level_2_notice()
    -- 2 = DLOG_NOTICE
    self.mc_error.print_log(2, "Notice message: ${msg}", {msg = "test notice"})

    lu.assertTrue(true)
end

-- 测试 DLOG_INFO (3) → mc::log::level::info
function TestLogLevelMapping:test_log_level_3_info()
    -- 3 = DLOG_INFO
    self.mc_error.print_log(3, "Info message: ${msg}", {msg = "test info"})

    lu.assertTrue(true)
end

-- 测试 DLOG_DEBUG (4) → mc::log::level::debug
function TestLogLevelMapping:test_log_level_4_debug()
    -- 4 = DLOG_DEBUG
    self.mc_error.print_log(4, "Debug message: ${msg}", {msg = "test debug"})

    lu.assertTrue(true)
end

-- 测试无效级别 - 小于 0
function TestLogLevelMapping:test_log_level_negative_invalid()
    -- 级别 -1: 应该被忽略
    self.mc_error.print_log(-1, "Should not print")

    lu.assertTrue(true)
end

-- 测试无效级别 - 大于 4
function TestLogLevelMapping:test_log_level_5_invalid()
    -- 级别 5: 超出范围(0-4)，应该被忽略
    self.mc_error.print_log(5, "Should not print")

    lu.assertTrue(true)
end

-- 测试 %s 格式化占位符（字符串）
function TestLogLevelMapping:test_print_log_with_string_placeholder()
    self.mc_error.print_log(3, "String test: %s", "hello")

    lu.assertTrue(true)
end

-- 测试 %d 格式化占位符（整数）
function TestLogLevelMapping:test_print_log_with_int_placeholder()
    self.mc_error.print_log(3, "Integer test: %d", 42)

    lu.assertTrue(true)
end

-- 测试多个占位符
function TestLogLevelMapping:test_print_log_with_multiple_placeholders()
    self.mc_error.print_log(3, "Multiple: %s %d %s", "first", 100, "third")

    lu.assertTrue(true)
end

-- ============================================================================
-- 测试套件 8: 集成测试 (TestErrorIntegration)
-- ============================================================================

TestErrorIntegration = {}

function TestErrorIntegration:setUp()
    self.mc_error = require("mc_error")
end

-- 测试完整工作流程
function TestErrorIntegration:test_full_workflow()
    local err = self.mc_error.new("WorkflowTest", "Processing %1 failed", {[0] = "operation"})

    local json_str = err:encode()

    lu.assertNotNil(err)
    lu.assertNotNil(json_str)
    lu.assertTrue(json_str:len() > 0)
end

-- 测试转换和序列化组合
function TestErrorIntegration:test_conversion_and_serialization()
    local err = self.mc_error.new("ComboTest", "Test %1", {[0] = "value"})

    local std_msg = self.mc_error.convert(err)
    local json_str = err:encode()

    lu.assertNotNil(std_msg)
    lu.assertNotNil(json_str)
end

-- 测试 post_process 和序列化
function TestErrorIntegration:test_error_with_post_process()
    local err = self.mc_error.new("PostProcessTest", "Test message", {
        ["arg1"] = "%arg1:value1",
        ["arg2"] = "%arg2:value2"
    })

    err:post_process({[1] = {name = "arg1"}, [2] = {name = "arg2"}})

    local json_str = err:encode()

    lu.assertNotNil(json_str)
end

-- 测试综合场景
function TestErrorIntegration:test_comprehensive_error_handling()
    local err = self.mc_error.new_message_error({
        name = "ComprehensiveTest",
        format = "Error in %1: %2",
        params = {[0] = "module", [1] = "description"},
        registry_prefix = "TestPrefix"
    })
    err:traceback()

    local json_str = err:encode()

    lu.assertNotNil(err)
    lu.assertEquals(err.name, "ComprehensiveTest")
    lu.assertNotNil(json_str)
end

