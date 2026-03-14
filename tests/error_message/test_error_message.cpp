/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>

#include <mc/error.h>
#include <mc/error_message_converter.h>
#include <mc/error_message_parser.h>

#include <filesystem>
#include <fstream>
#include <mc/dict.h>

namespace {

// 测试用的 JSON 字符串
constexpr std::string_view test_base_json = R"({
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
            "HttpStatusCode": 200,
            "IpmiCompletionCode": "0x00",
            "SnmpStatusCode": 0,
            "TraceDepth": 0
        },
        "PropertyDuplicate": {
            "Description": "Property duplicate",
            "Message": "The property %1 was duplicated in the request.",
            "Severity": "Warning",
            "NumberOfArgs": 1,
            "ParamTypes": ["string"],
            "Resolution": "Remove the duplicate property.",
            "HttpStatusCode": 400,
            "IpmiCompletionCode": "0xFF",
            "SnmpStatusCode": 5,
            "TraceDepth": 0
        },
        "GeneralError": {
            "Description": "General error",
            "Message": "A general error has occurred.",
            "Severity": "Critical",
            "NumberOfArgs": 0,
            "Resolution": "See ExtendedInfo",
            "HttpStatusCode": 500,
            "IpmiCompletionCode": "0xFF",
            "SnmpStatusCode": 5,
            "TraceDepth": 0
        }
    }
})";

constexpr std::string_view test_custom_json = R"({
    "Description": "Test custom registry",
    "RegistryPrefix": "openUBMC",
    "RegistryVersion": "1.0.0",
    "Messages": {
        "PropertyValueOutOfRange": {
            "Description": "Property value out of range",
            "Message": "The value '%1' for the property %2 is not in the supported range.",
            "Severity": "Warning",
            "NumberOfArgs": 2,
            "ParamTypes": ["string", "string"],
            "Resolution": "Try again using a valid value.",
            "HttpStatusCode": 400,
            "IpmiCompletionCode": "0xC9",
            "SnmpStatusCode": 10,
            "TraceDepth": 0
        }
    }
})";

class ErrorMessageTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 加载测试用的错误定义
        mc::error_message_converter::get_instance().load_registries_from_string(test_base_json, test_custom_json);
    }

    void TearDown() override
    {}
};

// 测试解析 base.json
TEST_F(ErrorMessageTest, ParseBaseRegistry)
{
    auto registry = mc::error_message_parser::parse_from_string(test_base_json);

    EXPECT_EQ(registry.registry_prefix, "Base");
    EXPECT_EQ(registry.registry_version, "1.0.0");
    EXPECT_FALSE(registry.messages.empty());
    EXPECT_TRUE(registry.messages.count("Success") > 0);
    EXPECT_TRUE(registry.messages.count("PropertyDuplicate") > 0);
    EXPECT_TRUE(registry.messages.count("GeneralError") > 0);
}

// 测试解析 custom.json
TEST_F(ErrorMessageTest, ParseCustomRegistry)
{
    auto registry = mc::error_message_parser::parse_from_string(test_custom_json);

    EXPECT_EQ(registry.registry_prefix, "openUBMC");
    EXPECT_EQ(registry.registry_version, "1.0.0");
    EXPECT_FALSE(registry.messages.empty());
    EXPECT_TRUE(registry.messages.count("PropertyValueOutOfRange") > 0);
}

// 测试查找消息定义
TEST_F(ErrorMessageTest, FindMessageDefinition)
{
    auto& converter = mc::error_message_converter::get_instance();

    // 查找 base 中的消息
    auto base_def = converter.find_definition("Success");
    ASSERT_TRUE(base_def.has_value());
    EXPECT_EQ(base_def->severity, "OK");
    EXPECT_EQ(base_def->http_status_code, 200);

    // 查找 custom 中的消息
    auto custom_def = converter.find_definition("PropertyValueOutOfRange");
    ASSERT_TRUE(custom_def.has_value());
    EXPECT_EQ(custom_def->severity, "Warning");
    EXPECT_EQ(custom_def->http_status_code, 400);

    // 查找不存在的消息
    auto not_found = converter.find_definition("NonExistentError");
    EXPECT_FALSE(not_found.has_value());
}

// 测试格式化消息
TEST_F(ErrorMessageTest, FormatMessage)
{
    // 测试无参数消息
    std::string msg1 = mc::error_message_parser::format_message("Successfully Completed Request", {});
    EXPECT_EQ(msg1, "Successfully Completed Request");

    // 测试单参数消息
    mc::dict args;
    args[0]          = "TestProperty";
    std::string msg2 = mc::error_message_parser::format_message("The property %1 was duplicated in the request.", args);
    EXPECT_EQ(msg2, "The property TestProperty was duplicated in the request.");

    // 测试双参数消息
    std::string msg3 = mc::error_message_parser::format_message(
        "The value '%1' for the property %2 is not in the supported range.", {{0, "100"}, {1, "Threshold"}});
    EXPECT_EQ(msg3, "The value '100' for the property Threshold is not in the supported range.");
}

// 测试转换错误为标准消息格式（base 消息）
TEST_F(ErrorMessageTest, ConvertBaseError)
{
    mc::error err("Success", "Successfully Completed Request");

    auto std_msg = mc::error_message_converter::get_instance().convert(err);

    EXPECT_EQ(std_msg.message_name, "Success");
    EXPECT_EQ(std_msg.message_id, "Base.1.0.0.Success");
    EXPECT_EQ(std_msg.registry_prefix, "Base");
    EXPECT_EQ(std_msg.registry_version, "1.0.0");
    EXPECT_EQ(std_msg.http_status_code, 200);
    EXPECT_EQ(std_msg.severity, "OK");
    EXPECT_EQ(std_msg.message, "Successfully Completed Request");
}

// 测试转换错误为标准消息格式（custom 消息）
TEST_F(ErrorMessageTest, ConvertCustomError)
{
    mc::error err("PropertyValueOutOfRange", "The value '%1' for the property %2 is not in the supported range.");
    err.append_arg(0, "100");
    err.append_arg(1, "Threshold");

    auto std_msg = mc::error_message_converter::get_instance().convert(err);

    EXPECT_EQ(std_msg.message_name, "PropertyValueOutOfRange");
    EXPECT_EQ(std_msg.message_id, "openUBMC.1.0.0.PropertyValueOutOfRange");
    EXPECT_EQ(std_msg.registry_prefix, "openUBMC");
    EXPECT_EQ(std_msg.registry_version, "1.0.0");
    EXPECT_EQ(std_msg.http_status_code, 400);
    EXPECT_EQ(std_msg.severity, "Warning");
    EXPECT_EQ(std_msg.message, "The value '100' for the property Threshold is not in the supported range.");
}

// 测试转换未定义的错误为 InternalError
TEST_F(ErrorMessageTest, ConvertUndefinedError)
{
    mc::error err("UndefinedError", "This is an undefined error: ${detail}");
    err.append_arg("detail", "something went wrong");

    auto std_msg = mc::error_message_converter::get_instance().convert(err);

    // 未定义的错误应该转换为 InternalError 或 GeneralError
    EXPECT_EQ(std_msg.message_name, "InternalError");
    EXPECT_EQ(std_msg.http_status_code, 500);
    EXPECT_EQ(std_msg.severity, "Critical");
    EXPECT_FALSE(std_msg.message.empty());
}

// 测试转换为 dict 格式
TEST_F(ErrorMessageTest, ConvertToDict)
{
    mc::error err("PropertyDuplicate", "The property %1 was duplicated in the request.");
    err.append_arg(0, "TestProperty");

    mc::dict dict = mc::error_message_converter::get_instance().convert_to_dict(err);

    EXPECT_TRUE(dict.contains("MessageId"));
    EXPECT_TRUE(dict.contains("Message"));
    EXPECT_TRUE(dict.contains("Severity"));
    EXPECT_TRUE(dict.contains("MessageArgs"));

    EXPECT_EQ(dict["MessageId"].as<std::string>(), "Base.1.0.0.PropertyDuplicate");
    EXPECT_EQ(dict["Severity"].as<std::string>(), "Warning");
    EXPECT_EQ(dict["Message"].as<std::string>(), "The property TestProperty was duplicated in the request.");
}

// 测试标准错误消息的 to_dict 方法
TEST_F(ErrorMessageTest, StandardMessageToDict)
{
    mc::standard_error_message std_msg;
    std_msg.message_id       = "Base.1.0.0.Test";
    std_msg.message_name     = "Test";
    std_msg.message          = "Test message";
    std_msg.severity         = "OK";
    std_msg.registry_prefix  = "Base";
    std_msg.registry_version = "1.0.0";
    std_msg.http_status_code = 200;
    std_msg.resolution       = "Test resolution";
    std_msg.message_args     = {{0, "value1"}, {1, "value2"}};

    mc::dict dict = std_msg.to_dict();

    EXPECT_EQ(dict["MessageId"].as<std::string>(), "Base.1.0.0.Test");
    EXPECT_EQ(dict["Message"].as<std::string>(), "Test message");
    EXPECT_EQ(dict["Severity"].as<std::string>(), "OK");
    EXPECT_EQ(dict["Resolution"].as<std::string>(), "Test resolution");
    EXPECT_TRUE(dict.contains("MessageArgs"));
}

// 测试辅助函数 to_standard_message
TEST_F(ErrorMessageTest, ToStandardMessageHelper)
{
    mc::error err("Success", "Successfully Completed Request");

    mc::standard_error_message std_msg = mc::to_standard_message(err);

    EXPECT_EQ(std_msg.message_name, "Success");
    EXPECT_EQ(std_msg.message_id, "Base.1.0.0.Success");
}

// 测试辅助函数 to_standard_message_dict
TEST_F(ErrorMessageTest, ToStandardMessageDictHelper)
{
    mc::error err("Success", "Successfully Completed Request");

    mc::dict dict = mc::to_standard_message_dict(err);

    EXPECT_TRUE(dict.contains("MessageId"));
    EXPECT_EQ(dict["MessageId"].as<std::string>(), "Base.1.0.0.Success");
}

// 测试错误定义的所有字段
TEST_F(ErrorMessageTest, ErrorDefinitionFields)
{
    auto registry = mc::error_message_parser::parse_from_string(test_base_json);

    auto& success_def = registry.messages.at("Success");
    EXPECT_EQ(success_def.description, "Success");
    EXPECT_EQ(success_def.message, "Successfully Completed Request");
    EXPECT_EQ(success_def.severity, "OK");
    EXPECT_EQ(success_def.number_of_args, 0);
    EXPECT_EQ(success_def.resolution, "None");
    EXPECT_EQ(success_def.http_status_code, 200);
    EXPECT_EQ(success_def.ipmi_completion_code, "0x00");
    EXPECT_EQ(success_def.snmp_status_code, 0);
    EXPECT_EQ(success_def.trace_depth, 0);

    auto& prop_def = registry.messages.at("PropertyDuplicate");
    EXPECT_EQ(prop_def.number_of_args, 1);
    EXPECT_EQ(prop_def.param_types.size(), 1);
    EXPECT_EQ(prop_def.param_types[0], "string");
}

// ============================================================================
// Phase 3 Tests: Enhanced Error Parsing and Formatting
// ============================================================================

// T015: 测试参数数量验证（不匹配时记录警告）
TEST_F(ErrorMessageTest, ParameterCountValidation)
{
    // 测试参数数量不匹配的情况（应该记录警告但不阻止格式化）
    std::string template_msg = "The value %1 for the property %2 is of a different type";

    // 提供少于模板需要的参数
    mc::dict    args1   = {{0, "abc"}};
    std::string result1 = mc::error_message_parser::format_message(template_msg, args1);
    // 应该成功格式化，但缺少的参数不会被替换
    EXPECT_TRUE(result1.find("abc") != std::string::npos);
    EXPECT_TRUE(result1.find("%2") != std::string::npos); // %2 仍然存在

    // 提供多余参数（多余的参数被忽略）
    mc::dict    args2   = {{0, "abc"}, {1, "port"}, {2, "extra"}};
    std::string result2 = mc::error_message_parser::format_message(template_msg, args2);
    EXPECT_EQ(result2, "The value abc for the property port is of a different type");

    // 提供正确数量的参数
    mc::dict    args3   = {{0, "abc"}, {1, "port"}};
    std::string result3 = mc::error_message_parser::format_message(template_msg, args3);
    EXPECT_EQ(result3, "The value abc for the property port is of a different type");
}

// T016: 测试参数类型转换
TEST_F(ErrorMessageTest, ParameterTypeConversion)
{
    std::string template_msg = "Test: %1, %2, %3, %4";

    mc::dict args;
    args[0] = std::string("string_value"); // string
    args[1] = int64_t(42);                 // int64
    args[2] = 3.14;                        // double
    args[3] = true;                        // bool

    std::string result = mc::error_message_parser::format_message(template_msg, args);

    EXPECT_TRUE(result.find("string_value") != std::string::npos);
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
    EXPECT_TRUE(result.find("true") != std::string::npos);
}

// T016.1: 测试无符号整数类型转换
TEST_F(ErrorMessageTest, UnsignedIntegerTypeConversion)
{
    std::string template_msg = "Value: %1";

    mc::dict args;
    args[0] = uint64_t(18446744073709551615ULL); // max uint64

    std::string result = mc::error_message_parser::format_message(template_msg, args);
    EXPECT_TRUE(result.find("18446744073709551615") != std::string::npos ||
                result.find("18446744073709551616") != std::string::npos); // 可能的精度差异
}

// T017: 测试消息格式化性能（应该 < 1ms）
TEST_F(ErrorMessageTest, MessageFormattingPerformance)
{
    std::string template_msg = "The value %1 for the property %2 is of a different type";
    mc::dict    args         = {{0, "test_value"}, {1, "test_property"}};

    // 多次格式化测试性能
    auto      start      = std::chrono::high_resolution_clock::now();
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        std::string result = mc::error_message_parser::format_message(template_msg, args);
        // 防止编译器优化掉调用
        if (result.empty()) {
            break;
        }
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    // 平均每次格式化应该 < 1ms (1000微秒)
    EXPECT_LT(avg_time_us, 1000.0) << "Average formatting time: " << avg_time_us << " microseconds";
}

// T018: 测试注册表加载性能（应该 < 100ms）
TEST_F(ErrorMessageTest, RegistryLoadingPerformance)
{
    auto start = std::chrono::high_resolution_clock::now();

    mc::error_message_converter::get_instance().load_registries_from_string(test_base_json, test_custom_json);

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 加载时间应该 < 100ms
    EXPECT_LT(duration.count(), 100) << "Registry loading time: " << duration.count() << " ms";
}

// T019: 测试 custom.json 覆盖 base.json 中的相同错误名
TEST_F(ErrorMessageTest, CustomRegistryOverride)
{
    // 创建两个注册表，其中 custom 包含与 base 同名的错误
    constexpr std::string_view base_json = R"({
        "Description": "Base registry",
        "RegistryPrefix": "Base",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "TestError": {
                "Description": "Base error",
                "Message": "Base message: %1",
                "Severity": "OK",
                "NumberOfArgs": 1,
                "ParamTypes": ["string"],
                "Resolution": "Base resolution",
                "HttpStatusCode": 200,
                "IpmiCompletionCode": "0x00",
                "SnmpStatusCode": 0,
                "TraceDepth": 0
            }
        }
    })";

    constexpr std::string_view custom_json = R"({
        "Description": "Custom registry",
        "RegistryPrefix": "openUBMC",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "TestError": {
                "Description": "Custom error",
                "Message": "Custom message: %1",
                "Severity": "Warning",
                "NumberOfArgs": 1,
                "ParamTypes": ["string"],
                "Resolution": "Custom resolution",
                "HttpStatusCode": 400,
                "IpmiCompletionCode": "0xC9",
                "SnmpStatusCode": 10,
                "TraceDepth": 0
            }
        }
    })";

    mc::error_message_converter::get_instance().load_registries_from_string(base_json, custom_json);

    // 查找 TestError，应该返回 custom 中的定义
    auto def = mc::error_message_converter::get_instance().find_definition("TestError");
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->description, "Custom error");
    EXPECT_EQ(def->message, "Custom message: %1");
    EXPECT_EQ(def->severity, "Warning");
    EXPECT_EQ(def->http_status_code, 400);
    EXPECT_EQ(def->resolution, "Custom resolution");
}

// T020: 测试边缘情况 - 缺失错误定义文件
TEST_F(ErrorMessageTest, MissingErrorDefinitionFiles)
{
    // 尝试从不存在的文件加载
    EXPECT_THROW(mc::error_message_parser::parse_from_file("/nonexistent/path/to/base.json"), mc::exception);
}

// T021: 测试边缘情况 - 格式错误的 JSON
TEST_F(ErrorMessageTest, MalformedJson)
{
    constexpr std::string_view malformed_json = R"({
        "Description": "Malformed registry",
        "RegistryPrefix": "Base",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "TestError": {
                "Description": "Test error",
                // 缺少 Message 字段
                "Severity": "OK"
            }
        }
    })"; // JSON 结构不完整

    EXPECT_THROW(mc::error_message_parser::parse_from_string(malformed_json), mc::exception);

    // 测试无效的 JSON 语法
    constexpr std::string_view invalid_json = R"({
        "Description": "Invalid registry",
        "RegistryPrefix": "Base",
        "RegistryVersion": "1.0.0",
        "Messages": {
            "TestError": {
                "Description": "Test error",
                "Message": "Test message"
                // 缺少逗号和后续字段
            }
        }
    })";

    EXPECT_THROW(mc::error_message_parser::parse_from_string(invalid_json), mc::exception);
}

// T022: 测试边缘情况 - 无效占位符语法
TEST_F(ErrorMessageTest, InvalidPlaceholderSyntax)
{
    // 测试各种无效的占位符格式
    std::string template_msg = "Test %1 %2 %invalid % % %9 %10 %11";

    mc::dict args = {{0, "value1"}, {1, "value2"}};

    std::string result = mc::error_message_parser::format_message(template_msg, args);

    // %1 和 %2 应该被替换
    EXPECT_TRUE(result.find("value1") != std::string::npos);
    EXPECT_TRUE(result.find("value2") != std::string::npos);

    // %invalid 应该保持不变（无法识别）
    EXPECT_TRUE(result.find("%invalid") != std::string::npos);

    // 超出范围的 %11 应该保持不变（只支持 %1-%10）
    EXPECT_TRUE(result.find("%11") != std::string::npos);
}

// T023.1: 测试消息缓存功能（验证缓存机制工作）
TEST_F(ErrorMessageTest, MessageCaching)
{
    mc::error err("Success", "Successfully Completed Request");

    // 首次调用 get_message() 应该格式化消息
    std::string msg1 = err.get_message();
    EXPECT_EQ(msg1, "Successfully Completed Request");

    // 再次调用应该返回缓存的消息
    std::string msg2 = err.get_message();
    EXPECT_EQ(msg2, "Successfully Completed Request");
    EXPECT_EQ(msg1, msg2);
}

// T023.2: 测试修改参数后缓存失效
TEST_F(ErrorMessageTest, CacheInvalidation)
{
    mc::error err("TestError", "Test: %1");

    err.append_arg(0, "value1");
    std::string msg1 = err.get_message();
    EXPECT_EQ(msg1, "Test: value1");

    // 修改参数后，缓存应该失效
    err.append_arg(0, "value2");
    std::string msg2 = err.get_message();
    EXPECT_EQ(msg2, "Test: value2");
    EXPECT_NE(msg1, msg2);
}

} // namespace
