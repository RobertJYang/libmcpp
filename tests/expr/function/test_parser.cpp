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

#include "mc/expr/function/parser.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace mc::expr;

namespace {

bool check_string_value(const mc::variant& value, const std::string& expected) {
    return value.get_type() == mc::type_id::string_type && value.as_string() == expected;
}

bool check_int_value(const mc::variant& value, int expected) {
    return value.get_type() == mc::type_id::int32_type &&
           value.as_int32() == static_cast<int32_t>(expected);
}

bool check_double_value(const mc::variant& value, double expected) {
    return value.get_type() == mc::type_id::double_type && value.as_double() == expected;
}

bool check_bool_value(const mc::variant& value, bool expected) {
    return value.get_type() == mc::type_id::bool_type && value.as_bool() == expected;
}

bool check_property_value(const mc::variant& value, const std::string& expected_obj,
                          const std::string& expected_prop, const std::string& expected_full_name, const std::string& expected_type) {
    if (value.get_type() != mc::type_id::object_type) {
        return false;
    }
    const auto& dict = value.as_dict();
    if (dict.size() != 5) { // 现在有5个字段：type, object_name, property_name, full_name, interface
        return false;
    }

    auto obj_it = dict.find("object_name");
    if (obj_it == dict.end() || !check_string_value(obj_it->value, expected_obj)) {
        return false;
    }

    auto prop_it = dict.find("property_name");
    if (prop_it == dict.end() || !check_string_value(prop_it->value, expected_prop)) {
        return false;
    }

    auto full_name_it = dict.find("full_name");
    if (full_name_it == dict.end() || !check_string_value(full_name_it->value, expected_full_name)) {
        return false;
    }

    auto type_it = dict.find("type");
    if (type_it == dict.end() || !check_string_value(type_it->value, expected_type)) {
        return false;
    }

    // 检查interface字段是否存在
    auto interface_it = dict.find("interface");
    if (interface_it == dict.end() || !interface_it->value.is_string()) {
        return false;
    }
    // 对于传统语法，interface应该为空字符串
    // 这里我们不强制检查具体的interface值，只要是字符串类型即可

    return true;
}

bool check_function_call_value(const mc::variant& value, const func_call& expected) {
    if (value.get_type() != mc::type_id::object_type) {
        return false;
    }
    const auto& dict = value.as_dict();
    if (dict.size() != 2) { // func and params
        return false;
    }

    auto func_it = dict.find("func");
    if (func_it == dict.end() || !check_string_value(func_it->value, expected.func)) {
        return false;
    }

    auto params_it = dict.find("params");
    if (params_it == dict.end() || params_it->value.get_type() != mc::type_id::object_type) {
        return false;
    }

    const auto& params_dict = params_it->value.as_dict();
    if (params_dict.size() != expected.params.size()) {
        return false;
    }

    for (const auto& entry : params_dict) {
        auto expected_it = expected.params.find(entry.key.as_string());
        if (expected_it == expected.params.end()) {
            return false;
        }
        if (!param_value_comparator()(entry.value, expected_it->value)) {
            return false;
        }
    }

    return true;
}

TEST(PropertyTest, ParseBasicProperty) {
    auto& parser = func_parser::get_instance();
    auto  prop   = parser.parse_property("CPU.Temperature");
    EXPECT_EQ(prop.object_name, "CPU");
    EXPECT_EQ(prop.property_name, "Temperature");
}

TEST(PropertyTest, ParseSyncProperty) {
    auto& parser = func_parser::get_instance();
    auto  prop   = parser.parse_sync_property("<=/CPU.Temperature");
    EXPECT_EQ(prop.type, "sync");
    EXPECT_EQ(prop.object_name, "CPU");
    EXPECT_EQ(prop.property_name, "Temperature");
}

TEST(PropertyTest, ParseRefProperty) {
    auto& parser = func_parser::get_instance();
    auto  prop   = parser.parse_ref_property("#/CPU.Temperature");
    EXPECT_EQ(prop.type, "ref");
    EXPECT_EQ(prop.object_name, "CPU");
    EXPECT_EQ(prop.property_name, "Temperature");
}

TEST(PropertyTest, InvalidPropertyFormat) {
    auto& parser = func_parser::get_instance();
    EXPECT_THROW(parser.parse_property("InvalidFormat"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_sync_property("InvalidFormat"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_property("InvalidFormat"), mc::invalid_arg_exception);
}

TEST(FunctionParserTest, ParseSimpleFunctionCall) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({})");

    EXPECT_EQ(result.func, "Func_test");
    EXPECT_TRUE(result.params.empty());
}

TEST(FunctionParserTest, ParseFunctionCallWithNoParams) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test()");

    EXPECT_EQ(result.func, "Func_test");
    EXPECT_TRUE(result.params.empty());
}

TEST(FunctionParserTest, ParseFunctionCallWithStringParam) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({param: \"value\"})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_string_value(result.params["param"], "value"));
}

TEST(FunctionParserTest, ParseFunctionCallWithIntParam) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({param: 42})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_int_value(result.params["param"], 42));
}

TEST(FunctionParserTest, ParseFunctionCallWithDoubleParam) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({param: 3.14})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_double_value(result.params["param"], 3.14));
}

TEST(FunctionParserTest, ParseFunctionCallWithBoolParam) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({param: true})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_bool_value(result.params["param"], true));
}

TEST(FunctionParserTest, ParseFunctionCallWithPropertyParam) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({param: CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_property_value(result.params["param"], "CPU", "Temperature", "CPU.Temperature", ""));
}

TEST(FunctionParserTest, ParseFunctionCallWithNestedFunctionCall) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_outer({nested: $Func_inner({param: \"value\"})})");

    EXPECT_EQ(result.func, "Func_outer");
    ASSERT_EQ(result.params.size(), 1);

    func_call expected_nested;
    expected_nested.func            = "Func_inner";
    expected_nested.params["param"] = mc::variant("value");

    EXPECT_TRUE(check_function_call_value(result.params["nested"], expected_nested));
}

TEST(FunctionParserTest, ParseFunctionCallWithMultipleParams) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({str: \"value\", num: 42, flag: true})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 3);
    EXPECT_TRUE(check_string_value(result.params["str"], "value"));
    EXPECT_TRUE(check_int_value(result.params["num"], 42));
    EXPECT_TRUE(check_bool_value(result.params["flag"], true));
}

TEST(FunctionParserTest, BasicFunctionCall) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_test({param1: \"value1\", param2: 42, param3: 3.14, param4: true, param5: "
         "$Func_nested({nested_param: \"nested_value\"})})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 5);

    EXPECT_TRUE(check_string_value(result.params["param1"], "value1"));
    EXPECT_TRUE(check_int_value(result.params["param2"], 42));
    EXPECT_TRUE(check_double_value(result.params["param3"], 3.14));
    EXPECT_TRUE(check_bool_value(result.params["param4"], true));

    func_call expected_nested;
    expected_nested.func                   = "Func_nested";
    expected_nested.params["nested_param"] = mc::variant("nested_value");
    EXPECT_TRUE(check_function_call_value(result.params["param5"], expected_nested));
}

TEST(FunctionParserTest, ParseNestedFunctionCallWithIdentifiers) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_TempAlert({temp: CPU.Temperature, health: $Func_TempStatus({x: CPU.Temperature})})");

    EXPECT_EQ(result.func, "Func_TempAlert");
    ASSERT_EQ(result.params.size(), 2);

    EXPECT_TRUE(check_property_value(result.params["temp"], "CPU", "Temperature", "CPU.Temperature", ""));
    EXPECT_TRUE(result.params["health"].get_type() == mc::type_id::object_type);

    const auto& health_dict = result.params["health"].as_dict();
    EXPECT_EQ(health_dict.size(), 2);

    auto func_it = health_dict.find("func");
    EXPECT_NE(func_it, health_dict.end());
    EXPECT_TRUE(check_string_value(func_it->value, "Func_TempStatus"));

    auto params_it = health_dict.find("params");
    EXPECT_NE(params_it, health_dict.end());
    EXPECT_TRUE(params_it->value.get_type() == mc::type_id::object_type);

    const auto& nested_params = params_it->value.as_dict();
    EXPECT_EQ(nested_params.size(), 1);
    EXPECT_TRUE(check_property_value(nested_params.find("x")->value, "CPU", "Temperature", "CPU.Temperature", ""));
}

TEST(FunctionParserTest, ParseFunctionCallWithSyncProperty) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({sync_temp: <=/CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_property_value(result.params["sync_temp"], "CPU", "Temperature", "<=/CPU.Temperature", "sync"));
}

TEST(FunctionParserTest, ParseFunctionCallWithRefProperty) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call("$Func_test({ref_temp: #/CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 1);
    EXPECT_TRUE(check_property_value(result.params["ref_temp"], "CPU", "Temperature", "#/CPU.Temperature", "ref"));
}

TEST(FunctionParserTest, ParseFunctionCallWithMultiplePropertyTypes) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_test({normal: CPU.Temperature, sync: <=/CPU.Temperature, ref: #/CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 3);
    EXPECT_TRUE(check_property_value(result.params["normal"], "CPU", "Temperature", "CPU.Temperature", ""));
    EXPECT_TRUE(check_property_value(result.params["sync"], "CPU", "Temperature", "<=/CPU.Temperature", "sync"));
    EXPECT_TRUE(check_property_value(result.params["ref"], "CPU", "Temperature", "#/CPU.Temperature", "ref"));
}

TEST(FunctionParserTest, ParseNestedFunctionCallWithProperties) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_outer({nested: $Func_inner({normal: CPU.Temperature, sync: "
         "<=/CPU.Temperature, ref: #/CPU.Temperature})})");

    EXPECT_EQ(result.func, "Func_outer");
    ASSERT_EQ(result.params.size(), 1);

    func_call expected_nested;
    expected_nested.func = "Func_inner";
    auto prop            = mc::dict();
    prop.insert("object_name", mc::variant("CPU"));
    prop.insert("property_name", mc::variant("Temperature"));
    prop.insert("full_name", mc::variant("CPU.Temperature"));
    prop.insert("type", mc::variant(""));
    prop.insert("interface", mc::variant(""));
    expected_nested.params["normal"] = mc::variant(prop);

    prop = mc::dict();
    prop.insert("object_name", mc::variant("CPU"));
    prop.insert("property_name", mc::variant("Temperature"));
    prop.insert("full_name", mc::variant("<=/CPU.Temperature"));
    prop.insert("type", mc::variant("sync"));
    prop.insert("interface", mc::variant(""));
    expected_nested.params["sync"] = mc::variant(prop);

    prop = mc::dict();
    prop.insert("object_name", mc::variant("CPU"));
    prop.insert("property_name", mc::variant("Temperature"));
    prop.insert("full_name", mc::variant("#/CPU.Temperature"));
    prop.insert("type", mc::variant("ref"));
    prop.insert("interface", mc::variant(""));
    expected_nested.params["ref"] = mc::variant(prop);

    EXPECT_TRUE(check_function_call_value(result.params["nested"], expected_nested));
}

TEST(FunctionParserTest, ParseFunctionCallWithMixedParameters) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_test({str: \"value\", num: 42, flag: true, normal: CPU.Temperature, sync: "
         "<=/CPU.Temperature, ref: #/CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 6);
    EXPECT_TRUE(check_string_value(result.params["str"], "value"));
    EXPECT_TRUE(check_int_value(result.params["num"], 42));
    EXPECT_TRUE(check_bool_value(result.params["flag"], true));
    EXPECT_TRUE(check_property_value(result.params["normal"], "CPU", "Temperature", "CPU.Temperature", ""));
    EXPECT_TRUE(check_property_value(result.params["sync"], "CPU", "Temperature", "<=/CPU.Temperature", "sync"));
    EXPECT_TRUE(check_property_value(result.params["ref"], "CPU", "Temperature", "#/CPU.Temperature", "ref"));
}

// 测试 relate_property 结构体的 variant 转换
TEST(RelatePropertyTest, VariantConversion) {
    relate_property prop;
    prop.type          = "ref";
    prop.object_name   = "CPU";
    prop.property_name = "Temperature";
    prop.full_name     = "#/CPU.Temperature";
    prop.interface     = ""; // 添加interface字段

    // 测试 to_variant
    mc::variant v;
    to_variant(prop, v);

    EXPECT_TRUE(v.is_dict());
    const auto& dict = v.as_dict();
    EXPECT_EQ(dict.size(), 5); // 现在有5个字段
    EXPECT_EQ(dict["type"].as_string(), "ref");
    EXPECT_EQ(dict["object_name"].as_string(), "CPU");
    EXPECT_EQ(dict["property_name"].as_string(), "Temperature");
    EXPECT_EQ(dict["full_name"].as_string(), "#/CPU.Temperature");
    EXPECT_EQ(dict["interface"].as_string(), "");

    // 测试 from_variant
    relate_property prop2;
    from_variant(v, prop2);

    EXPECT_EQ(prop2.type, "ref");
    EXPECT_EQ(prop2.object_name, "CPU");
    EXPECT_EQ(prop2.property_name, "Temperature");
    EXPECT_EQ(prop2.full_name, "#/CPU.Temperature");
    EXPECT_EQ(prop2.interface, "");
}

// 测试空的 relate_property 转换
TEST(RelatePropertyTest, EmptyVariantConversion) {
    relate_property prop;

    // 测试空结构体的 to_variant
    mc::variant v;
    to_variant(prop, v);

    EXPECT_TRUE(v.is_dict());
    const auto& dict = v.as_dict();
    EXPECT_EQ(dict.size(), 5); // 现在有5个字段
    EXPECT_EQ(dict["type"].as_string(), "");
    EXPECT_EQ(dict["object_name"].as_string(), "");
    EXPECT_EQ(dict["property_name"].as_string(), "");
    EXPECT_EQ(dict["full_name"].as_string(), "");
    EXPECT_EQ(dict["interface"].as_string(), "");
}

// 测试不完整字典的 from_variant
TEST(RelatePropertyTest, PartialVariantConversion) {
    mc::dict dict;
    dict["type"]        = "sync";
    dict["object_name"] = "Memory";
    // 缺少 property_name 和 full_name

    mc::variant     v = dict;
    relate_property prop;
    from_variant(v, prop);

    EXPECT_EQ(prop.type, "sync");
    EXPECT_EQ(prop.object_name, "Memory");
    EXPECT_EQ(prop.property_name, ""); // 应该为空
    EXPECT_EQ(prop.full_name, "");     // 应该为空
}

// 测试非字典类型的 from_variant
TEST(RelatePropertyTest, InvalidVariantConversion) {
    mc::variant     v("not a dict");
    relate_property prop;
    prop.type = "original";

    // 由于反射系统的实现，当传入非字典类型时会抛出异常
    // 这是预期的行为
    EXPECT_THROW(from_variant(v, prop), mc::bad_cast_exception);
}

// 测试各种属性类型的解析
TEST(PropertyParserTest, ParseDifferentPropertyTypes) {
    auto& parser = func_parser::get_instance();

    // 测试普通属性（无前缀）
    auto normal_prop = parser.parse_property("CPU.Temperature");
    EXPECT_EQ(normal_prop.object_name, "CPU");
    EXPECT_EQ(normal_prop.property_name, "Temperature");
    EXPECT_EQ(normal_prop.full_name, "CPU.Temperature");
    EXPECT_TRUE(normal_prop.type.empty()); // 普通属性没有type

    // 测试同步属性
    auto sync_prop = parser.parse_sync_property("<=/CPU.Temperature");
    EXPECT_EQ(sync_prop.type, "sync");
    EXPECT_EQ(sync_prop.object_name, "CPU");
    EXPECT_EQ(sync_prop.property_name, "Temperature");
    EXPECT_EQ(sync_prop.full_name, "<=/CPU.Temperature");

    // 测试引用属性
    auto ref_prop = parser.parse_ref_property("#/CPU.Temperature");
    EXPECT_EQ(ref_prop.type, "ref");
    EXPECT_EQ(ref_prop.object_name, "CPU");
    EXPECT_EQ(ref_prop.property_name, "Temperature");
    EXPECT_EQ(ref_prop.full_name, "#/CPU.Temperature");
}

// 测试复杂的对象属性名
TEST(PropertyParserTest, ParseComplexPropertyNames) {
    auto& parser = func_parser::get_instance();

    // 测试包含数字的对象名
    auto prop1 = parser.parse_property("CPU01.Temperature");
    EXPECT_EQ(prop1.object_name, "CPU01");
    EXPECT_EQ(prop1.property_name, "Temperature");

    // 测试包含下划线的属性名
    auto prop2 = parser.parse_property("Memory.Max_Usage");
    EXPECT_EQ(prop2.object_name, "Memory");
    EXPECT_EQ(prop2.property_name, "Max_Usage");

    // 测试长名称
    auto prop3 = parser.parse_property("NetworkAdapter_Ethernet.PacketCount");
    EXPECT_EQ(prop3.object_name, "NetworkAdapter_Ethernet");
    EXPECT_EQ(prop3.property_name, "PacketCount");
}

// 测试边界和错误情况
TEST(PropertyParserTest, ParseErrorCases) {
    auto& parser = func_parser::get_instance();

    // 测试无效格式的属性（没有点分隔符）
    EXPECT_THROW(parser.parse_property("InvalidProperty"), std::exception);

    // 测试空字符串
    EXPECT_THROW(parser.parse_property(""), std::exception);

    // 测试只有点的字符串
    EXPECT_THROW(parser.parse_property("."), std::exception);

    // 测试多个点的字符串
    EXPECT_THROW(parser.parse_property("A.B.C"), std::exception);

    // 测试同步属性无效格式
    EXPECT_THROW(parser.parse_sync_property("Invalid"), std::exception);
    EXPECT_THROW(parser.parse_sync_property("<=/"), std::exception);

    // 测试引用属性无效格式
    EXPECT_THROW(parser.parse_ref_property("Invalid"), std::exception);
    EXPECT_THROW(parser.parse_ref_property("#/"), std::exception);
}

// 测试函数解析的边界情况
TEST(FunctionParserTest, ParseEdgeCases) {
    auto& parser = func_parser::get_instance();

    // 测试无参数函数
    auto result1 = parser.parse_function_call("$Func_Test()");
    EXPECT_EQ(result1.func, "Func_Test");
    EXPECT_TRUE(result1.params.empty());

    // 测试空花括号参数
    auto result2 = parser.parse_function_call("$Func_Test({})");
    EXPECT_EQ(result2.func, "Func_Test");
    EXPECT_TRUE(result2.params.empty());

    // 测试只有空格的参数
    auto result3 = parser.parse_function_call("$Func_Test({   })");
    EXPECT_EQ(result3.func, "Func_Test");
    EXPECT_TRUE(result3.params.empty());

    // 测试单个参数
    auto result4 = parser.parse_function_call("$Func_Test({param: \"value\"})");
    EXPECT_EQ(result4.func, "Func_Test");
    EXPECT_EQ(result4.params.size(), 1);
    EXPECT_EQ(result4.params["param"].as_string(), "value");
}

// 测试参数类型推断
TEST(FunctionParserTest, ParseParameterTypes) {
    auto& parser = func_parser::get_instance();

    // 测试各种参数类型的推断
    auto result = parser.parse_function_call(
        "$Func_Test({str: \"hello\", num: 42, flag: true, prop: CPU.Temperature})");

    EXPECT_EQ(result.func, "Func_Test");
    EXPECT_EQ(result.params.size(), 4);

    // 检查字符串参数
    EXPECT_TRUE(result.params["str"].is_string());
    EXPECT_EQ(result.params["str"].as_string(), "hello");

    // 检查数值参数
    EXPECT_TRUE(result.params["num"].is_int32());
    EXPECT_EQ(result.params["num"].as_int32(), 42);

    // 检查布尔参数
    EXPECT_TRUE(result.params["flag"].is_bool());
    EXPECT_EQ(result.params["flag"].as_bool(), true);

    // 检查属性参数
    EXPECT_TRUE(result.params["prop"].is_dict());
    const auto& prop_dict = result.params["prop"].as_dict();
    EXPECT_EQ(prop_dict["object_name"].as_string(), "CPU");
    EXPECT_EQ(prop_dict["property_name"].as_string(), "Temperature");
}

// 测试嵌套解析的深度
TEST(FunctionParserTest, ParseNestedDepth) {
    auto& parser = func_parser::get_instance();

    // 测试三层嵌套函数调用
    auto result = parser.parse_function_call(
        "$Func_Level1({param1: $Func_Level2({param2: $Func_Level3({param3: \"deep_value\"})})})");

    EXPECT_EQ(result.func, "Func_Level1");
    EXPECT_EQ(result.params.size(), 1);

    // 检查第二层
    EXPECT_TRUE(result.params["param1"].is_dict());
    const auto& level2_dict = result.params["param1"].as_dict();
    EXPECT_EQ(level2_dict["func"].as_string(), "Func_Level2");

    // 检查第三层
    const auto& level2_params = level2_dict["params"].as_dict();
    EXPECT_TRUE(level2_params["param2"].is_dict());
    const auto& level3_dict = level2_params["param2"].as_dict();
    EXPECT_EQ(level3_dict["func"].as_string(), "Func_Level3");

    // 检查最深层的值
    const auto& level3_params = level3_dict["params"].as_dict();
    EXPECT_EQ(level3_params["param3"].as_string(), "deep_value");
}

// 测试特殊字符和转义
TEST(FunctionParserTest, ParseSpecialCharacters) {
    auto& parser = func_parser::get_instance();

    // 测试包含特殊字符的字符串
    auto result = parser.parse_function_call("$Func_Test({msg: \"Hello, World!\"})");
    EXPECT_EQ(result.params["msg"].as_string(), "Hello, World!");

    // 测试包含数字和符号的函数名
    auto result2 = parser.parse_function_call("$Func_Test_123({value: 42})");
    EXPECT_EQ(result2.func, "Func_Test_123");

    // 测试负数参数
    auto result3 = parser.parse_function_call("$Func_Test({negative: -42})");
    EXPECT_EQ(result3.params["negative"].as_int32(), -42);

    // 测试浮点数参数
    auto result4 = parser.parse_function_call("$Func_Test({decimal: 3.14159})");
    EXPECT_DOUBLE_EQ(result4.params["decimal"].as_double(), 3.14159);
}

// 测试新语法：带接口的属性解析
TEST(FunctionParserTest, ParsePropertyWithInterface) {
    auto& parser = func_parser::get_instance();

    // 测试引用属性的新语法
    auto ref_result = parser.parse_ref_property("#/Device[bmc.dev.TestInterface].Temperature");
    EXPECT_EQ(ref_result.type, "ref");
    EXPECT_EQ(ref_result.object_name, "Device");
    EXPECT_EQ(ref_result.interface, "bmc.dev.TestInterface");
    EXPECT_EQ(ref_result.property_name, "Temperature");
    EXPECT_EQ(ref_result.full_name, "#/Device[bmc.dev.TestInterface].Temperature");

    // 测试同步属性的新语法
    auto sync_result = parser.parse_sync_property("<=/CPU[bmc.hardware.Processor].Usage");
    EXPECT_EQ(sync_result.type, "sync");
    EXPECT_EQ(sync_result.object_name, "CPU");
    EXPECT_EQ(sync_result.interface, "bmc.hardware.Processor");
    EXPECT_EQ(sync_result.property_name, "Usage");
    EXPECT_EQ(sync_result.full_name, "<=/CPU[bmc.hardware.Processor].Usage");

    // 测试传统语法（向后兼容）
    auto traditional_ref = parser.parse_ref_property("#/CPU.Temperature");
    EXPECT_EQ(traditional_ref.type, "ref");
    EXPECT_EQ(traditional_ref.object_name, "CPU");
    EXPECT_EQ(traditional_ref.interface, ""); // 传统语法接口为空
    EXPECT_EQ(traditional_ref.property_name, "Temperature");
    EXPECT_EQ(traditional_ref.full_name, "#/CPU.Temperature");

    auto traditional_sync = parser.parse_sync_property("<=/Memory.Usage");
    EXPECT_EQ(traditional_sync.type, "sync");
    EXPECT_EQ(traditional_sync.object_name, "Memory");
    EXPECT_EQ(traditional_sync.interface, ""); // 传统语法接口为空
    EXPECT_EQ(traditional_sync.property_name, "Usage");
    EXPECT_EQ(traditional_sync.full_name, "<=/Memory.Usage");
}

// 测试普通属性解析的新语法支持
TEST(FunctionParserTest, ParsePropertyWithInterfaceNoPrefix) {
    auto& parser = func_parser::get_instance();

    // 测试不带前缀的新语法
    auto result = parser.parse_property("GPU[bmc.hardware.Graphics].Load");
    EXPECT_EQ(result.object_name, "GPU");
    EXPECT_EQ(result.interface, "bmc.hardware.Graphics");
    EXPECT_EQ(result.property_name, "Load");
    EXPECT_EQ(result.full_name, "GPU[bmc.hardware.Graphics].Load");

    // 测试不带前缀的传统语法
    auto traditional = parser.parse_property("GPU.Load");
    EXPECT_EQ(traditional.object_name, "GPU");
    EXPECT_EQ(traditional.interface, "");
    EXPECT_EQ(traditional.property_name, "Load");
    EXPECT_EQ(traditional.full_name, "GPU.Load");
}

// 测试函数调用中的新语法参数
TEST(FunctionParserTest, ParseFunctionCallWithInterfaceParameters) {
    auto& parser = func_parser::get_instance();
    auto  result = parser.parse_function_call(
        "$Func_test({device_temp: #/Device[bmc.dev.TestInterface].Temperature, "
         "cpu_usage: <=/CPU[bmc.hardware.Processor].Usage, "
         "traditional: #/GPU.Load})");

    EXPECT_EQ(result.func, "Func_test");
    ASSERT_EQ(result.params.size(), 3);

    // 检查带接口的引用属性
    auto device_temp = result.params["device_temp"].as<relate_property>();
    EXPECT_EQ(device_temp.type, "ref");
    EXPECT_EQ(device_temp.object_name, "Device");
    EXPECT_EQ(device_temp.interface, "bmc.dev.TestInterface");
    EXPECT_EQ(device_temp.property_name, "Temperature");

    // 检查带接口的同步属性
    auto cpu_usage = result.params["cpu_usage"].as<relate_property>();
    EXPECT_EQ(cpu_usage.type, "sync");
    EXPECT_EQ(cpu_usage.object_name, "CPU");
    EXPECT_EQ(cpu_usage.interface, "bmc.hardware.Processor");
    EXPECT_EQ(cpu_usage.property_name, "Usage");

    // 检查传统语法
    auto traditional = result.params["traditional"].as<relate_property>();
    EXPECT_EQ(traditional.type, "ref");
    EXPECT_EQ(traditional.object_name, "GPU");
    EXPECT_EQ(traditional.interface, "");
    EXPECT_EQ(traditional.property_name, "Load");
}

// 测试引用对象解析功能
TEST(PropertyParserTest, ParseRefObject) {
    auto& parser = func_parser::get_instance();

    // 测试基本引用对象解析
    auto ref_obj = parser.parse_ref_object("#/CPU");
    EXPECT_EQ(ref_obj.type, "ref");
    EXPECT_EQ(ref_obj.object_name, "CPU");
    EXPECT_EQ(ref_obj.full_name, "#/CPU");

    // 测试不同对象名的引用对象
    auto ref_memory = parser.parse_ref_object("#/Memory_Controller");
    EXPECT_EQ(ref_memory.type, "ref");
    EXPECT_EQ(ref_memory.object_name, "Memory_Controller");
    EXPECT_EQ(ref_memory.full_name, "#/Memory_Controller");

    auto ref_gpu = parser.parse_ref_object("#/GPU_Device");
    EXPECT_EQ(ref_gpu.type, "ref");
    EXPECT_EQ(ref_gpu.object_name, "GPU_Device");
    EXPECT_EQ(ref_gpu.full_name, "#/GPU_Device");
}

// 测试引用对象解析的错误情况
TEST(PropertyParserTest, ParseRefObjectErrors) {
    auto& parser = func_parser::get_instance();

    // 测试无效前缀
    EXPECT_THROW(parser.parse_ref_object("CPU"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_object("/CPU"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_object("#CPU"), mc::invalid_arg_exception);

    // 测试空对象名
    EXPECT_THROW(parser.parse_ref_object("#/"), mc::invalid_arg_exception);

    // 测试无效对象名（包含点号等特殊字符）
    EXPECT_THROW(parser.parse_ref_object("#/CPU.Temperature"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_object("#/CPU-Device"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_object("#/123CPU"), mc::invalid_arg_exception);
    EXPECT_THROW(parser.parse_ref_object("#/CPU@Device"), mc::invalid_arg_exception);

    // 测试有效的对象名（应该成功）
    EXPECT_NO_THROW(parser.parse_ref_object("#/CPU"));
    EXPECT_NO_THROW(parser.parse_ref_object("#/_Memory"));
    EXPECT_NO_THROW(parser.parse_ref_object("#/Device123"));
    EXPECT_NO_THROW(parser.parse_ref_object("#/Memory_Controller"));
}

// 测试函数调用解析中的引用对象vs引用属性区分
TEST(PropertyParserTest, ParseFunctionCallWithRefObjectAndRefProperty) {
    auto& parser = func_parser::get_instance();

    // 测试包含引用对象的函数调用
    std::string func_call_with_ref_obj = "$Func_Test({ref_obj: #/CPU, ref_prop: #/Memory.Usage})";
    auto        result                 = parser.parse_function_call(func_call_with_ref_obj);

    EXPECT_EQ(result.func, "Func_Test");
    EXPECT_EQ(result.params.size(), 2);

    // 验证引用对象参数
    EXPECT_TRUE(result.params.contains("ref_obj"));
    auto ref_obj_variant = result.params["ref_obj"];
    auto ref_obj         = ref_obj_variant.as<mc::expr::relate_object>();
    EXPECT_EQ(ref_obj.type, "ref");
    EXPECT_EQ(ref_obj.object_name, "CPU");
    EXPECT_EQ(ref_obj.full_name, "#/CPU");

    // 验证引用属性参数
    EXPECT_TRUE(result.params.contains("ref_prop"));
    auto ref_prop_variant = result.params["ref_prop"];
    auto ref_prop         = ref_prop_variant.as<mc::expr::relate_property>();
    EXPECT_EQ(ref_prop.type, "ref");
    EXPECT_EQ(ref_prop.object_name, "Memory");
    EXPECT_EQ(ref_prop.property_name, "Usage");
    EXPECT_EQ(ref_prop.full_name, "#/Memory.Usage");
}

// 测试 relate_object 的 variant 转换
TEST(RelateObjectTest, VariantConversion) {
    mc::expr::relate_object obj;
    obj.type        = "ref";
    obj.object_name = "CPU";
    obj.full_name   = "#/CPU";

    // to_variant 测试
    mc::variant v;
    to_variant(obj, v);
    EXPECT_TRUE(v.is_dict());

    auto dict = v.as_dict();
    EXPECT_EQ(dict["type"].as_string(), "ref");
    EXPECT_EQ(dict["object_name"].as_string(), "CPU");
    EXPECT_EQ(dict["full_name"].as_string(), "#/CPU");

    // from_variant 测试
    mc::expr::relate_object obj2;
    from_variant(v, obj2);
    EXPECT_EQ(obj2.type, "ref");
    EXPECT_EQ(obj2.object_name, "CPU");
    EXPECT_EQ(obj2.full_name, "#/CPU");
}

// 测试 relate_object 的部分字段转换
TEST(RelateObjectTest, PartialVariantConversion) {
    mc::dict dict;
    dict["type"]        = "ref";
    dict["object_name"] = "Memory";
    // 缺少 full_name

    mc::variant             v = dict;
    mc::expr::relate_object obj;
    from_variant(v, obj);

    EXPECT_EQ(obj.type, "ref");
    EXPECT_EQ(obj.object_name, "Memory");
    EXPECT_EQ(obj.full_name, ""); // 应该为空
}

// 测试非字典类型的 relate_object from_variant
TEST(RelateObjectTest, InvalidVariantConversion) {
    mc::variant             v("not a dict");
    mc::expr::relate_object obj;
    obj.type = "original";

    // 由于反射系统的实现，当传入非字典类型时会抛出异常
    EXPECT_THROW(from_variant(v, obj), mc::bad_cast_exception);
}

} // namespace