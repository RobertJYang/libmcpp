/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file test_json_wrapper.cpp
 * @brief JSON包装器的单元测试
 */

#include <gtest/gtest.h>
#include <json_api.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/json_wrapper.h>
#include <mc/variant.h>
#include <string>

using namespace mc;
using namespace mc::json_wrapper;

// 定义 TagJsonChildList 结构体用于访问 ref_count（匹配底层 JSON 库的结构）
typedef struct TagJsonChildList {
    TagJsonChildList* prev;
    TagJsonChildList* next;
    uint16_t          ref_count;
} Json;

// ======================== JSON值创建测试 ========================

TEST(JsonValueCreateTest, create_null) {
    JsonValue val = JsonValue::make_null();
    EXPECT_TRUE(val.is_null());
    EXPECT_FALSE(val.is_bool());
    EXPECT_FALSE(val.is_number());
}

TEST(JsonValueCreateTest, create_bool) {
    JsonValue val_true = JsonValue::make_bool(true);
    EXPECT_TRUE(val_true.is_bool());
    EXPECT_TRUE(val_true.as_bool());

    JsonValue val_false = JsonValue::make_bool(false);
    EXPECT_TRUE(val_false.is_bool());
    EXPECT_FALSE(val_false.as_bool());
}

TEST(JsonValueCreateTest, create_int) {
    JsonValue val = JsonValue::make_int(12345);
    EXPECT_TRUE(val.is_int());
    EXPECT_TRUE(val.is_number());
    EXPECT_EQ(val.as_int(), 12345);
}

TEST(JsonValueCreateTest, create_double) {
    JsonValue val = JsonValue::make_double(3.14159);
    EXPECT_TRUE(val.is_double());
    EXPECT_TRUE(val.is_number());
    EXPECT_DOUBLE_EQ(val.as_double(), 3.14159);
}

TEST(JsonValueCreateTest, create_string) {
    JsonValue val = JsonValue::make_string("hello world");
    EXPECT_TRUE(val.is_string());
    EXPECT_EQ(val.as_string(), "hello world");
}

TEST(JsonValueCreateTest, create_string_empty) {
    JsonValue val = JsonValue::make_string("");
    EXPECT_TRUE(val.is_string());
    EXPECT_EQ(val.as_string(), "");
}

TEST(JsonValueCreateTest, create_array) {
    JsonValue val = JsonValue::make_array();
    EXPECT_TRUE(val.is_array());
    EXPECT_EQ(val.as_array().size(), 0);
}

TEST(JsonValueCreateTest, create_object) {
    JsonValue val = JsonValue::make_object();
    EXPECT_TRUE(val.is_object());
    EXPECT_EQ(val.as_object().size(), 0);
}

// ======================== JSON值拷贝测试 ========================

TEST(JsonValueCopyTest, copy_constructor) {
    JsonValue val1 = JsonValue::make_int(100);
    JsonValue val2(val1);
    EXPECT_EQ(val1.as_int(), val2.as_int());
    EXPECT_EQ(val1, val2);
}

TEST(JsonValueCopyTest, copy_assignment) {
    JsonValue val1 = JsonValue::make_string("test");
    JsonValue val2 = JsonValue::make_null();
    val2           = val1;
    EXPECT_EQ(val1.as_string(), val2.as_string());
    EXPECT_EQ(val1, val2);
}

TEST(JsonValueCopyTest, move_constructor) {
    JsonValue val1 = JsonValue::make_int(200);
    JsonValue val2(std::move(val1));
    EXPECT_EQ(val2.as_int(), 200);
}

TEST(JsonValueCopyTest, move_assignment) {
    JsonValue val1 = JsonValue::make_string("move test");
    JsonValue val2 = JsonValue::make_null();
    val2           = std::move(val1);
    EXPECT_EQ(val2.as_string(), "move test");
}

// ======================== JSON类型判断测试 ========================

TEST(JsonValueTypeTest, type_check) {
    JsonValue null_val = JsonValue::make_null();
    EXPECT_TRUE(null_val.is_null());
    EXPECT_FALSE(null_val.is_bool());
    EXPECT_FALSE(null_val.is_number());
    EXPECT_FALSE(null_val.is_string());
    EXPECT_FALSE(null_val.is_array());
    EXPECT_FALSE(null_val.is_object());

    JsonValue bool_val = JsonValue::make_bool(true);
    EXPECT_FALSE(bool_val.is_null());
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_FALSE(bool_val.is_number());

    JsonValue int_val = JsonValue::make_int(42);
    EXPECT_TRUE(int_val.is_int());
    EXPECT_TRUE(int_val.is_number());
    EXPECT_FALSE(int_val.is_double());

    JsonValue double_val = JsonValue::make_double(3.14);
    EXPECT_FALSE(double_val.is_int());
    EXPECT_TRUE(double_val.is_double());
    EXPECT_TRUE(double_val.is_number());

    JsonValue str_val = JsonValue::make_string("test");
    EXPECT_TRUE(str_val.is_string());

    JsonValue arr_val = JsonValue::make_array();
    EXPECT_TRUE(arr_val.is_array());

    JsonValue obj_val = JsonValue::make_object();
    EXPECT_TRUE(obj_val.is_object());
}

TEST(JsonValueTypeTest, invalid_type_cast) {
    JsonValue val = JsonValue::make_int(42);
    EXPECT_THROW(val.as_bool(), bad_cast_exception);
    EXPECT_THROW(val.as_string(), bad_cast_exception);
    EXPECT_THROW(val.as_array(), bad_cast_exception);
    EXPECT_THROW(val.as_object(), bad_cast_exception);
}

// ======================== variant转换测试 ========================

TEST(JsonValueVariantTest, to_variant_basic_types) {
    JsonValue null_val = JsonValue::make_null();
    variant   v_null   = null_val.to_variant();
    EXPECT_TRUE(v_null.is_null());

    JsonValue bool_val = JsonValue::make_bool(true);
    variant   v_bool   = bool_val.to_variant();
    EXPECT_EQ(v_bool, true);

    JsonValue int_val = JsonValue::make_int(42);
    variant   v_int   = int_val.to_variant();
    EXPECT_EQ(v_int, 42);

    JsonValue double_val = JsonValue::make_double(3.14);
    variant   v_double   = double_val.to_variant();
    EXPECT_DOUBLE_EQ(v_double.as<double>(), 3.14);

    JsonValue str_val = JsonValue::make_string("hello");
    variant   v_str   = str_val.to_variant();
    EXPECT_EQ(v_str, "hello");
}

TEST(JsonValueVariantTest, to_variant_array) {
    JsonValue arr = JsonValue::make_array();
    arr.as_array().push_back(JsonValue::make_int(1));
    arr.as_array().push_back(JsonValue::make_string("test"));
    arr.as_array().push_back(JsonValue::make_bool(true));

    variant v_arr = arr.to_variant();
    EXPECT_TRUE(v_arr.is_array());
    EXPECT_EQ(v_arr.get_array().size(), 3);
    EXPECT_EQ(v_arr.get_array()[0], 1);
    EXPECT_EQ(v_arr.get_array()[1], "test");
    EXPECT_EQ(v_arr.get_array()[2], true);
}

TEST(JsonValueVariantTest, to_variant_object) {
    JsonValue obj = JsonValue::make_object();
    obj.as_object().set("name", JsonValue::make_string("test"));
    obj.as_object().set("age", JsonValue::make_int(30));

    variant v_obj = obj.to_variant();
    EXPECT_TRUE(v_obj.is_object());
    EXPECT_EQ(v_obj.get_object()["name"], "test");
    EXPECT_EQ(v_obj.get_object()["age"], 30);
}

TEST(JsonValueVariantTest, from_variant_basic_types) {
    JsonValue null_val = JsonValue::from_variant(variant());
    EXPECT_TRUE(null_val.is_null());

    JsonValue bool_val = JsonValue::from_variant(variant(true));
    EXPECT_EQ(bool_val.as_bool(), true);

    JsonValue int_val = JsonValue::from_variant(variant(int64_t{42}));
    EXPECT_EQ(int_val.as_int(), 42);

    JsonValue double_val = JsonValue::from_variant(variant(3.14));
    EXPECT_DOUBLE_EQ(double_val.as_double(), 3.14);

    JsonValue str_val = JsonValue::from_variant(variant("hello"));
    EXPECT_EQ(str_val.as_string(), "hello");
}

TEST(JsonValueVariantTest, from_variant_array) {
    variants  arr{variant(1), variant("test"), variant(true)};
    JsonValue json_arr = JsonValue::from_variant(variant(arr));
    EXPECT_TRUE(json_arr.is_array());
    EXPECT_EQ(json_arr.as_array().size(), 3);
}

TEST(JsonValueVariantTest, from_variant_object) {
    dict      obj{{"name", "test"}, {"age", 30}};
    JsonValue json_obj = JsonValue::from_variant(variant(obj));
    EXPECT_TRUE(json_obj.is_object());
    EXPECT_TRUE(json_obj.as_object().has("name"));
    EXPECT_EQ(json_obj.as_object().get("name").as_string(), "test");
}

TEST(JsonValueVariantTest, round_trip) {
    dict    original{{"name", "test"}, {"age", 30}, {"scores", variants{85, 92, 78}}};
    variant v_original(original);

    JsonValue json_val = JsonValue::from_variant(v_original);
    variant   v_result = json_val.to_variant();

    EXPECT_EQ(v_result.get_object()["name"], "test");
    EXPECT_EQ(v_result.get_object()["age"], 30);
    EXPECT_EQ(v_result.get_object()["scores"].get_array().size(), 3);
}

// ======================== 相等比较测试 ========================

TEST(JsonValueEqualTest, equal_basic_types) {
    EXPECT_EQ(JsonValue::make_null(), JsonValue::make_null());
    EXPECT_EQ(JsonValue::make_bool(true), JsonValue::make_bool(true));
    EXPECT_EQ(JsonValue::make_int(42), JsonValue::make_int(42));
    EXPECT_EQ(JsonValue::make_double(3.14), JsonValue::make_double(3.14));
    EXPECT_EQ(JsonValue::make_string("test"), JsonValue::make_string("test"));
}

TEST(JsonValueEqualTest, not_equal_basic_types) {
    EXPECT_NE(JsonValue::make_null(), JsonValue::make_bool(false));
    EXPECT_NE(JsonValue::make_bool(true), JsonValue::make_bool(false));
    EXPECT_NE(JsonValue::make_int(42), JsonValue::make_int(43));
    EXPECT_NE(JsonValue::make_double(3.14), JsonValue::make_double(3.15));
    EXPECT_NE(JsonValue::make_string("test"), JsonValue::make_string("test2"));
}

TEST(JsonValueEqualTest, equal_array) {
    JsonValue arr1 = JsonValue::make_array();
    arr1.as_array().push_back(JsonValue::make_int(1));
    arr1.as_array().push_back(JsonValue::make_string("test"));

    JsonValue arr2 = JsonValue::make_array();
    arr2.as_array().push_back(JsonValue::make_int(1));
    arr2.as_array().push_back(JsonValue::make_string("test"));

    EXPECT_EQ(arr1, arr2);
}

TEST(JsonValueEqualTest, not_equal_array) {
    JsonValue arr1 = JsonValue::make_array();
    arr1.as_array().push_back(JsonValue::make_int(1));

    JsonValue arr2 = JsonValue::make_array();
    arr2.as_array().push_back(JsonValue::make_int(2));

    EXPECT_NE(arr1, arr2);
}

TEST(JsonValueEqualTest, equal_object) {
    JsonValue obj1 = JsonValue::make_object();
    obj1.as_object().set("name", JsonValue::make_string("test"));
    obj1.as_object().set("age", JsonValue::make_int(30));

    JsonValue obj2 = JsonValue::make_object();
    obj2.as_object().set("name", JsonValue::make_string("test"));
    obj2.as_object().set("age", JsonValue::make_int(30));

    EXPECT_EQ(obj1, obj2);
}

TEST(JsonValueEqualTest, not_equal_object) {
    JsonValue obj1 = JsonValue::make_object();
    obj1.as_object().set("name", JsonValue::make_string("test"));

    JsonValue obj2 = JsonValue::make_object();
    obj2.as_object().set("name", JsonValue::make_string("test2"));

    EXPECT_NE(obj1, obj2);
}

// ======================== JsonArray测试 ========================

TEST(JsonArrayTest, empty_array) {
    JsonArray arr = JsonValue::make_array().as_array();
    EXPECT_EQ(arr.size(), 0);
    EXPECT_TRUE(arr.empty());
}

TEST(JsonArrayTest, push_back) {
    JsonValue val = JsonValue::make_array();
    JsonArray arr = val.as_array();

    arr.push_back(JsonValue::make_int(1));
    arr.push_back(JsonValue::make_string("test"));
    arr.push_back(JsonValue::make_bool(true));

    EXPECT_EQ(arr.size(), 3);
    EXPECT_FALSE(arr.empty());
}

TEST(JsonArrayTest, at_access) {
    JsonValue val = JsonValue::make_array();
    JsonArray arr = val.as_array();

    arr.push_back(JsonValue::make_int(10));
    arr.push_back(JsonValue::make_int(20));
    arr.push_back(JsonValue::make_int(30));

    EXPECT_EQ(arr.at(0).as_int(), 10);
    EXPECT_EQ(arr.at(1).as_int(), 20);
    EXPECT_EQ(arr.at(2).as_int(), 30);
}

TEST(JsonArrayTest, operator_access) {
    JsonValue val = JsonValue::make_array();
    JsonArray arr = val.as_array();

    arr.push_back(JsonValue::make_string("a"));
    arr.push_back(JsonValue::make_string("b"));

    EXPECT_EQ(arr[0].as_string(), "a");
    EXPECT_EQ(arr[1].as_string(), "b");
}

TEST(JsonArrayTest, set_element) {
    JsonValue val = JsonValue::make_array();
    JsonArray arr = val.as_array();

    arr.push_back(JsonValue::make_int(1));
    arr.push_back(JsonValue::make_int(2));

    arr.set(0, JsonValue::make_int(100));
    EXPECT_EQ(arr[0].as_int(), 100);
    EXPECT_EQ(arr[1].as_int(), 2);
}

TEST(JsonArrayTest, iteration) {
    JsonValue val = JsonValue::make_array();
    JsonArray arr = val.as_array();

    arr.push_back(JsonValue::make_int(1));
    arr.push_back(JsonValue::make_int(2));
    arr.push_back(JsonValue::make_int(3));

    int sum = 0;
    for (const auto& item : arr) {
        sum += item.as_int();
    }
    EXPECT_EQ(sum, 6);
}

TEST(JsonArrayTest, copy_semantics) {
    JsonValue val  = JsonValue::make_array();
    JsonArray arr1 = val.as_array();
    arr1.push_back(JsonValue::make_int(42));

    JsonArray arr2 = arr1;
    EXPECT_EQ(arr2.size(), 1);
    EXPECT_EQ(arr2[0].as_int(), 42);
}

// ======================== JsonObject测试 ========================

TEST(JsonObjectTest, empty_object) {
    JsonObject obj = JsonValue::make_object().as_object();
    EXPECT_EQ(obj.size(), 0);
    EXPECT_TRUE(obj.empty());
}

TEST(JsonObjectTest, set_and_get) {
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();

    obj.set("name", JsonValue::make_string("test"));
    obj.set("age", JsonValue::make_int(30));

    EXPECT_TRUE(obj.has("name"));
    EXPECT_TRUE(obj.has("age"));
    EXPECT_FALSE(obj.has("nonexistent"));

    EXPECT_EQ(obj.get("name").as_string(), "test");
    EXPECT_EQ(obj.get("age").as_int(), 30);
}

TEST(JsonObjectTest, operator_access) {
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();

    obj.set("key", JsonValue::make_string("value"));
    EXPECT_EQ(obj["key"].as_string(), "value");
}

TEST(JsonObjectTest, erase) {
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();

    obj.set("key1", JsonValue::make_int(1));
    obj.set("key2", JsonValue::make_int(2));
    EXPECT_EQ(obj.size(), 2);

    obj.erase("key1");
    EXPECT_FALSE(obj.has("key1"));
    EXPECT_TRUE(obj.has("key2"));
}

TEST(JsonObjectTest, size) {
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();

    EXPECT_EQ(obj.size(), 0);

    obj.set("a", JsonValue::make_int(1));
    EXPECT_EQ(obj.size(), 1);

    obj.set("b", JsonValue::make_int(2));
    EXPECT_EQ(obj.size(), 2);

    obj.set("c", JsonValue::make_int(3));
    EXPECT_EQ(obj.size(), 3);
}

TEST(JsonObjectTest, iteration) {
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();

    obj.set("a", JsonValue::make_int(1));
    obj.set("b", JsonValue::make_int(2));
    obj.set("c", JsonValue::make_int(3));

    int sum   = 0;
    int count = 0;
    for (const auto& kv : obj) {
        sum += kv.value.as_int();
        count++;
    }
    EXPECT_EQ(sum, 6);
    EXPECT_EQ(count, 3);
}

TEST(JsonObjectTest, copy_semantics) {
    JsonValue  val  = JsonValue::make_object();
    JsonObject obj1 = val.as_object();
    obj1.set("key", JsonValue::make_string("value"));

    JsonObject obj2 = obj1;
    EXPECT_TRUE(obj2.has("key"));
    EXPECT_EQ(obj2.get("key").as_string(), "value");
}

// ======================== 复杂场景测试 ========================

TEST(JsonComplexTest, nested_structure) {
    JsonValue  root     = JsonValue::make_object();
    JsonObject root_obj = root.as_object();

    // 添加基本类型
    root_obj.set("name", JsonValue::make_string("John"));
    root_obj.set("age", JsonValue::make_int(30));

    // 添加数组
    JsonValue scores = JsonValue::make_array();
    scores.as_array().push_back(JsonValue::make_int(85));
    scores.as_array().push_back(JsonValue::make_int(92));
    scores.as_array().push_back(JsonValue::make_int(78));
    root_obj.set("scores", scores);

    // 添加嵌套对象
    JsonValue address = JsonValue::make_object();
    address.as_object().set("city", JsonValue::make_string("Beijing"));
    address.as_object().set("zipcode", JsonValue::make_string("100000"));
    root_obj.set("address", address);

    // 验证结构
    EXPECT_EQ(root_obj.get("name").as_string(), "John");
    EXPECT_EQ(root_obj.get("age").as_int(), 30);
    EXPECT_EQ(root_obj.get("scores").as_array().size(), 3);
    EXPECT_EQ(root_obj.get("scores").as_array()[1].as_int(), 92);
    EXPECT_EQ(root_obj.get("address").as_object().get("city").as_string(), "Beijing");
}

TEST(JsonComplexTest, modify_existing_structure) {
    // 手动构建 JSON 结构
    JsonValue  val = JsonValue::make_object();
    JsonObject obj = val.as_object();
    obj.set("name", JsonValue::make_string("Alice"));

    JsonValue scores_val = JsonValue::make_array();
    JsonArray scores     = scores_val.as_array();
    scores.push_back(JsonValue::make_int(80));
    scores.push_back(JsonValue::make_int(90));
    scores.push_back(JsonValue::make_int(85));
    obj.set("scores", scores_val);

    // 修改结构
    obj.set("age", JsonValue::make_int(25));
    JsonArray scores_arr = obj.get("scores").as_array();
    scores_arr.push_back(JsonValue::make_int(95));

    EXPECT_TRUE(obj.has("age"));
    EXPECT_EQ(obj.get("age").as_int(), 25);
    EXPECT_EQ(scores_arr.size(), 4);
}

TEST(JsonComplexTest, variant_round_trip_complex) {
    dict complex_obj{{"name", "test"},
                     {"numbers", variants{1, 2, 3}},
                     {"nested", dict{{"key", "value"}}}};

    JsonValue json_val = JsonValue::from_variant(variant(complex_obj));
    variant   result   = json_val.to_variant();

    EXPECT_EQ(result.get_object()["name"], "test");
    EXPECT_EQ(result.get_object()["numbers"].get_array().size(), 3);
    EXPECT_EQ(result.get_object()["nested"].get_object()["key"], "value");
}

// 演示混合类型数组的JSON输出
TEST(JsonComplexTest, mixed_type_array_output) {
    JsonArray arr = JsonValue::make_array().as_array();

    // 添加不同类型的元素
    arr.push_back(JsonValue::make_int(1));         // 整数
    arr.push_back(JsonValue::make_string("test")); // 字符串
    arr.push_back(JsonValue::make_bool(true));     // 布尔值
    arr.push_back(JsonValue::make_double(3.14));   // 浮点数
    arr.push_back(JsonValue::make_array());        // 空数组
    arr.push_back(JsonValue::make_object());       // 空对象

    JsonValue val(arr.get_raw());

    // 验证数组内容
    EXPECT_EQ(arr.size(), 6);
    EXPECT_EQ(arr[0].as_int(), 1);
    EXPECT_EQ(arr[1].as_string(), "test");
    EXPECT_EQ(arr[2].as_bool(), true);
    EXPECT_DOUBLE_EQ(arr[3].as_double(), 3.14);
    EXPECT_TRUE(arr[4].is_array());
    EXPECT_TRUE(arr[5].is_object());
}

// ======================== 复杂场景：API使用示例 ========================

// 场景1：构建用户列表JSON
TEST(JsonRealWorldTest, build_user_list) {
    // 目标: {"users":[{"name":"Alice","age":30},{"name":"Bob","age":25}],"count":2}

    // 创建根对象
    JsonObject root = JsonValue::make_object().as_object();

    // 创建用户数组
    JsonArray users = JsonValue::make_array().as_array();

    // 创建第一个用户
    JsonObject user1 = JsonValue::make_object().as_object();
    user1.set("name", JsonValue::make_string("Alice"));
    user1.set("age", JsonValue::make_int(30));
    users.push_back(JsonValue(user1.get_raw()));

    // 创建第二个用户
    JsonObject user2 = JsonValue::make_object().as_object();
    user2.set("name", JsonValue::make_string("Bob"));
    user2.set("age", JsonValue::make_int(25));
    users.push_back(JsonValue(user2.get_raw()));

    // 将数组添加到根对象
    root.set("users", JsonValue(users.get_raw()));
    root.set("count", JsonValue::make_int(2));

    // 验证结构
    JsonValue root_val(root.get_raw());
    EXPECT_TRUE(root_val.is_object());
    EXPECT_EQ(root.size(), 2);
    EXPECT_TRUE(root.has("users"));
    EXPECT_TRUE(root.has("count"));

    JsonArray users_arr = root.get("users").as_array();
    EXPECT_EQ(users_arr.size(), 2);
    EXPECT_EQ(users_arr[0].as_object().get("name").as_string(), "Alice");
    EXPECT_EQ(users_arr[1].as_object().get("name").as_string(), "Bob");
}

// 场景2：构建并修改JSON
TEST(JsonRealWorldTest, build_and_modify) {
    // 手动构建 JSON 结构
    JsonValue  root     = JsonValue::make_object();
    JsonObject root_obj = root.as_object();

    // 创建用户数组
    JsonValue users_val = JsonValue::make_array();
    JsonArray users     = users_val.as_array();

    // 创建第一个用户
    JsonObject user1 = JsonValue::make_object().as_object();
    user1.set("name", JsonValue::make_string("Alice"));
    user1.set("age", JsonValue::make_int(30));
    users.push_back(JsonValue(user1.get_raw()));

    root_obj.set("users", users_val);
    root_obj.set("count", JsonValue::make_int(users.size()));

    // 添加新用户
    JsonObject new_user = JsonValue::make_object().as_object();
    new_user.set("name", JsonValue::make_string("Charlie"));
    new_user.set("age", JsonValue::make_int(35));
    JsonArray users_arr = root_obj.get("users").as_array();
    users_arr.push_back(JsonValue(new_user.get_raw()));

    // 更新计数
    root_obj.set("count", JsonValue::make_int(users_arr.size()));

    // 验证
    EXPECT_EQ(users_arr.size(), 2);
    EXPECT_EQ(root_obj.get("count").as_int(), 2);
    EXPECT_EQ(users_arr[1].as_object().get("name").as_string(), "Charlie");
}

// 场景3：统一处理不同类型的JSON值
TEST(JsonRealWorldTest, generic_json_processing) {
    // 辅助函数：计算JSON中的值数量
    auto count_values = [](const JsonValue& value, auto& count_func) -> int {
        if (value.is_array()) {
            JsonArray arr   = value.as_array();
            int       count = 0;
            for (uint32_t i = 0; i < arr.size(); ++i) {
                count += count_func(arr[i], count_func);
            }
            return count;
        } else if (value.is_object()) {
            JsonObject obj   = value.as_object();
            int        count = 0;
            for (const auto& kv : obj) {
                count += count_func(kv.value, count_func);
            }
            return count;
        } else {
            return 1; // 基本类型计为1个值
        }
    };

    // 创建测试JSON: {"a":1,"b":[2,3,4],"c":{"d":5}}
    JsonObject root = JsonValue::make_object().as_object();
    root.set("a", JsonValue::make_int(1));

    JsonArray arr = JsonValue::make_array().as_array();
    arr.push_back(JsonValue::make_int(2));
    arr.push_back(JsonValue::make_int(3));
    arr.push_back(JsonValue::make_int(4));
    root.set("b", JsonValue(arr.get_raw()));

    JsonObject nested = JsonValue::make_object().as_object();
    nested.set("d", JsonValue::make_int(5));
    root.set("c", JsonValue(nested.get_raw()));

    // 统计值数量
    JsonValue root_val(root.get_raw());
    int       total = count_values(root_val, count_values);
    EXPECT_EQ(total, 5); // 1, 2, 3, 4, 5
}

// 场景4：类型检查和安全访问
TEST(JsonRealWorldTest, type_safe_access) {
    // 手动构建 JSON 结构
    JsonValue  root = JsonValue::make_object();
    JsonObject obj  = root.as_object();

    obj.set("name", JsonValue::make_string("Alice"));
    obj.set("age", JsonValue::make_int(30));

    JsonValue tags_val = JsonValue::make_array();
    JsonArray tags     = tags_val.as_array();
    tags.push_back(JsonValue::make_string("cpp"));
    tags.push_back(JsonValue::make_string("json"));
    obj.set("tags", tags_val);

    // 安全的类型检查访问
    if (root.is_object()) {
        // 检查字段是否存在
        if (obj.has("name")) {
            JsonValue name_val = obj.get("name");
            if (name_val.is_string()) {
                std::string name = name_val.as_string();
                EXPECT_EQ(name, "Alice");
            }
        }

        // 检查数组字段
        if (obj.has("tags")) {
            JsonValue tags_v = obj.get("tags");
            if (tags_v.is_array()) {
                JsonArray tags_arr = tags_v.as_array();
                EXPECT_EQ(tags_arr.size(), 2);
                EXPECT_EQ(tags_arr[0].as_string(), "cpp");
            }
        }
    }
}

// 场景5：JsonValue作为统一接口
TEST(JsonRealWorldTest, unified_interface_with_json_value) {
    // 函数接受JsonValue作为参数，可以处理任何类型
    auto process_json = [](const JsonValue& value) -> std::string {
        if (value.is_null()) {
            return "null";
        }
        if (value.is_bool()) {
            return value.as_bool() ? "true" : "false";
        }
        if (value.is_int()) {
            return std::to_string(value.as_int());
        }
        if (value.is_string()) {
            return "\"" + value.as_string() + "\"";
        }
        if (value.is_array()) {
            return "array[" + std::to_string(value.as_array().size()) + "]";
        }
        if (value.is_object()) {
            return "object{" + std::to_string(value.as_object().size()) + "}";
        }
        return "unknown";
    };

    // 测试不同类型
    EXPECT_EQ(process_json(JsonValue::make_null()), "null");
    EXPECT_EQ(process_json(JsonValue::make_bool(true)), "true");
    EXPECT_EQ(process_json(JsonValue::make_int(42)), "42");
    EXPECT_EQ(process_json(JsonValue::make_string("test")), "\"test\"");

    JsonArray arr = JsonValue::make_array().as_array();
    arr.push_back(JsonValue::make_int(1));
    arr.push_back(JsonValue::make_int(2));
    EXPECT_EQ(process_json(JsonValue(arr.get_raw())), "array[2]");
}

// ======================== 引用计数和内存安全测试 ========================

// 测试：引用计数基础功能（拷贝、移动、共享）
TEST(JsonRefCountTest, basic_reference_counting) {
    // 测试拷贝共享
    JsonValue val1 = JsonValue::make_int(42);
    JsonValue val2(val1);
    EXPECT_EQ(val1.get_raw(), val2.get_raw());
    EXPECT_EQ(val2.as_int(), 42);

    // 测试移动转移所有权
    Json*     raw = val1.get_raw();
    JsonValue val3(std::move(val1));
    EXPECT_EQ(val3.get_raw(), raw);
    EXPECT_EQ(val1.get_raw(), nullptr);

    // 测试多引用共享
    JsonValue val4 = JsonValue::make_string("shared");
    Json*     raw4 = val4.get_raw();
    {
        JsonValue val5 = val4;
        JsonValue val6 = val4;
        EXPECT_EQ(val5.get_raw(), raw4);
        EXPECT_EQ(val6.get_raw(), raw4);
    }
    // 作用域结束后仍然有效
    EXPECT_EQ(val4.as_string(), "shared");

    // 测试自赋值安全
    val4 = val4;
    EXPECT_EQ(val4.get_raw(), raw4);
}

// 测试：容器类型的引用计数
TEST(JsonRefCountTest, container_reference_counting) {
    // 测试 JsonArray
    JsonValue val_arr = JsonValue::make_array();
    Json*     raw_arr = val_arr.get_raw();
    JsonArray arr1    = val_arr.as_array();
    JsonArray arr2    = arr1;
    EXPECT_EQ(arr1.get_raw(), raw_arr);
    EXPECT_EQ(arr2.get_raw(), raw_arr);
    arr1.push_back(JsonValue::make_int(1));
    EXPECT_EQ(arr2.size(), 1);

    // 测试 JsonObject
    JsonValue  val_obj = JsonValue::make_object();
    Json*      raw_obj = val_obj.get_raw();
    JsonObject obj1    = val_obj.as_object();
    JsonObject obj2    = obj1;
    EXPECT_EQ(obj1.get_raw(), raw_obj);
    EXPECT_EQ(obj2.get_raw(), raw_obj);
    obj1.set("key", JsonValue::make_string("value"));
    EXPECT_TRUE(obj2.has("key"));
}

// 测试：对象包含子对象时的引用计数
TEST(JsonRefCountTest, object_child_reference_counting) {
    // 创建父对象和子对象
    JsonValue parent_val = JsonValue::make_object();
    JsonValue child_val  = JsonValue::make_object();
    Json*     child_raw  = child_val.get_raw();

    // 初始状态：child_val 持有引用，引用计数 = 1
    EXPECT_EQ(child_raw->ref_count, 1);

    // 将子对象添加到父对象（set 会创建 Quote，增加引用计数）
    parent_val.as_object().set("name", JsonValue::make_string("parent"));
    parent_val.as_object().set("child", child_val);

    // 引用计数 = 2：child_val(1) + Quote(1)
    EXPECT_EQ(child_raw->ref_count, 2);

    // 通过父对象获取子对象（get 会调用 add_ref，增加引用计数）
    JsonValue* child_from_parent = new JsonValue(parent_val.as_object().get("child"));
    EXPECT_EQ(child_from_parent->get_raw(), child_raw);
    EXPECT_EQ(child_raw->ref_count, 3); // child_val(1) + Quote(1) + get()中的add_ref(1)

    // 手动释放 child_from_parent（减少引用计数）
    delete child_from_parent;
    EXPECT_EQ(child_raw->ref_count, 2); // child_val(1) + Quote(1)

    // 手动释放 parent_val（Quote 会被释放，减少引用计数）
    JsonValue* parent_ptr = new JsonValue(std::move(parent_val));
    delete parent_ptr;
    EXPECT_EQ(child_raw->ref_count, 1); // 只有 child_val(1)
}

// 测试：数组包含子对象时的引用计数
TEST(JsonRefCountTest, array_child_reference_counting) {
    // 创建数组和子对象
    JsonValue arr_val   = JsonValue::make_array();
    JsonValue child_val = JsonValue::make_object();
    Json*     child_raw = child_val.get_raw();

    // 初始状态：child_val 持有引用，引用计数 = 1
    EXPECT_EQ(child_raw->ref_count, 1);

    // 将子对象添加到数组（push_back 会创建 Quote，增加引用计数）
    child_val.as_object().set("name", JsonValue::make_string("child"));
    arr_val.as_array().push_back(child_val);

    // 引用计数 = 2：child_val(1) + Quote(1)
    EXPECT_EQ(child_raw->ref_count, 2);

    // 通过数组获取子对象（at 会调用 add_ref，增加引用计数）
    JsonValue* child_from_array = new JsonValue(arr_val.as_array().at(0));
    EXPECT_EQ(child_from_array->get_raw(), child_raw);
    EXPECT_EQ(child_raw->ref_count, 3); // child_val(1) + Quote(1) + at()中的add_ref(1)

    // 手动释放 child_from_array（减少引用计数）
    delete child_from_array;
    EXPECT_EQ(child_raw->ref_count, 2); // child_val(1) + Quote(1)

    // 手动释放 arr_val（Quote 会被释放，减少引用计数）
    JsonValue* arr_ptr = new JsonValue(std::move(arr_val));
    delete arr_ptr;
    EXPECT_EQ(child_raw->ref_count, 1); // 只有 child_val(1)
}

// 测试：循环引用时的引用计数
TEST(JsonRefCountTest, circular_reference_counting) {
    // 创建两个对象，形成循环引用
    JsonValue obj1_val = JsonValue::make_object();
    JsonValue obj2_val = JsonValue::make_object();
    Json*     obj1_raw = obj1_val.get_raw();
    Json*     obj2_raw = obj2_val.get_raw();

    // 初始状态：每个对象引用计数 = 1
    EXPECT_EQ(obj1_raw->ref_count, 1);
    EXPECT_EQ(obj2_raw->ref_count, 1);

    // obj1 包含 obj2（创建 Quote，obj2 引用计数 +1）
    obj1_val.as_object().set("name", JsonValue::make_string("obj1"));
    obj1_val.as_object().set("child", obj2_val);

    // obj2 引用计数 = 2：obj2_val(1) + Quote(1)
    EXPECT_EQ(obj2_raw->ref_count, 2);

    // obj2 包含 obj1（创建 Quote，obj1 引用计数 +1）
    obj2_val.as_object().set("name", JsonValue::make_string("obj2"));
    obj2_val.as_object().set("parent", obj1_val);

    // obj1 引用计数 = 2：obj1_val(1) + Quote(1)
    // obj2 引用计数 = 2：obj2_val(1) + Quote(1)
    EXPECT_EQ(obj1_raw->ref_count, 2);
    EXPECT_EQ(obj2_raw->ref_count, 2);

    // 通过 obj1 获取 obj2（get 会调用 add_ref，obj2 引用计数 +1）
    JsonValue* obj2_from_obj1 = new JsonValue(obj1_val.as_object().get("child"));
    EXPECT_EQ(obj2_from_obj1->get_raw(), obj2_raw);
    EXPECT_EQ(obj2_raw->ref_count, 3); // obj2_val(1) + Quote(1) + get()中的add_ref(1)

    // 通过 obj2 获取 obj1（get 会调用 add_ref，obj1 引用计数 +1）
    JsonValue* obj1_from_obj2 = new JsonValue(obj2_val.as_object().get("parent"));
    EXPECT_EQ(obj1_from_obj2->get_raw(), obj1_raw);
    EXPECT_EQ(obj1_raw->ref_count, 3); // obj1_val(1) + Quote(1) + get()中的add_ref(1)

    // 手动释放临时引用
    delete obj2_from_obj1;
    delete obj1_from_obj2;

    // 引用计数恢复为 2
    EXPECT_EQ(obj1_raw->ref_count, 2);
    EXPECT_EQ(obj2_raw->ref_count, 2);

    // 手动释放 obj1_val（obj1 的引用计数 -1）
    // 当 obj1 被释放时，底层库会先释放 obj1 中的 Quote 引用的实际对象（obj2），然后再释放 Quote 本身
    // 所以 obj2 的引用计数会从 2 变成 1（obj2_val(1)）
    JsonValue* obj1_ptr = new JsonValue(std::move(obj1_val));
    delete obj1_ptr;
    EXPECT_EQ(obj1_raw->ref_count, 1); // obj2 中的 Quote(1)
}

// 测试：数组循环引用时的引用计数
TEST(JsonRefCountTest, array_circular_reference_counting) {
    // 创建两个数组，形成循环引用
    JsonValue arr1_val = JsonValue::make_array();
    JsonValue arr2_val = JsonValue::make_array();
    Json*     arr1_raw = arr1_val.get_raw();
    Json*     arr2_raw = arr2_val.get_raw();

    // 初始状态：每个数组引用计数 = 1
    EXPECT_EQ(arr1_raw->ref_count, 1);
    EXPECT_EQ(arr2_raw->ref_count, 1);

    // arr1 包含 arr2（创建 Quote，arr2 引用计数 +1）
    arr1_val.as_array().push_back(JsonValue::make_string("arr1"));
    arr1_val.as_array().push_back(arr2_val);

    // arr2 引用计数 = 2：arr2_val(1) + Quote(1)
    EXPECT_EQ(arr2_raw->ref_count, 2);

    // arr2 包含 arr1（创建 Quote，arr1 引用计数 +1）
    arr2_val.as_array().push_back(JsonValue::make_string("arr2"));
    arr2_val.as_array().push_back(arr1_val);

    // arr1 引用计数 = 2：arr1_val(1) + Quote(1)
    // arr2 引用计数 = 2：arr2_val(1) + Quote(1)
    EXPECT_EQ(arr1_raw->ref_count, 2);
    EXPECT_EQ(arr2_raw->ref_count, 2);

    // 通过 arr1 获取 arr2（at 会调用 add_ref，arr2 引用计数 +1）
    JsonValue* arr2_from_arr1 = new JsonValue(arr1_val.as_array().at(1));
    EXPECT_EQ(arr2_from_arr1->get_raw(), arr2_raw);
    EXPECT_EQ(arr2_raw->ref_count, 3); // arr2_val(1) + Quote(1) + at()中的add_ref(1)

    // 通过 arr2 获取 arr1（at 会调用 add_ref，arr1 引用计数 +1）
    JsonValue* arr1_from_arr2 = new JsonValue(arr2_val.as_array().at(1));
    EXPECT_EQ(arr1_from_arr2->get_raw(), arr1_raw);
    EXPECT_EQ(arr1_raw->ref_count, 3); // arr1_val(1) + Quote(1) + at()中的add_ref(1)

    // 手动释放临时引用
    delete arr2_from_arr1;
    delete arr1_from_arr2;

    // 引用计数恢复为 2
    EXPECT_EQ(arr1_raw->ref_count, 2);
    EXPECT_EQ(arr2_raw->ref_count, 2);

    // 手动释放 arr1_val（arr1 的引用计数 -1）
    // 当 arr1 被释放时，底层库会先释放 arr1 中的 Quote 引用的实际对象（arr2），然后再释放 Quote 本身
    // 所以 arr2 的引用计数会从 2 变成 1（arr2_val(1)）
    JsonValue* arr1_ptr = new JsonValue(std::move(arr1_val));
    delete arr1_ptr;
    EXPECT_EQ(arr1_raw->ref_count, 1); // arr2 中的 Quote(1)
}

// 测试：多层嵌套对象的引用计数
TEST(JsonRefCountTest, nested_object_reference_counting) {
    // 创建三层嵌套对象
    JsonValue level1_val = JsonValue::make_object();
    JsonValue level2_val = JsonValue::make_object();
    JsonValue level3_val = JsonValue::make_object();
    Json*     level2_raw = level2_val.get_raw();
    Json*     level3_raw = level3_val.get_raw();

    // 初始状态：每个对象引用计数 = 1
    EXPECT_EQ(level2_raw->ref_count, 1);
    EXPECT_EQ(level3_raw->ref_count, 1);

    // 构建嵌套结构：level1 -> level2 -> level3
    level1_val.as_object().set("name", JsonValue::make_string("level1"));
    level1_val.as_object().set("child", level2_val);

    // level2 引用计数 = 2：level2_val(1) + Quote(1)
    EXPECT_EQ(level2_raw->ref_count, 2);

    level2_val.as_object().set("name", JsonValue::make_string("level2"));
    level2_val.as_object().set("child", level3_val);

    // level3 引用计数 = 2：level3_val(1) + Quote(1)
    EXPECT_EQ(level3_raw->ref_count, 2);

    level3_val.as_object().set("name", JsonValue::make_string("level3"));

    // 通过 level1 获取 level2（get 会调用 add_ref，level2 引用计数 +1）
    JsonValue* level2_from_level1 = new JsonValue(level1_val.as_object().get("child"));
    EXPECT_EQ(level2_from_level1->get_raw(), level2_raw);
    EXPECT_EQ(level2_raw->ref_count, 3); // level2_val(1) + Quote(1) + get()中的add_ref(1)

    // 通过 level2_from_level1 获取 level3（get 会调用 add_ref，level3 引用计数 +1）
    JsonValue* level3_from_level2 = new JsonValue(level2_from_level1->as_object().get("child"));
    EXPECT_EQ(level3_from_level2->get_raw(), level3_raw);
    EXPECT_EQ(level3_raw->ref_count, 3); // level3_val(1) + Quote(1) + get()中的add_ref(1)

    // 手动释放临时引用
    delete level3_from_level2;
    EXPECT_EQ(level3_raw->ref_count, 2); // level3_val(1) + Quote(1)

    delete level2_from_level1;
    EXPECT_EQ(level2_raw->ref_count, 2); // level2_val(1) + Quote(1)

    // 手动释放 level1_val（level1 中的 Quote 被释放，level2 引用计数 -1）
    JsonValue* level1_ptr = new JsonValue(std::move(level1_val));
    delete level1_ptr;
    EXPECT_EQ(level2_raw->ref_count, 1); // level2_val(1)

    // 手动释放 level2_val（level2 中的 Quote 被释放，level3 引用计数 -1）
    JsonValue* level2_ptr = new JsonValue(std::move(level2_val));
    delete level2_ptr;
    EXPECT_EQ(level3_raw->ref_count, 1); // level3_val(1)

    // 手动释放 level3_val
    JsonValue* level3_ptr = new JsonValue(std::move(level3_val));
    delete level3_ptr;
    EXPECT_EQ(level3_raw->ref_count, 0);
}

// ======================== Quote 引用正确性测试 ========================

// 测试：Quote 类型的解引用和替换
TEST(JsonQuoteTest, quote_dereference_and_replacement) {
    // 测试数组中 Quote 的解引用
    JsonValue orig_arr = JsonValue::make_string("original");
    Json*     raw_arr  = orig_arr.get_raw();
    JsonValue arr_val  = JsonValue::make_array();
    arr_val.as_array().push_back(orig_arr);
    JsonValue retrieved_arr = arr_val.as_array().at(0);
    EXPECT_EQ(retrieved_arr.get_raw(), raw_arr);
    EXPECT_EQ(retrieved_arr.as_string(), "original");

    // 测试对象中 Quote 的解引用
    JsonValue orig_obj = JsonValue::make_int(999);
    Json*     raw_obj  = orig_obj.get_raw();
    JsonValue obj_val  = JsonValue::make_object();
    obj_val.as_object().set("value", orig_obj);
    JsonValue retrieved_obj = obj_val.as_object().get("value");
    EXPECT_EQ(retrieved_obj.get_raw(), raw_obj);
    EXPECT_EQ(retrieved_obj.as_int(), 999);

    // 测试替换后旧引用仍有效
    JsonValue arr2 = JsonValue::make_array();
    JsonArray a    = arr2.as_array();
    a.push_back(JsonValue::make_string("old"));
    JsonValue old_val = a.at(0);
    a.set(0, JsonValue::make_string("new"));
    EXPECT_EQ(old_val.as_string(), "old");
    EXPECT_EQ(a.at(0).as_string(), "new");

    // 测试迭代器引用
    JsonValue arr3 = JsonValue::make_array();
    JsonValue item = JsonValue::make_string("iter_test");
    Json*     raw  = item.get_raw();
    arr3.as_array().push_back(item);
    for (const auto& elem : arr3.as_array()) {
        EXPECT_EQ(elem.get_raw(), raw);
    }
}

// ======================== 取值正确性测试 ========================

// 测试：嵌套访问和多路径访问的正确性
TEST(JsonValueCorrectnessTest, access_correctness) {
    // 测试嵌套结构访问
    JsonValue  root = JsonValue::make_object();
    JsonObject obj  = root.as_object();
    JsonValue  arr  = JsonValue::make_array();
    arr.as_array().push_back(JsonValue::make_int(100));
    arr.as_array().push_back(JsonValue::make_int(200));
    obj.set("numbers", arr);
    EXPECT_EQ(obj.get("numbers").as_array().at(1).as_int(), 200);

    // 测试多路径访问同一数据
    JsonValue arr2 = JsonValue::make_array();
    arr2.as_array().push_back(JsonValue::make_int(20));
    JsonValue elem1 = arr2.as_array().at(0);
    JsonValue elem2 = arr2.as_array()[0];
    EXPECT_EQ(elem1.get_raw(), elem2.get_raw());
    EXPECT_EQ(elem1.as_int(), 20);
}

// ======================== 内存安全测试 ========================

// 测试：生命周期管理（创建销毁、拷贝、独立生命周期）
TEST(JsonMemorySafetyTest, lifecycle_management) {
    // 测试大量创建销毁
    for (int i = 0; i < 500; ++i) {
        JsonValue val = JsonValue::make_int(i);
        EXPECT_EQ(val.as_int(), i);
    }

    // 测试大量拷贝
    JsonValue              original = JsonValue::make_string("test");
    std::vector<JsonValue> copies;
    for (int i = 0; i < 50; ++i) {
        copies.push_back(original);
    }
    EXPECT_EQ(copies[0].as_string(), "test");

    // 测试容器销毁后元素仍有效
    JsonValue              arr = JsonValue::make_array();
    std::vector<JsonValue> elements;
    for (int i = 0; i < 5; ++i) {
        JsonValue elem = JsonValue::make_int(i * 10);
        arr.as_array().push_back(elem);
        elements.push_back(elem);
    }
    arr = JsonValue::make_null();
    EXPECT_EQ(elements[2].as_int(), 20);

    // 测试连续赋值
    JsonValue val = JsonValue::make_null();
    for (int i = 0; i < 50; ++i) {
        val = JsonValue::make_int(i);
    }
    EXPECT_EQ(val.as_int(), 49);
}

// 测试：迭代器和移动安全性
TEST(JsonMemorySafetyTest, iterator_and_move_safety) {
    // 测试迭代器失效安全
    JsonValue arr = JsonValue::make_array();
    for (int i = 0; i < 3; ++i) {
        arr.as_array().push_back(JsonValue::make_int(i));
    }
    std::vector<JsonValue> values;
    for (const auto& item : arr.as_array()) {
        values.push_back(item);
    }
    arr.as_array().push_back(JsonValue::make_int(999));
    EXPECT_EQ(values[1].as_int(), 1);

    // 测试移动后安全
    JsonValue val1 = JsonValue::make_int(42);
    JsonValue val2(std::move(val1));
    EXPECT_EQ(val1.get_raw(), nullptr);
    EXPECT_EQ(val2.as_int(), 42);
}

// ======================== Double Free 防御测试 ========================

// 测试：移动操作防止 double free
TEST(JsonDoubleFreeTest, move_safety) {
    // 测试移动构造
    JsonValue val1 = JsonValue::make_string("test");
    JsonValue val2(std::move(val1));
    EXPECT_EQ(val1.get_raw(), nullptr);

    // 测试移动赋值
    JsonValue val3 = JsonValue::make_int(100);
    JsonValue val4 = JsonValue::make_null();
    val4           = std::move(val3);
    EXPECT_EQ(val3.get_raw(), nullptr);
    EXPECT_EQ(val4.as_int(), 100);

    // 测试自移动赋值
    Json* raw = val4.get_raw();
    val4      = std::move(val4);
    EXPECT_EQ(val4.get_raw(), raw);

    // 测试连续移动
    JsonValue v1 = JsonValue::make_int(42);
    JsonValue v2(std::move(v1));
    JsonValue v3(std::move(v2));
    EXPECT_EQ(v1.get_raw(), nullptr);
    EXPECT_EQ(v2.get_raw(), nullptr);
    EXPECT_EQ(v3.as_int(), 42);
}

// ======================== 引用链测试 ========================

// 测试：引用链传递的正确性
TEST(JsonRefChainTest, reference_chain) {
    // 测试长引用链
    JsonValue              original = JsonValue::make_string("chain");
    Json*                  orig_raw = original.get_raw();
    std::vector<JsonValue> chain;
    chain.push_back(original);
    for (int i = 0; i < 20; ++i) {
        chain.push_back(chain.back());
    }
    EXPECT_EQ(chain[10].get_raw(), orig_raw);

    // 测试通过数组传递引用链
    JsonValue orig2 = JsonValue::make_int(42);
    Json*     raw2  = orig2.get_raw();
    JsonValue arr1  = JsonValue::make_array();
    arr1.as_array().push_back(orig2);
    JsonValue arr2 = JsonValue::make_array();
    arr2.as_array().push_back(arr1.as_array().at(0));
    EXPECT_EQ(arr2.as_array().at(0).get_raw(), raw2);

    // 测试通过对象传递引用链
    JsonValue orig3 = JsonValue::make_string("value");
    Json*     raw3  = orig3.get_raw();
    JsonValue obj1  = JsonValue::make_object();
    obj1.as_object().set("data", orig3);
    JsonValue obj2 = JsonValue::make_object();
    obj2.as_object().set("data", obj1.as_object().get("data"));
    EXPECT_EQ(obj2.as_object().get("data").get_raw(), raw3);
}

// ======================== 压力测试 ========================

// 测试：综合压力测试（大量引用、多次替换、深层嵌套）
TEST(JsonStressTest, comprehensive_stress) {
    // 测试大量引用
    JsonValue              original = JsonValue::make_string("shared");
    std::vector<JsonValue> refs;
    for (int i = 0; i < 200; ++i) {
        refs.push_back(original);
    }
    EXPECT_EQ(refs[100].get_raw(), original.get_raw());

    // 测试多次替换
    JsonValue arr = JsonValue::make_array();
    arr.as_array().push_back(JsonValue::make_int(0));
    for (int i = 1; i <= 50; ++i) {
        arr.as_array().set(0, JsonValue::make_int(i));
    }
    EXPECT_EQ(arr.as_array()[0].as_int(), 50);

    JsonValue obj = JsonValue::make_object();
    for (int i = 0; i < 50; ++i) {
        obj.as_object().set("key", JsonValue::make_int(i));
    }
    EXPECT_EQ(obj.as_object().get("key").as_int(), 49);

    // 测试深层嵌套
    JsonValue root    = JsonValue::make_object();
    JsonValue current = root;
    for (int i = 0; i < 30; ++i) {
        JsonValue nested = JsonValue::make_object();
        current.as_object().set("next", nested);
        current.as_object().set("level", JsonValue::make_int(i));
        current = nested;
    }
    EXPECT_EQ(root.as_object().get("level").as_int(), 0);
    EXPECT_EQ(root.as_object().get("next").as_object().get("level").as_int(), 1);
}
