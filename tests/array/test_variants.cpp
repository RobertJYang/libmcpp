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
#include <mc/array.h>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_reference.h>

TEST(variants_test, StronglyTypedIntArrayAccess) {
    // 测试 mc::array<int> 的 mc::variant 包装和访问
    mc::array<int> arr = {1, 2, 3, 4, 5};
    mc::variant    v   = arr; // 隐式转换

    // 验证是 array 类型
    EXPECT_TRUE(v.is_array());

    // 测试索引读取（带类型转换）
    EXPECT_EQ(v[0].as_int32(), 1);
    EXPECT_EQ(v[1].as_int32(), 2);
    EXPECT_EQ(v[4].as_int32(), 5);

    // 测试索引写入（带类型转换）
    v[0] = 100;
    v[1] = 200;
    EXPECT_EQ(v[0].as_int32(), 100);
    EXPECT_EQ(v[1].as_int32(), 200);

    // 测试取回原始类型
    mc::array<int> arr2 = v.as<mc::array<int>>();
    EXPECT_EQ(arr2.size(), 5);
    EXPECT_EQ(arr2[0], 100);
    EXPECT_EQ(arr2[1], 200);
    EXPECT_EQ(arr2[2], 3);
}

TEST(variants_test, StronglyTypedStringArrayAccess) {
    mc::array<std::string> arr = {"hello", "world", "test"};
    mc::variant            v   = arr;

    EXPECT_TRUE(v.is_array());

    // 读取字符串
    EXPECT_EQ(v[0].as_string(), "hello");
    EXPECT_EQ(v[1].as_string(), "world");

    // 修改字符串
    v[0] = "HELLO";
    EXPECT_EQ(v[0].as_string(), "HELLO");

    // 取回原始类型
    mc::array<std::string> arr2 = v.as<mc::array<std::string>>();
    EXPECT_EQ(arr2[0], "HELLO");
    EXPECT_EQ(arr2[1], "world");
}

TEST(variants_test, StronglyTypedArraySharedSemantics) {
    // 验证强类型数组的共享语义
    mc::array<int> arr1 = {10, 20, 30};
    mc::variant    v1   = arr1;

    // 拷贝 variant
    mc::variant v2 = v1;

    // 修改 v1
    v1[0] = 100;

    // v2 应该受影响（因为共享内部数据）
    EXPECT_EQ(v2[0].as_int32(), 100);

    // 取回的 array 也共享数据
    mc::array<int> arr2 = v1.as<mc::array<int>>();
    EXPECT_EQ(arr2[0], 100);

    // 修改 arr2
    arr2[1] = 200;

    // v1 应该受影响
    EXPECT_EQ(v1[1].as_int32(), 200);
}

TEST(variants_test, StronglyTypedArrayInDict) {
    // 测试在 dict 中使用强类型数组
    mc::array<int>         arr1 = {1, 2, 3};
    mc::array<std::string> arr2 = {"a", "b", "c"};

    mc::dict obj;
    obj["numbers"] = arr1;
    obj["strings"] = arr2;

    EXPECT_TRUE(obj["numbers"].is_array());
    EXPECT_TRUE(obj["strings"].is_array());

    // 访问嵌套元素
    EXPECT_EQ(obj["numbers"][0].as_int32(), 1);
    EXPECT_EQ(obj["strings"][1].as_string(), "b");

    // 修改嵌套元素
    obj["numbers"][0] = 100;
    EXPECT_EQ(obj["numbers"][0].as_int32(), 100);
}

TEST(variants_test, StronglyTypedArrayMixedWithVariants) {
    // 测试强类型数组与普通 variants 的混合使用
    mc::array<int> int_arr = {1, 2, 3};
    mc::variants   var_arr = {10, 20, 30};

    mc::variant  v1 = int_arr;       // 强类型数组
    mc::variant  v2 = var_arr;       // 普通数组
    mc::variants v3 = v1.as_array(); // 强类型数组可以转换成 mc::variants（保持内部引用）

    EXPECT_TRUE(v1.is_array());
    EXPECT_TRUE(v2.is_array());

    // 共享底层数据
    EXPECT_EQ(v3.data(), v1.get_array().data());

    // 两种方式都可以访问
    EXPECT_EQ(v1[0].as_int32(), 1);
    EXPECT_EQ(v2[0].as_int32(), 10);
    EXPECT_EQ(v3[0].as_int32(), int_arr[0]);

    // 放入同一个 dict
    mc::dict obj;
    obj["ext_arr"]  = v1;
    obj["var_arr"]  = v2;
    obj["ext_arr1"] = v3;

    EXPECT_EQ(obj["ext_arr"][0].as_int32(), 1);
    EXPECT_EQ(obj["var_arr"][0].as_int32(), 10);
    EXPECT_EQ(obj["ext_arr1"][0].as_int32(), 1);
}

// ========== 强类型数组类型兼容性测试 ==========

TEST(variants_test, StronglyTypedArrayRejectIncompatibleType) {
    // 测试强类型数组拒绝不兼容的类型
    mc::array<int> int_arr = {1, 2, 3};
    mc::variant    v       = int_arr;

    EXPECT_TRUE(v.is_array());

    // 尝试设置为字符串应该抛出异常
    EXPECT_THROW({ v[0] = "text"; }, mc::bad_cast_exception);

    // 验证字符串赋值失败后值不变
    EXPECT_EQ(v[0].as_int32(), 1);

    // 浮点数和布尔值会被隐式转换为 int（3.14 -> 3, true -> 1）
    // 所以这些赋值不会抛出异常，而是会转换
    v[1] = 3.14;
    EXPECT_EQ(v[1].as_int32(), 3); // 3.14 被截断为 3

    v[2] = true;
    EXPECT_EQ(v[2].as_int32(), 1); // true 被转换为 1
}

TEST(variants_test, StronglyTypedArrayAcceptCompatibleType) {
    // 测试强类型数组接受兼容的类型
    mc::array<int> int_arr = {1, 2, 3};
    mc::variant    v       = int_arr;

    // 设置为相同类型
    v[0] = 100;
    EXPECT_EQ(v[0].as_int32(), 100);

    // 设置为不同的整数类型（可以转换）
    v[1] = static_cast<int64_t>(200);
    EXPECT_EQ(v[1].as_int32(), 200);

    // 设置为 unsigned int（可以转换）
    v[2] = static_cast<unsigned int>(300);
    EXPECT_EQ(v[2].as_int32(), 300);
}

TEST(variants_test, WeaklyTypedArrayAcceptAnyType) {
    // 测试弱类型数组可以接受任何类型
    mc::variants weak_arr = {1, 2, 3};
    mc::variant  v        = weak_arr;

    EXPECT_TRUE(v.is_array());

    // 可以设置为字符串
    v[0] = "text";
    EXPECT_EQ(v[0].as_string(), "text");

    // 可以设置为浮点数
    v[1] = 3.14;
    EXPECT_DOUBLE_EQ(v[1].as_double(), 3.14);

    // 可以设置为布尔值
    v[2] = true;
    EXPECT_EQ(v[2].as_bool(), true);
}

TEST(variants_test, StronglyTypedStringArrayRejectNonString) {
    // 测试强类型字符串数组拒绝非字符串类型
    mc::array<std::string> str_arr = {"hello", "world", "test"};
    mc::variant            v       = str_arr;

    EXPECT_TRUE(v.is_array());

    // 整数、浮点数、布尔值会被转换为字符串表示
    v[0] = 123;
    EXPECT_EQ(v[0].as_string(), "123"); // 123 转换为 "123"

    v[1] = 3.14;
    EXPECT_EQ(v[1].as_string(), "3.14"); // 3.14 转换为 "3.14"

    v[2] = true;
    EXPECT_EQ(v[2].as_string(), "true"); // true 转换为 "true"
}

TEST(variants_test, StronglyTypedStringArrayAcceptString) {
    // 测试强类型字符串数组接受字符串类型
    mc::array<std::string> str_arr = {"hello", "world", "test"};
    mc::variant            v       = str_arr;

    // 设置为字符串字面量
    v[0] = "HELLO";
    EXPECT_EQ(v[0].as_string(), "HELLO");

    // 设置为 std::string
    v[1] = std::string("WORLD");
    EXPECT_EQ(v[1].as_string(), "WORLD");

    // 设置为 const char*
    const char* new_text = "TEST";
    v[2]                 = new_text;
    EXPECT_EQ(v[2].as_string(), "TEST");
}

// ========== variants 与 mc::array<T> 比较运算符测试 ==========

TEST(variants_test, ComparisonWithArrayInt) {
    // 测试 variants 与 mc::array<int> 的比较
    mc::array<int> arr = {1, 2, 3, 4, 5};
    mc::variants   var_arr{1, 2, 3, 4, 5};

    // 相等比较
    EXPECT_TRUE(var_arr == arr);
    EXPECT_TRUE(arr == var_arr);
    EXPECT_FALSE(var_arr != arr);
    EXPECT_FALSE(arr != var_arr);

    // 不等比较
    mc::array<int> arr2 = {1, 2, 3, 4, 6};
    EXPECT_FALSE(var_arr == arr2);
    EXPECT_TRUE(var_arr != arr2);

    // 小于比较
    mc::array<int> arr3 = {1, 2, 3, 4};
    EXPECT_FALSE(var_arr < arr3);
    EXPECT_TRUE(arr3 < var_arr);

    // 大于比较
    EXPECT_TRUE(var_arr > arr3);
    EXPECT_FALSE(arr3 > var_arr);

    // 小于等于
    EXPECT_FALSE(var_arr <= arr3);
    EXPECT_TRUE(arr3 <= var_arr);

    // 大于等于
    EXPECT_TRUE(var_arr >= arr3);
    EXPECT_FALSE(arr3 >= var_arr);
}

TEST(variants_test, ComparisonWithArraySharedReference) {
    // 测试共享引用时的快速路径
    mc::array<int> arr = {10, 20, 30};
    mc::variants   var_arr(arr); // 从 array 构造

    // 共享同一个内部数据，应该快速返回 true
    EXPECT_TRUE(var_arr == arr);
    EXPECT_TRUE(arr == var_arr);

    // 修改其中一个，另一个也受影响
    var_arr[0] = 100;
    EXPECT_EQ(arr[0], 100);

    // 共享引用时的比较仍然为 true
    EXPECT_TRUE(var_arr == arr);
}

TEST(variants_test, ComparisonWithArrayEmpty) {
    // 测试空数组的比较
    mc::array<int> empty_arr;
    mc::variants   empty_var;

    // 空数组相等
    EXPECT_TRUE(empty_var == empty_arr);
    EXPECT_TRUE(empty_arr == empty_var);
    EXPECT_FALSE(empty_var != empty_arr);
    EXPECT_FALSE(empty_arr != empty_var);

    // 非空数组与空数组比较
    mc::array<int> arr = {1, 2, 3};
    mc::variants   var_arr{1, 2, 3};

    EXPECT_FALSE(empty_var == arr);
    EXPECT_TRUE(empty_var != arr);
    EXPECT_TRUE(empty_var < arr);
    EXPECT_FALSE(arr < empty_var);
}

// ========== variants 与 std::vector<T> 比较运算符测试 ==========

TEST(variants_test, ComparisonWithVectorInt) {
    // 测试 variants 与 std::vector<int> 的比较
    std::vector<int> vec = {1, 2, 3, 4, 5};
    mc::variants     var_arr{1, 2, 3, 4, 5};

    // 相等比较
    EXPECT_TRUE(var_arr == vec);
    EXPECT_TRUE(vec == var_arr);
    EXPECT_FALSE(var_arr != vec);
    EXPECT_FALSE(vec != var_arr);

    // 不等比较
    std::vector<int> vec2 = {1, 2, 3, 4, 6};
    EXPECT_FALSE(var_arr == vec2);
    EXPECT_TRUE(var_arr != vec2);

    // 小于比较
    std::vector<int> vec3 = {1, 2, 3, 4};
    EXPECT_FALSE(var_arr < vec3);
    EXPECT_TRUE(vec3 < var_arr);

    // 大于比较
    EXPECT_TRUE(var_arr > vec3);
    EXPECT_FALSE(vec3 > var_arr);

    // 小于等于
    EXPECT_FALSE(var_arr <= vec3);
    EXPECT_TRUE(vec3 <= var_arr);

    // 大于等于
    EXPECT_TRUE(var_arr >= vec3);
    EXPECT_FALSE(vec3 >= var_arr);
}

TEST(variants_test, ComparisonWithVectorString) {
    // 测试 variants 与 std::vector<std::string> 的比较
    std::vector<std::string> vec = {"hello", "world", "test"};
    mc::variants             var_arr{"hello", "world", "test"};

    // 相等比较
    EXPECT_TRUE(var_arr == vec);
    EXPECT_TRUE(vec == var_arr);
    EXPECT_FALSE(var_arr != vec);

    // 不等比较
    std::vector<std::string> vec2 = {"hello", "world", "test2"};
    EXPECT_FALSE(var_arr == vec2);
    EXPECT_TRUE(var_arr != vec2);

    // 小于比较（字典序）
    std::vector<std::string> vec3 = {"a", "b", "c"};
    EXPECT_FALSE(var_arr < vec3);
    EXPECT_TRUE(vec3 < var_arr);
}

TEST(variants_test, ComparisonWithVectorEmpty) {
    // 测试 variants 与空 vector 的比较
    std::vector<int> empty_vec;
    mc::variants     empty_var;

    // 空 vector 相等
    EXPECT_TRUE(empty_var == empty_vec);
    EXPECT_TRUE(empty_vec == empty_var);
    EXPECT_FALSE(empty_var != empty_vec);

    // 非空 vector 与空 variants 比较
    std::vector<int> vec = {1, 2, 3};
    mc::variants     var_arr{1, 2, 3};

    EXPECT_FALSE(empty_var == vec);
    EXPECT_TRUE(empty_var != vec);
    EXPECT_TRUE(empty_var < vec);
    EXPECT_FALSE(vec < empty_var);
}

TEST(variants_test, ComparisonWithVectorLexicographic) {
    // 测试字典序比较
    std::vector<int> vec1 = {1, 2, 3};
    std::vector<int> vec2 = {1, 2, 4};
    std::vector<int> vec3 = {1, 3};

    mc::variants var_arr1{1, 2, 3};
    mc::variants var_arr2{1, 2, 4};
    mc::variants var_arr3{1, 3};

    // vec1 < vec2
    EXPECT_TRUE(vec1 < vec2);
    EXPECT_TRUE(var_arr1 < vec2);
    EXPECT_TRUE(vec1 < var_arr2);

    // vec1 < vec3
    EXPECT_TRUE(vec1 < vec3);
    EXPECT_TRUE(var_arr1 < vec3);
    EXPECT_TRUE(vec1 < var_arr3);

    // vec2 < vec3（字典序：{1,2,4} < {1,3} 因为在第二个元素处 2 < 3）
    EXPECT_TRUE(vec2 < vec3);
    EXPECT_TRUE(var_arr2 < vec3);
    EXPECT_TRUE(vec2 < var_arr3);

    // vec3 > vec2
    EXPECT_TRUE(vec3 > vec2);
    EXPECT_TRUE(var_arr3 > vec2);
    EXPECT_TRUE(vec3 > var_arr2);
}

TEST(variants_test, variant_reference_initializer_list_constructor) {
    // 创建一个包含 mc::variant 的数组
    mc::array<mc::variant> source_array = {mc::variant(1.5), mc::variant("world"), mc::variant(false)};

    // 直接创建 variant_reference 对象
    mc::variant_reference ref1(source_array[0]);
    mc::variant_reference ref2(source_array[1]);
    mc::variant_reference ref3(source_array[2]);

    // 使用 variant_reference 初始化列表构造 variants
    mc::variants new_variants = {mc::variant_reference{ref1}, mc::variant_reference{ref2}, mc::variant_reference{ref3}};

    // 验证结果
    EXPECT_EQ(new_variants.size(), 3);
    EXPECT_EQ(new_variants[0].as_double(), 1.5);
    EXPECT_EQ(new_variants[1].as_string(), "world");
    EXPECT_EQ(new_variants[2].as_bool(), false);

    // 验证类型：应该是 mc::variant 类型，不是 variant_reference 类型
    EXPECT_TRUE(new_variants[0].is_double());
    EXPECT_TRUE(new_variants[1].is_string());
    EXPECT_TRUE(new_variants[2].is_bool());
}

// 测试 mc::variants 的 variant_reference 迭代器构造函数
TEST(variants_test, variant_reference_iterator_range_constructor) {
    // 创建一个包含 mc::variant 的数组
    mc::array<mc::variant> source_array = {mc::variant("test"), mc::variant(3.14), mc::variant(999)};

    // 获取 variant_reference 迭代器
    std::vector<mc::variant_reference> refs;
    refs.emplace_back(source_array[0]);
    refs.emplace_back(source_array[1]);
    refs.emplace_back(source_array[2]);

    mc::variants new_variants(refs.begin(), refs.end());

    // 验证结果
    EXPECT_EQ(new_variants.size(), 3);
    EXPECT_EQ(new_variants[0].as_string(), "test");
    EXPECT_EQ(new_variants[1].as_double(), 3.14);
    EXPECT_EQ(new_variants[2].as_int32(), 999);
}
