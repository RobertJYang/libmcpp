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

#include "mc/expr/function/call.h"
#include "mc/expr/function/collection.h"
#include "mc/expr/function/parser.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace mc::expr;

namespace {

TEST(FunctionCallTest, BasicUsage) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建函数定义
    mc::dict args = {
        {"x", mc::variant(1)}, // 无法转换为数值的字符串
        {"y", mc::variant(10)} // 数值
    };
    func add_func("x + y", args);
    functions.insert("$Func_Test", mc::variant(add_func));

    func_collection::get_instance().add("01", nullptr, functions);
    // 创建函数字典

    // 创建函数调用
    func_call fc;
    fc.func = "$Func_Test";
    fc.params.insert("x", mc::variant(2));

    // 执行函数调用
    mc::variant result = add_func.call("01", fc.params);
    EXPECT_EQ(result.as_string(), "12");
}

// 测试 get_relate_properties 方法
TEST(FunctionCallTest, GetRelatePropertiesBasic) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建包含 relate_property 的函数参数
    mc::mutable_dict relate_prop_dict;
    relate_prop_dict["type"]          = "ref";
    relate_prop_dict["object_name"]   = "CPU";
    relate_prop_dict["property_name"] = "Temperature";
    relate_prop_dict["full_name"]     = "CPU.Temperature";
    relate_prop_dict["interface"]     = ""; // 添加interface字段

    mc::dict args = {
        {"prop_param", relate_prop_dict}};

    func test_func("prop_param", args);
    functions.insert("$Func_Test", mc::variant(test_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 测试参数
    mc::mutable_dict params;
    // 不传入参数，使用默认值

    auto result = test_func.get_relate_properties("01", params);

    // 应该返回默认参数中的 relate_property
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.contains("CPU.Temperature"));
}

// 测试带接口的 relate_properties 处理
TEST(FunctionCallTest, GetRelatePropertiesWithInterface) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建包含带接口的 relate_property 的函数参数
    mc::mutable_dict interface_prop_dict;
    interface_prop_dict["type"]          = "ref";
    interface_prop_dict["object_name"]   = "Device";
    interface_prop_dict["property_name"] = "Temperature";
    interface_prop_dict["interface"]     = "bmc.dev.TestInterface";
    interface_prop_dict["full_name"]     = "#/Device[bmc.dev.TestInterface].Temperature";

    mc::mutable_dict traditional_prop_dict;
    traditional_prop_dict["type"]          = "ref";
    traditional_prop_dict["object_name"]   = "CPU";
    traditional_prop_dict["property_name"] = "Usage";
    traditional_prop_dict["interface"]     = ""; // 传统语法接口为空
    traditional_prop_dict["full_name"]     = "#/CPU.Usage";

    mc::dict args = {
        {"interface_prop", interface_prop_dict},
        {"traditional_prop", traditional_prop_dict}
    };

    func test_func("interface_prop + traditional_prop", args);
    functions.insert("$Func_Test", mc::variant(test_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 测试参数
    mc::mutable_dict params;
    // 不传入参数，使用默认值

    auto result = test_func.get_relate_properties("01", params);

    // 应该返回两个 relate_property
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result.contains("Device[bmc.dev.TestInterface].Temperature"));
    EXPECT_TRUE(result.contains("CPU.Usage"));
    
    // 验证带接口的属性信息
    auto interface_result = result["Device[bmc.dev.TestInterface].Temperature"].as<relate_property>();
    EXPECT_EQ(interface_result.object_name, "Device");
    EXPECT_EQ(interface_result.interface, "bmc.dev.TestInterface");
    EXPECT_EQ(interface_result.property_name, "Temperature");
    
    // 验证传统语法的属性信息
    auto traditional_result = result["CPU.Usage"].as<relate_property>();
    EXPECT_EQ(traditional_result.object_name, "CPU");
    EXPECT_EQ(traditional_result.interface, "");
    EXPECT_EQ(traditional_result.property_name, "Usage");
}

// 测试混合新旧语法的函数参数
TEST(FunctionCallTest, GetRelatePropertiesMixedSyntax) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建混合新旧语法的函数参数
    mc::mutable_dict new_syntax_dict;
    new_syntax_dict["type"]          = "sync";
    new_syntax_dict["object_name"]   = "GPU";
    new_syntax_dict["property_name"] = "Load";
    new_syntax_dict["interface"]     = "bmc.hardware.Graphics";
    new_syntax_dict["full_name"]     = "<=/GPU[bmc.hardware.Graphics].Load";

    mc::mutable_dict old_syntax_dict;
    old_syntax_dict["type"]          = "ref";
    old_syntax_dict["object_name"]   = "Memory";
    old_syntax_dict["property_name"] = "Usage";
    old_syntax_dict["interface"]     = "";
    old_syntax_dict["full_name"]     = "#/Memory.Usage";

    mc::dict args = {
        {"normal_param", mc::variant("value")},
        {"new_syntax", new_syntax_dict},
        {"old_syntax", old_syntax_dict}
    };

    func test_func("normal_param + new_syntax + old_syntax", args);
    functions.insert("$Func_Test", mc::variant(test_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 测试参数 - 覆盖部分参数
    mc::mutable_dict params;
    params["normal_param"] = "new_value";
    // new_syntax 和 old_syntax 使用默认值

    auto result = test_func.get_relate_properties("01", params);

    // 应该识别出两个 relate_property
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result.contains("GPU[bmc.hardware.Graphics].Load"));
    EXPECT_TRUE(result.contains("Memory.Usage"));
}

// 测试 is_relate_property 函数对新语法的支持
TEST(FunctionCallTest, IsRelatePropertyWithInterface) {
    // 测试带接口的 relate_property 格式
    mc::mutable_dict interface_prop;
    interface_prop["type"]          = "ref";
    interface_prop["object_name"]   = "Device";
    interface_prop["property_name"] = "Temperature";
    interface_prop["interface"]     = "bmc.dev.TestInterface";
    interface_prop["full_name"]     = "#/Device[bmc.dev.TestInterface].Temperature";
    EXPECT_TRUE(is_relate_property(mc::variant(interface_prop)));

    // 测试传统格式（interface为空）
    mc::mutable_dict traditional_prop;
    traditional_prop["type"]          = "ref";
    traditional_prop["object_name"]   = "CPU";
    traditional_prop["property_name"] = "Usage";
    traditional_prop["interface"]     = "";
    traditional_prop["full_name"]     = "#/CPU.Usage";
    EXPECT_TRUE(is_relate_property(mc::variant(traditional_prop)));

    // 测试缺少interface字段的情况（不再支持旧格式）
    mc::mutable_dict legacy_prop;
    legacy_prop["type"]          = "ref";
    legacy_prop["object_name"]   = "Memory";
    legacy_prop["property_name"] = "Total";
    legacy_prop["full_name"]     = "#/Memory.Total";
    // 注意：没有interface字段，所以现在只有4个字段
    EXPECT_FALSE(is_relate_property(mc::variant(legacy_prop))); // 因为字段数不为5，现在严格要求5个字段
}

// 测试嵌套函数调用中的 relate_properties
TEST(FunctionCallTest, GetRelatePropertiesNested) {
    // 清理之前测试的状态
    func_collection::get_instance().clear();

    mc::mutable_dict functions;
    // 创建嵌套函数调用参数
    mc::mutable_dict nested_func_dict;
    nested_func_dict["func"] = "inner_func";

    mc::mutable_dict nested_params;
    mc::mutable_dict nested_prop_dict;
    nested_prop_dict["type"]          = "ref";
    nested_prop_dict["object_name"]   = "Memory";
    nested_prop_dict["property_name"] = "Usage";
    nested_prop_dict["full_name"]     = "Memory.Usage";
    nested_prop_dict["interface"]     = ""; // 添加interface字段
    nested_params["nested_prop"]      = nested_prop_dict;
    nested_func_dict["params"]        = nested_params;

    // 创建内部函数
    mc::dict inner_args = {
        {"nested_prop", nested_prop_dict}};
    func inner_func("nested_prop", inner_args);
    functions.insert("inner_func", mc::variant(inner_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 创建外部函数
    mc::dict outer_args = {
        {"outer_param", mc::variant("default")}};
    func outer_func("outer_param", outer_args);

    // 测试参数 - 传入嵌套函数调用
    mc::mutable_dict params;
    params["outer_param"] = nested_func_dict;

    auto result = outer_func.get_relate_properties("01", params);

    // 应该从嵌套函数调用中提取 relate_properties
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.contains("Memory.Usage"));
}

// 测试混合参数的 get_relate_properties
TEST(FunctionCallTest, GetRelatePropertiesMixed) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建包含多种类型参数的函数
    mc::mutable_dict prop1_dict;
    prop1_dict["type"]          = "ref";
    prop1_dict["object_name"]   = "CPU";
    prop1_dict["property_name"] = "Temperature";
    prop1_dict["full_name"]     = "CPU.Temperature";
    prop1_dict["interface"]     = ""; // 添加interface字段

    mc::mutable_dict prop2_dict;
    prop2_dict["type"]          = "ref";
    prop2_dict["object_name"]   = "GPU";
    prop2_dict["property_name"] = "Load";
    prop2_dict["full_name"]     = "GPU.Load";
    prop2_dict["interface"]     = ""; // 添加interface字段

    mc::dict args = {
        {"normal_param", mc::variant("value")},
        {"prop1", prop1_dict},
        {"prop2", prop2_dict}};

    func test_func("normal_param + prop1 + prop2", args);
    functions.insert("$Func_Test", mc::variant(test_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 测试参数 - 覆盖部分参数
    mc::mutable_dict params;
    params["normal_param"] = "new_value";
    // prop1 使用默认值，prop2 将被覆盖
    params["prop2"] = "not_a_property";

    auto result = test_func.get_relate_properties("01", params);

    // 只有 prop1 应该被识别为 relate_property
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.contains("CPU.Temperature"));
}

// 测试空结果的 get_relate_properties
TEST(FunctionCallTest, GetRelatePropertiesEmpty) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建不包含任何 relate_property 的函数
    mc::dict args = {
        {"param1", mc::variant("value1")},
        {"param2", mc::variant(42)}};

    func test_func("param1 + param2", args);
    functions.insert("$Func_Test", mc::variant(test_func));
    func_collection::get_instance().add("01", nullptr, functions);

    mc::mutable_dict params;
    params["param1"] = "new_value";

    auto result = test_func.get_relate_properties("01", params);

    // 应该返回空结果
    EXPECT_TRUE(result.empty());
}

// 测试 is_relate_property 函数的各种情况
TEST(FunctionCallTest, IsRelatePropertyFunction) {
    // 测试正确的 relate_property 格式（大小为5，包含必要字段）
    mc::mutable_dict valid_prop;
    valid_prop["type"]          = "ref";
    valid_prop["full_name"]     = "CPU.Temperature";
    valid_prop["object_name"]   = "CPU";
    valid_prop["property_name"] = "Temperature";
    valid_prop["interface"]     = ""; // 新增interface字段
    EXPECT_TRUE(is_relate_property(mc::variant(valid_prop)));

    // 测试包含额外字段的字典（大小不为5）
    mc::mutable_dict extended_prop;
    extended_prop["type"]          = "ref";
    extended_prop["full_name"]     = "CPU.Temperature";
    extended_prop["object_name"]   = "CPU";
    extended_prop["property_name"] = "Temperature";
    extended_prop["interface"]     = "";
    extended_prop["test"]          = "test";
    EXPECT_FALSE(is_relate_property(mc::variant(extended_prop)));

    // 测试缺少必要字段的字典
    mc::mutable_dict incomplete_prop;
    incomplete_prop["type"]        = "ref";
    incomplete_prop["full_name"]   = "CPU.Temperature";
    incomplete_prop["object_name"] = "CPU";
    // 缺少 property_name 和 interface
    EXPECT_FALSE(is_relate_property(mc::variant(incomplete_prop)));

    // 测试字段名错误的字典
    mc::mutable_dict wrong_fields;
    wrong_fields["type"]      = "ref";
    wrong_fields["full_name"] = "CPU.Temperature";
    wrong_fields["obj_name"]  = "CPU";
    wrong_fields["prop_name"] = "Temperature";
    EXPECT_FALSE(is_relate_property(mc::variant(wrong_fields)));

    // 测试字段值类型错误的字典
    mc::mutable_dict wrong_types;
    wrong_types["type"]          = "ref";
    wrong_types["full_name"]     = "CPU.Temperature";
    wrong_types["object_name"]   = 123; // 应该是字符串
    wrong_types["property_name"] = "Temperature";
    EXPECT_FALSE(is_relate_property(mc::variant(wrong_types)));

    // 测试非字典类型
    EXPECT_FALSE(is_relate_property(mc::variant("not a dict")));
    EXPECT_FALSE(is_relate_property(mc::variant(42)));
    EXPECT_FALSE(is_relate_property(mc::variant()));

    // 测试空字典
    mc::mutable_dict empty_dict;
    EXPECT_FALSE(is_relate_property(mc::variant(empty_dict)));
}

// 测试 is_function_call 函数的各种情况
TEST(FunctionCallTest, IsFunctionCallFunction) {
    // 测试正确的函数调用格式
    mc::mutable_dict valid_call;
    valid_call["func"]   = "test_function";
    valid_call["params"] = mc::mutable_dict();
    EXPECT_TRUE(is_function_call(mc::variant(valid_call)));

    // 测试缺少 func 字段
    mc::mutable_dict no_func;
    no_func["params"] = mc::mutable_dict();
    EXPECT_FALSE(is_function_call(mc::variant(no_func)));

    // 测试缺少 params 字段
    mc::mutable_dict no_params;
    no_params["func"] = "test_function";
    EXPECT_FALSE(is_function_call(mc::variant(no_params)));

    // 测试 func 字段类型错误
    mc::mutable_dict wrong_func_type;
    wrong_func_type["func"]   = 123; // 应该是字符串
    wrong_func_type["params"] = mc::mutable_dict();
    EXPECT_FALSE(is_function_call(mc::variant(wrong_func_type)));

    // 测试 params 字段类型错误
    mc::mutable_dict wrong_params_type;
    wrong_params_type["func"]   = "test_function";
    wrong_params_type["params"] = "not a dict"; // 应该是字典
    EXPECT_FALSE(is_function_call(mc::variant(wrong_params_type)));

    // 测试非字典类型
    EXPECT_FALSE(is_function_call(mc::variant("not a dict")));
    EXPECT_FALSE(is_function_call(mc::variant(42)));
    EXPECT_FALSE(is_function_call(mc::variant()));
}

// 测试复杂的嵌套场景
TEST(FunctionCallTest, ComplexNestedScenario) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 创建一个包含嵌套函数调用和relate_properties的复杂场景

    // 内层函数的参数（包含 relate_property）
    mc::mutable_dict inner_prop;
    inner_prop["type"]          = "ref";
    inner_prop["object_name"]   = "Memory";
    inner_prop["property_name"] = "Usage";
    inner_prop["full_name"]     = "Memory.Usage";
    inner_prop["interface"]     = ""; // 添加interface字段

    mc::mutable_dict inner_params;
    inner_params["memory_param"] = inner_prop;

    // 内层函数调用
    mc::mutable_dict inner_call;
    inner_call["func"]   = "inner_function";
    inner_call["params"] = inner_params;

    // 外层函数的参数
    mc::mutable_dict outer_prop;
    outer_prop["type"]          = "ref";
    outer_prop["object_name"]   = "CPU";
    outer_prop["property_name"] = "Temperature";
    outer_prop["full_name"]     = "CPU.Temperature";
    outer_prop["interface"]     = ""; // 添加interface字段

    mc::dict outer_args = {
        {"cpu_param", outer_prop},
        {"nested_call", mc::variant("default")}};

    func outer_func("cpu_param + nested_call", outer_args);
    functions.insert("$Func_Test", mc::variant(outer_func));
    func_collection::get_instance().add("01", nullptr, functions);

    // 测试参数 - 传入嵌套函数调用
    mc::mutable_dict test_params;
    test_params["nested_call"] = inner_call;

    auto result = outer_func.get_relate_properties("01", test_params);

    // 应该包含来自默认参数和嵌套函数调用的relate_properties
    EXPECT_GE(result.size(), 1); // 至少包含CPU.Temperature
    EXPECT_TRUE(result.contains("CPU.Temperature"));

    // 如果递归处理正确，也应该包含Memory.Usage
    // 注意：这取决于get_relate_properties的具体实现
}

// 测试错误处理和边界情况
TEST(FunctionCallTest, ErrorHandlingAndEdgeCases) {
    func_collection::get_instance().clear();
    mc::mutable_dict functions;

    // 测试空参数的函数
    mc::dict empty_args;
    func     empty_func("42", empty_args);
    functions.insert("$Func_Test", mc::variant(empty_func));
    func_collection::get_instance().add("01", nullptr, functions);

    mc::mutable_dict empty_params;
    auto             result = empty_func.get_relate_properties("01", empty_params);
    EXPECT_TRUE(result.empty());

    // 测试只有普通参数的函数
    mc::dict normal_args = {
        {"param1", mc::variant("value1")},
        {"param2", mc::variant(42)},
        {"param3", mc::variant(true)}};
    func normal_func("param1 + param2 + param3", normal_args);
    functions.clear();
    functions.insert("$Func_Normal", mc::variant(normal_func));
    func_collection::get_instance().add("02", nullptr, functions);

    mc::mutable_dict normal_params;
    auto             normal_result = normal_func.get_relate_properties("02", normal_params);
    EXPECT_TRUE(normal_result.empty());

    // 测试参数类型不匹配的情况
    mc::mutable_dict invalid_params;
    invalid_params["param1"] = mc::variant(123); // 覆盖字符串默认值

    auto invalid_result = normal_func.get_relate_properties("02", invalid_params);
    EXPECT_TRUE(invalid_result.empty());
}

} // namespace