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

#include <algorithm>
#include <gtest/gtest.h>
#include <mc/array.h>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_reference.h>
#include <string>

// 测试默认构造
TEST(array_test, default_constructor)
{
    mc::array<int> arr;
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

// 测试指定大小构造
TEST(array_test, size_constructor)
{
    mc::array<int> arr(5);
    EXPECT_EQ(arr.size(), 5);
    EXPECT_FALSE(arr.empty());
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 0);
    }
}

// 测试指定大小和值构造
TEST(array_test, size_value_constructor)
{
    mc::array<int> arr(5, 42);
    EXPECT_EQ(arr.size(), 5);
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 42);
    }
}

// 测试初始化列表构造
TEST(array_test, initializer_list_constructor)
{
    mc::array<int> arr = {1, 2, 3, 4, 5};
    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
    EXPECT_EQ(arr[4], 5);
}

// 测试迭代器范围构造
TEST(array_test, iterator_constructor)
{
    std::vector<int> vec = {10, 20, 30};
    mc::array<int>   arr(vec.begin(), vec.end());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 30);
}

// 测试从 std::vector 构造
TEST(array_test, vector_constructor)
{
    std::vector<int> vec = {7, 8, 9};
    mc::array<int>   arr(vec);
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 7);
    EXPECT_EQ(arr[1], 8);
    EXPECT_EQ(arr[2], 9);
}

// 测试拷贝构造（引用语义）
TEST(array_test, copy_constructor_shared_semantics)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = arr1;

    // 拷贝后应该共享数据
    EXPECT_EQ(arr1.size(), 3);
    EXPECT_EQ(arr2.size(), 3);

    // 修改 arr1 应该影响 arr2
    arr1[0] = 100;
    EXPECT_EQ(arr2[0], 100);

    // 修改 arr2 应该影响 arr1
    arr2[1] = 200;
    EXPECT_EQ(arr1[1], 200);
}

// 测试移动构造
TEST(array_test, move_constructor)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = std::move(arr1);

    EXPECT_EQ(arr2.size(), 3);
    EXPECT_EQ(arr2[0], 1);
    EXPECT_EQ(arr2[1], 2);
    EXPECT_EQ(arr2[2], 3);
}

// 测试赋值运算符（引用语义）
TEST(array_test, copy_assignment_shared_semantics)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2;
    arr2 = arr1;

    // 赋值后应该共享数据
    arr1[0] = 99;
    EXPECT_EQ(arr2[0], 99);
}

// 测试元素访问
TEST(array_test, element_access)
{
    mc::array<int> arr = {10, 20, 30, 40, 50};

    // operator[]
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[4], 50);

    // at()
    EXPECT_EQ(arr.at(0), 10);
    EXPECT_EQ(arr.at(4), 50);
    EXPECT_THROW(arr.at(5), mc::out_of_range_exception);

    // front() 和 back()
    EXPECT_EQ(arr.front(), 10);
    EXPECT_EQ(arr.back(), 50);
}

// 测试 const 元素访问
TEST(array_test, const_element_access)
{
    const mc::array<int> arr = {1, 2, 3};

    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr.at(1), 2);
    EXPECT_EQ(arr.front(), 1);
    EXPECT_EQ(arr.back(), 3);
}

// 测试迭代器
TEST(array_test, iterators)
{
    mc::array<int> arr = {1, 2, 3, 4, 5};

    // 正向迭代器
    int sum = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        sum += *it;
    }
    EXPECT_EQ(sum, 15);

    // 范围 for 循环
    sum = 0;
    for (int val : arr) {
        sum += val;
    }
    EXPECT_EQ(sum, 15);

    // 反向迭代器
    std::vector<int> reversed;
    for (auto it = arr.rbegin(); it != arr.rend(); ++it) {
        reversed.push_back(*it);
    }
    EXPECT_EQ(reversed, std::vector<int>({5, 4, 3, 2, 1}));
}

// 测试容量
TEST(array_test, capacity)
{
    mc::array<int> arr;

    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);

    arr.push_back(1);
    EXPECT_FALSE(arr.empty());
    EXPECT_EQ(arr.size(), 1);

    arr.reserve(100);
    EXPECT_GE(arr.capacity(), 100);
    EXPECT_EQ(arr.size(), 1);
}

// 测试 push_back
TEST(array_test, push_back)
{
    mc::array<int> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);

    // 测试移动语义
    int value = 42;
    arr.push_back(std::move(value));
    EXPECT_EQ(arr[3], 42);
}

// 测试 emplace_back
TEST(array_test, emplace_back)
{
    mc::array<std::string> arr;

    arr.emplace_back("hello");
    arr.emplace_back(5, 'x'); // 构造 "xxxxx"

    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], "hello");
    EXPECT_EQ(arr[1], "xxxxx");
}

// 测试 pop_back
TEST(array_test, pop_back)
{
    mc::array<int> arr = {1, 2, 3, 4, 5};

    arr.pop_back();
    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr.back(), 4);

    arr.pop_back();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr.back(), 3);
}

// 测试 insert
TEST(array_test, insert)
{
    mc::array<int> arr = {1, 2, 5};

    // 插入单个元素
    auto it = arr.insert(arr.begin() + 2, 3);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[2], 3);

    // 插入多个相同元素
    arr.insert(arr.begin() + 3, 2, 4);
    EXPECT_EQ(arr.size(), 6);
    EXPECT_EQ(arr[3], 4);
    EXPECT_EQ(arr[4], 4);

    // 插入初始化列表
    arr.insert(arr.end(), {6, 7, 8});
    EXPECT_EQ(arr.size(), 9);
    EXPECT_EQ(arr[6], 6);
    EXPECT_EQ(arr[7], 7);
    EXPECT_EQ(arr[8], 8);
}

// 测试 emplace
TEST(array_test, emplace)
{
    mc::array<std::string> arr = {"a", "c"};

    auto it = arr.emplace(arr.begin() + 1, 3, 'b'); // 插入 "bbb"
    EXPECT_EQ(*it, "bbb");
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], "a");
    EXPECT_EQ(arr[1], "bbb");
    EXPECT_EQ(arr[2], "c");
}

// 测试 erase
TEST(array_test, erase)
{
    mc::array<int> arr = {1, 2, 3, 4, 5};

    // 删除单个元素
    auto it = arr.erase(arr.begin() + 1);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[1], 3);

    // 删除范围
    arr.erase(arr.begin() + 1, arr.begin() + 3);
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 5);
}

// 测试 resize
TEST(array_test, resize)
{
    mc::array<int> arr = {1, 2, 3};

    // 扩大
    arr.resize(5);
    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[4], 0);

    // 扩大并填充值
    arr.resize(7, 99);
    EXPECT_EQ(arr.size(), 7);
    EXPECT_EQ(arr[5], 99);
    EXPECT_EQ(arr[6], 99);

    // 缩小
    arr.resize(3);
    EXPECT_EQ(arr.size(), 3);
}

// 测试 clear
TEST(array_test, clear)
{
    mc::array<int> arr = {1, 2, 3, 4, 5};

    arr.clear();
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

// 测试 swap
TEST(array_test, swap)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = {4, 5, 6, 7};

    arr1.swap(arr2);

    EXPECT_EQ(arr1.size(), 4);
    EXPECT_EQ(arr1[0], 4);

    EXPECT_EQ(arr2.size(), 3);
    EXPECT_EQ(arr2[0], 1);
}

// 测试比较运算符
TEST(array_test, comparison_operators)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = {1, 2, 3};
    mc::array<int> arr3 = {1, 2, 4};
    mc::array<int> arr4 = {1, 2};

    // ==
    EXPECT_TRUE(arr1 == arr2);
    EXPECT_FALSE(arr1 == arr3);

    // !=
    EXPECT_TRUE(arr1 != arr3);
    EXPECT_FALSE(arr1 != arr2);

    // <
    EXPECT_TRUE(arr1 < arr3);
    EXPECT_TRUE(arr4 < arr1);
    EXPECT_FALSE(arr3 < arr1);

    // <=
    EXPECT_TRUE(arr1 <= arr2);
    EXPECT_TRUE(arr1 <= arr3);

    // >
    EXPECT_TRUE(arr3 > arr1);
    EXPECT_FALSE(arr1 > arr2);

    // >=
    EXPECT_TRUE(arr1 >= arr2);
    EXPECT_TRUE(arr3 >= arr1);
}

// 测试深拷贝（基本类型）
TEST(array_test, deep_copy_basic_type)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = arr1.deep_copy();

    // 深拷贝后应该是独立的数据
    arr1[0] = 100;
    EXPECT_EQ(arr1[0], 100);
    EXPECT_EQ(arr2[0], 1); // arr2 不受影响
}

// 测试深拷贝（支持 deep_copy 的类型）
TEST(array_test, deep_copy_with_variant)
{
    mc::array<mc::variant> arr1;
    arr1.push_back(mc::variant(mc::variants{1, 2, 3}));
    arr1.push_back(mc::variant(42));

    mc::array<mc::variant> arr2 = arr1.deep_copy();

    // 修改 arr1 中的嵌套数组
    arr1[0][0] = 999;

    // arr2 不应该受影响（因为 variant 支持 deep_copy）
    EXPECT_EQ(arr2[0][0], 1);
}

// 测试 copy（浅拷贝）
TEST(array_test, copy_shallow)
{
    mc::array<mc::variant> arr1;
    arr1.push_back(mc::variant(mc::variants{1, 2, 3}));
    arr1.push_back(mc::variant(42));

    mc::array<mc::variant> arr2 = arr1.copy();

    // 直接修改 arr1 的值 arr2 不受影响，因为是浅拷贝
    arr1[1] = 43;
    EXPECT_EQ(arr2[1], 42);

    // 修改 arr1 中的嵌套数组 arr2 受影响，因为浅拷贝不会嵌套拷贝值
    arr1[0][0] = 999;
    EXPECT_EQ(arr2[0][0], 999);
}

// 测试 as_mut
TEST(array_test, as_mut)
{
    mc::array<int> arr     = {1, 2, 3};
    auto&          mut_arr = arr.as_mut();

    // 应该是同一个对象
    mut_arr[0] = 99;
    EXPECT_EQ(arr[0], 99);

    // const 版本
    const mc::array<int> const_arr = {4, 5, 6};
    auto                 mut_copy  = const_arr.as_mut();
    EXPECT_EQ(mut_copy[0], 4);
}

// 测试 to_vector
TEST(array_test, to_vector)
{
    mc::array<int>   arr = {1, 2, 3, 4, 5};
    std::vector<int> vec = arr.to_vector();

    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[4], 5);

    // 修改 vector 不应该影响 array
    vec[0] = 999;
    EXPECT_EQ(arr[0], 1);
}

// 测试使用 mc::variant
TEST(array_test, with_variant)
{
    mc::array<mc::variant> arr;

    arr.push_back(mc::variant(42));
    arr.push_back(mc::variant("hello"));
    arr.push_back(mc::variant(3.14));
    arr.push_back(mc::variant(true));

    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[0], 42);
    EXPECT_EQ(arr[1], "hello");
    EXPECT_EQ(arr[2], 3.14);
    EXPECT_EQ(arr[3], true);
}

// 测试与 mc::variants 的兼容性
TEST(array_test, compatibility_with_variants)
{
    // mc::array<mc::variant> 应该可以替代 mc::variants
    mc::array<mc::variant> arr = {
        mc::variant(1),
        mc::variant("test"),
        mc::variant(2.5),
    };

    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], "test");
    EXPECT_EQ(arr[2], 2.5);
}

// 测试字符串类型
TEST(array_test, with_string)
{
    mc::array<std::string> arr = {"hello", "world", "test"};

    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], "hello");
    EXPECT_EQ(arr[1], "world");
    EXPECT_EQ(arr[2], "test");

    arr.push_back("new");
    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr.back(), "new");
}

// 测试自定义类型
struct custom_type {
    int value;

    custom_type()
        : value(0)
    {
    }

    explicit custom_type(int v)
        : value(v)
    {
    }

    bool operator==(const custom_type& other) const
    {
        return value == other.value;
    }
};

TEST(array_test, with_custom_type)
{
    mc::array<custom_type> arr;

    arr.emplace_back(10);
    arr.emplace_back(20);
    arr.emplace_back(30);

    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].value, 10);
    EXPECT_EQ(arr[1].value, 20);
    EXPECT_EQ(arr[2].value, 30);
}

// 测试空数组的边界情况
TEST(array_test, empty_array_edge_cases)
{
    mc::array<int> arr;

    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
    EXPECT_EQ(arr.data(), nullptr);

    // 空数组的 const 访问应该抛出异常
    const mc::array<int> const_arr;
    EXPECT_THROW(const_arr.at(0), mc::out_of_range_exception);
}

// 测试数据共享
TEST(array_test, data_sharing)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = arr1;
    mc::array<int> arr3 = arr1;

    // 三个对象共享同一份数据
    arr1[0] = 100;
    EXPECT_EQ(arr2[0], 100);
    EXPECT_EQ(arr3[0], 100);

    arr2[1] = 200;
    EXPECT_EQ(arr1[1], 200);
    EXPECT_EQ(arr3[1], 200);

    arr3[2] = 300;
    EXPECT_EQ(arr1[2], 300);
    EXPECT_EQ(arr2[2], 300);
}

// 测试嵌套数组
TEST(array_test, nested_array)
{
    mc::array<mc::array<int>> nested;

    nested.push_back(mc::array<int>{1, 2, 3});
    nested.push_back(mc::array<int>{4, 5, 6});
    nested.push_back(mc::array<int>{7, 8, 9});

    EXPECT_EQ(nested.size(), 3);
    EXPECT_EQ(nested[0].size(), 3);
    EXPECT_EQ(nested[0][0], 1);
    EXPECT_EQ(nested[1][1], 5);
    EXPECT_EQ(nested[2][2], 9);
}

// 测试与 std::vector 的比较运算符
TEST(array_test, comparison_with_vector)
{
    mc::array<int>   arr  = {1, 2, 3, 4, 5};
    std::vector<int> vec1 = {1, 2, 3, 4, 5};
    std::vector<int> vec2 = {1, 2, 3, 4, 6};
    std::vector<int> vec3 = {1, 2, 3};

    // ==
    EXPECT_TRUE(arr == vec1);
    EXPECT_TRUE(vec1 == arr);
    EXPECT_FALSE(arr == vec2);
    EXPECT_FALSE(vec2 == arr);

    // !=
    EXPECT_FALSE(arr != vec1);
    EXPECT_FALSE(vec1 != arr);
    EXPECT_TRUE(arr != vec2);
    EXPECT_TRUE(vec2 != arr);

    // <
    EXPECT_TRUE(arr < vec2);
    EXPECT_FALSE(vec2 < arr);
    EXPECT_FALSE(arr < vec3);
    EXPECT_TRUE(vec3 < arr);

    // <=
    EXPECT_TRUE(arr <= vec1);
    EXPECT_TRUE(vec1 <= arr);
    EXPECT_TRUE(arr <= vec2);
    EXPECT_TRUE(vec3 <= arr);

    // >
    EXPECT_FALSE(arr > vec1);
    EXPECT_FALSE(vec1 > arr);
    EXPECT_TRUE(arr > vec3);
    EXPECT_FALSE(vec3 > arr);

    // >=
    EXPECT_TRUE(arr >= vec1);
    EXPECT_TRUE(vec1 >= arr);
    EXPECT_TRUE(arr >= vec3);
    EXPECT_FALSE(vec3 >= arr);
}

// 测试空 array 与空 vector 的比较
TEST(array_test, empty_comparison_with_vector)
{
    mc::array<int>   arr;
    std::vector<int> vec;

    EXPECT_TRUE(arr == vec);
    EXPECT_TRUE(vec == arr);
    EXPECT_FALSE(arr != vec);
    EXPECT_FALSE(vec != arr);
}

// 测试与 std::vector<mc::variant> 的兼容性
TEST(array_test, compatibility_with_std_vector_variant)
{
    mc::array<mc::variant>   arr = {mc::variant(1), mc::variant(2), mc::variant(3)};
    std::vector<mc::variant> vec = {mc::variant(1), mc::variant(2), mc::variant(3)};

    // 应该可以相互比较
    EXPECT_TRUE(arr == vec);
    EXPECT_TRUE(vec == arr);

    vec.push_back(mc::variant(4));
    EXPECT_FALSE(arr == vec);
    EXPECT_TRUE(arr != vec);
}

// 测试 mc::array 的 variant_reference 特殊构造函数
TEST(array_test, variant_reference_initializer_list_constructor)
{
    // 创建一个包含 variant 的数组
    mc::array<mc::variant> source_array = {mc::variant(42), mc::variant("hello"), mc::variant(true)};

    // 直接创建 variant_reference 对象
    mc::variant_reference ref1(source_array[0]);
    mc::variant_reference ref2(source_array[1]);
    mc::variant_reference ref3(source_array[2]);

    // 使用 variant_reference 初始化列表构造新的 array
    // 应该创建 mc::array<variant> 而不是 mc::array<variant_reference>
    mc::array<mc::variant> new_array = {ref1, ref2, ref3};

    // 验证结果
    EXPECT_EQ(new_array.size(), 3);
    EXPECT_EQ(new_array[0].as_int32(), 42);
    EXPECT_EQ(new_array[1].as_string(), "hello");
    EXPECT_EQ(new_array[2].as_bool(), true);

    // 验证类型：应该是 variant 类型，不是 variant_reference 类型
    EXPECT_TRUE(new_array[0].is_int32());
    EXPECT_TRUE(new_array[1].is_string());
    EXPECT_TRUE(new_array[2].is_bool());
}

// 测试 mc::array 的 variant_reference 迭代器构造函数
TEST(array_test, variant_reference_iterator_range_constructor)
{
    // 创建一个包含 variant 的数组
    mc::array<mc::variant> source_array = {mc::variant(100), mc::variant(200), mc::variant(300)};

    // 获取 variant_reference 迭代器
    std::vector<mc::variant_reference> refs;
    refs.emplace_back(source_array[0]);
    refs.emplace_back(source_array[1]);
    refs.emplace_back(source_array[2]);

    // 使用 variant_reference 迭代器范围构造新的 array
    mc::array<mc::variant> new_array(refs.begin(), refs.end());

    // 验证结果
    EXPECT_EQ(new_array.size(), 3);
    EXPECT_EQ(new_array[0].as_int32(), 100);
    EXPECT_EQ(new_array[1].as_int32(), 200);
    EXPECT_EQ(new_array[2].as_int32(), 300);
}

// 测试链式访问后的构造函数
TEST(array_test, variant_reference_chained_access_constructor)
{
    // 创建一个嵌套的 variant 结构
    mc::dict nested_dict;
    nested_dict["value"] = mc::variant(42);

    mc::array<mc::variant> source_array = {mc::variant(nested_dict)};

    // 链式访问获取 variant_reference
    auto ref = source_array[0]["value"];

    // 使用链式访问的 variant_reference 构造新的 array
    mc::array<mc::variant> new_array = {ref};

    // 验证结果
    EXPECT_EQ(new_array.size(), 1);
    EXPECT_EQ(new_array[0].as_int32(), 42);
    EXPECT_TRUE(new_array[0].is_int32());
}

// 测试类型安全性：确保不会创建 variant_reference 类型的数组
TEST(array_test, variant_reference_type_safety_check)
{
    mc::array<mc::variant> source_array = {mc::variant(123)};
    auto                   ref          = source_array[0];

    // 构造新数组
    mc::array<mc::variant> new_array = {ref};

    // 验证新数组的元素类型是 variant，不是 variant_reference
    // 这通过编译时类型检查来确保
    static_assert(std::is_same_v<decltype(new_array[0]), mc::variant&>,
                  "Array element should be variant, not variant_reference");
}

TEST(array_test, assign_count_value)
{
    mc::array<int> arr;

    // 测试 assign(count, value)
    arr.assign(5, 42);
    EXPECT_EQ(arr.size(), 5);
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 42);
    }

    // 测试重新 assign
    arr.assign(3, 100);
    EXPECT_EQ(arr.size(), 3);
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], 100);
    }

    // 测试 assign 到 0 个元素
    arr.assign(0, 999);
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

TEST(array_test, assign_iterator_range)
{
    mc::array<int> arr;

    // 准备源数据
    std::vector<int> source = {10, 20, 30, 40, 50};

    // 测试 assign(first, last)
    arr.assign(source.begin(), source.end());
    EXPECT_EQ(arr.size(), 5);
    for (size_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], source[i]);
    }

    // 测试部分范围 assign
    arr.assign(source.begin() + 1, source.begin() + 4);
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], 20);
    EXPECT_EQ(arr[1], 30);
    EXPECT_EQ(arr[2], 40);

    // 测试空范围 assign
    arr.assign(source.end(), source.end());
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

TEST(array_test, assign_initializer_list)
{
    mc::array<std::string> arr;

    // 测试 assign(initializer_list)
    arr.assign({"hello", "world", "test"});
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0], "hello");
    EXPECT_EQ(arr[1], "world");
    EXPECT_EQ(arr[2], "test");

    // 测试重新 assign
    arr.assign({"foo", "bar"});
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], "foo");
    EXPECT_EQ(arr[1], "bar");

    // 测试空初始化列表 assign
    arr.assign({});
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

TEST(array_test, assign_shared_semantics)
{
    mc::array<int> arr1 = {1, 2, 3};
    mc::array<int> arr2 = arr1; // 共享数据

    // 对 arr1 进行 assign 操作
    arr1.assign(2, 999);

    // arr2 应该受到影响（因为共享数据）
    EXPECT_EQ(arr2.size(), 2);
    EXPECT_EQ(arr2[0], 999);
    EXPECT_EQ(arr2[1], 999);

    // 验证它们仍然共享数据
    arr1[0] = 888;
    EXPECT_EQ(arr2[0], 888);
}

// ========== 排序算法兼容性测试 ==========

TEST(array_test, std_sort_basic)
{
    // 测试基本的 std::sort 排序
    mc::array<int> arr = {5, 2, 8, 1, 9, 3};

    std::sort(arr.begin(), arr.end());

    EXPECT_EQ(arr.size(), 6);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 5);
    EXPECT_EQ(arr[4], 8);
    EXPECT_EQ(arr[5], 9);
}

TEST(array_test, std_sort_string)
{
    // 测试字符串排序
    mc::array<std::string> arr = {"zebra", "apple", "banana", "cherry"};

    std::sort(arr.begin(), arr.end());

    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[0], "apple");
    EXPECT_EQ(arr[1], "banana");
    EXPECT_EQ(arr[2], "cherry");
    EXPECT_EQ(arr[3], "zebra");
}

TEST(array_test, std_sort_custom_comparator)
{
    // 测试自定义比较函数的排序
    mc::array<int> arr = {5, 2, 8, 1, 9, 3};

    // 降序排序
    std::sort(arr.begin(), arr.end(), std::greater<int>());

    EXPECT_EQ(arr.size(), 6);
    EXPECT_EQ(arr[0], 9);
    EXPECT_EQ(arr[1], 8);
    EXPECT_EQ(arr[2], 5);
    EXPECT_EQ(arr[3], 3);
    EXPECT_EQ(arr[4], 2);
    EXPECT_EQ(arr[5], 1);
}

TEST(array_test, std_sort_lambda_comparator)
{
    // 测试 lambda 比较函数的排序
    mc::array<std::string> arr = {"apple", "pie", "a", "banana"};

    // 按字符串长度排序
    std::sort(arr.begin(), arr.end(), [](const std::string& a, const std::string& b) {
        return a.length() < b.length();
    });

    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[0], "a");      // 长度 1
    EXPECT_EQ(arr[1], "pie");    // 长度 3
    EXPECT_EQ(arr[2], "apple");  // 长度 5
    EXPECT_EQ(arr[3], "banana"); // 长度 6
}

TEST(array_test, std_stable_sort)
{
    // 测试稳定排序
    struct item {
        int value;
        int order;

        item()
            : value(0), order(0)
        {
        }
        item(int v, int o)
            : value(v), order(o)
        {
        }

        bool operator<(const item& other) const
        {
            return value < other.value;
        }

        bool operator==(const item& other) const
        {
            return value == other.value && order == other.order;
        }
    };

    mc::array<item> arr;
    arr.emplace_back(3, 1);
    arr.emplace_back(1, 2);
    arr.emplace_back(3, 3);
    arr.emplace_back(2, 4);
    arr.emplace_back(3, 5);

    std::stable_sort(arr.begin(), arr.end());

    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(arr[0].value, 1);
    EXPECT_EQ(arr[0].order, 2);
    EXPECT_EQ(arr[1].value, 2);
    EXPECT_EQ(arr[1].order, 4);
    // 稳定排序保持相同值的相对顺序
    EXPECT_EQ(arr[2].value, 3);
    EXPECT_EQ(arr[2].order, 1);
    EXPECT_EQ(arr[3].value, 3);
    EXPECT_EQ(arr[3].order, 3);
    EXPECT_EQ(arr[4].value, 3);
    EXPECT_EQ(arr[4].order, 5);
}

TEST(array_test, std_partial_sort)
{
    // 测试部分排序
    mc::array<int> arr = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    // 只排序前 3 个最小的元素
    std::partial_sort(arr.begin(), arr.begin() + 3, arr.end());

    EXPECT_EQ(arr.size(), 10);
    // 前 3 个元素应该是最小的且已排序
    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 1);
    EXPECT_EQ(arr[2], 2);
    // 后面的元素顺序不确定，但应该都大于等于 arr[2]
    for (size_t i = 3; i < arr.size(); ++i) {
        EXPECT_GE(arr[i], arr[2]);
    }
}

TEST(array_test, std_nth_element)
{
    // 测试 nth_element 算法
    mc::array<int> arr = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    // 找到第 5 小的元素（索引 4）
    std::nth_element(arr.begin(), arr.begin() + 4, arr.end());

    EXPECT_EQ(arr.size(), 10);
    EXPECT_EQ(arr[4], 4); // 第 5 小的元素应该是 4

    // 验证分区性质：前面的元素都小于等于 arr[4]，后面的都大于等于 arr[4]
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_LE(arr[i], arr[4]);
    }
    for (size_t i = 5; i < arr.size(); ++i) {
        EXPECT_GE(arr[i], arr[4]);
    }
}

TEST(array_test, std_sort_empty_array)
{
    // 测试空数组的排序
    mc::array<int> arr;

    // 空数组排序不应该崩溃
    std::sort(arr.begin(), arr.end());

    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

TEST(array_test, std_sort_single_element)
{
    // 测试单元素数组的排序
    mc::array<int> arr = {42};

    std::sort(arr.begin(), arr.end());

    EXPECT_EQ(arr.size(), 1);
    EXPECT_EQ(arr[0], 42);
}

TEST(array_test, std_sort_shared_semantics)
{
    // 测试共享语义下的排序
    mc::array<int> arr1 = {5, 2, 8, 1, 9, 3};
    mc::array<int> arr2 = arr1; // 共享数据

    // 对 arr1 进行排序
    std::sort(arr1.begin(), arr1.end());

    // arr2 应该也被排序（因为共享数据）
    EXPECT_EQ(arr2.size(), 6);
    EXPECT_EQ(arr2[0], 1);
    EXPECT_EQ(arr2[1], 2);
    EXPECT_EQ(arr2[2], 3);
    EXPECT_EQ(arr2[3], 5);
    EXPECT_EQ(arr2[4], 8);
    EXPECT_EQ(arr2[5], 9);
}
