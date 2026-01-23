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

/**
 * @file test_variant_containers.cpp
 * @brief 测试 variant 与内置容器类型（dict、array等）的交互
 */
#include "test_variant_helpers.h"
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant.h>
#include <mc/variant/variant_common.h>
#include <stdexcept>
#include <typeindex>
#include <test_utilities/test_base.h>

namespace mc {
namespace test {

class VariantContainersTest : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 在每个测试前执行
        sample_dict  = create_sample_dict();
        sample_array = create_sample_array();
    }

    void TearDown() override {
        // 在每个测试后执行
    }

    dict create_sample_dict() {
        dict m_dict;
        m_dict["int_value"]    = 42;
        m_dict["double_value"] = 3.14;
        m_dict["string_value"] = "test string";
        m_dict["bool_value"]   = true;
        return m_dict;
    }

    variants create_sample_array() {
        variants arr;
        arr.push_back(1);
        arr.push_back(2.5);
        arr.push_back("array item");
        arr.push_back(false);
        return arr;
    }

    dict     sample_dict;
    variants sample_array;
};

/**
 * @brief 测试 dict 类型的 variant 创建
 */
TEST_F(VariantContainersTest, DictConstruction) {
    variant v(sample_dict);

    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    ASSERT_FALSE(v.is_null()) << "variant 不应该是 null";
    ASSERT_FALSE(v.is_array()) << "variant 不应该是数组类型";
}

/**
 * @brief 测试 dict 类型的 variant 转换
 */
TEST_F(VariantContainersTest, DictConversion) {
    variant v(sample_dict);
    dict    result = v.as<dict>();

    ASSERT_EQ(result.size(), sample_dict.size()) << "dict 大小不匹配";

    // 验证键值对是否正确
    ASSERT_TRUE(result.contains("int_value")) << "结果 dict 缺少键";
    ASSERT_EQ(result["int_value"].as<int>(), 42) << "int 值不匹配";

    ASSERT_TRUE(result.contains("double_value")) << "结果 dict 缺少键";
    ASSERT_DOUBLE_EQ(result["double_value"].as<double>(), 3.14) << "double 值不匹配";

    ASSERT_TRUE(result.contains("string_value")) << "结果 dict 缺少键";
    ASSERT_EQ(result["string_value"].as_string(), "test string") << "string 值不匹配";

    ASSERT_TRUE(result.contains("bool_value")) << "结果 dict 缺少键";
    ASSERT_EQ(result["bool_value"].as_bool(), true) << "bool 值不匹配";
}

/**
 * @brief 测试 dict 访问操作
 */
TEST_F(VariantContainersTest, DictAccess) {
    variant     v(sample_dict);
    const dict& d = v.get_object();

    ASSERT_EQ(d.size(), sample_dict.size()) << "dict 大小不匹配";

    // 测试键访问
    ASSERT_TRUE(d.contains("int_value")) << "dict 缺少键";
    ASSERT_EQ(d["int_value"].as<int>(), 42) << "int 值不匹配";

    // 测试默认值获取
    ASSERT_EQ(d.get("non_existent", 100).as<int>(), 100) << "默认值不正确";

    // 测试遍历
    bool found_string = false;
    for (auto it = d.begin(); it != d.end(); ++it) {
        if (it->key == "string_value") {
            found_string = true;
            ASSERT_EQ(it->value.as_string(), "test string") << "通过迭代器访问的值不匹配";
        }
    }
    ASSERT_TRUE(found_string) << "迭代器未找到目标键";

    // 测试索引访问
    int index = d.find_index("bool_value");
    ASSERT_GE(index, 0) << "find_index 未找到目标键";
    ASSERT_TRUE(d.at_index(index).value.as_bool()) << "通过索引访问的值不匹配";
}

/**
 * @brief 测试 dict 转换
 */
TEST_F(VariantContainersTest, MutableDictConversion) {
    dict m_dict;
    m_dict["key1"] = 1;
    m_dict["key2"] = "value2";

    variant v(m_dict);
    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";

    // 转换回 dict
    dict result = v.as<dict>();
    ASSERT_EQ(result.size(), m_dict.size()) << "dict 大小不匹配";
    ASSERT_EQ(result["key1"].as<int>(), 1) << "值不匹配";
    ASSERT_EQ(result["key2"].as_string(), "value2") << "值不匹配";

    // 修改 dict 并测试
    result["key3"] = true;
    ASSERT_EQ(result.size(), 3) << "添加键后大小不正确";
    ASSERT_TRUE(result["key3"].as_bool()) << "新添加的值不匹配";
}

/**
 * @brief 测试 variants 类型的 variant 创建
 */
TEST_F(VariantContainersTest, ArrayConstruction) {
    variant v(sample_array);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_FALSE(v.is_null()) << "variant 不应该是 null";
    ASSERT_FALSE(v.is_object()) << "variant 不应该是对象类型";
}

/**
 * @brief 测试 variants 类型的 variant 转换
 */
TEST_F(VariantContainersTest, ArrayConversion) {
    variant  v(sample_array);
    variants result = v.as<variants>();

    ASSERT_EQ(result.size(), sample_array.size()) << "数组大小不匹配";

    // 验证数组元素是否正确
    ASSERT_EQ(result[0].as<int>(), 1) << "第一个元素不匹配";
    ASSERT_DOUBLE_EQ(result[1].as<double>(), 2.5) << "第二个元素不匹配";
    ASSERT_EQ(result[2].as_string(), "array item") << "第三个元素不匹配";
    ASSERT_EQ(result[3].as_bool(), false) << "第四个元素不匹配";
}

/**
 * @brief 测试数组访问操作
 */
TEST_F(VariantContainersTest, ArrayAccess) {
    variant v(sample_array);

    // 测试 size 方法
    ASSERT_EQ(v.size(), sample_array.size()) << "通过 size() 方法获取的大小不匹配";

    // 测试索引访问
    ASSERT_EQ(v[0].as<int>(), 1) << "通过索引访问的第一个元素不匹配";
    ASSERT_DOUBLE_EQ(v[1].as<double>(), 2.5) << "通过索引访问的第二个元素不匹配";
    ASSERT_EQ(v[2].as_string(), "array item") << "通过索引访问的第三个元素不匹配";
    ASSERT_EQ(v[3].as_bool(), false) << "通过索引访问的第四个元素不匹配";

    // 测试 get_array 方法
    const variants& arr = v.get_array();
    ASSERT_EQ(arr.size(), sample_array.size()) << "通过 get_array() 获取的数组大小不匹配";
    ASSERT_EQ(arr[2].as_string(), "array item") << "通过 get_array() 访问的元素不匹配";
}

TEST_F(VariantContainersTest, VariantsCapacityManagement) {
    variants values;
    EXPECT_TRUE(values.empty());

    values.reserve(4);
    values.assign(2, variant(1));
    EXPECT_EQ(values.size(), 2);

    values.emplace_back(variant("tail"));
    EXPECT_EQ(values[2].as_string(), "tail");

    values.resize(5, variant(false));
    EXPECT_EQ(values.size(), 5);
    EXPECT_FALSE(values[3].as_bool());

    values.shrink_to_fit();
    values.clear();
    EXPECT_TRUE(values.empty());
}

TEST_F(VariantContainersTest, VariantsInsertEraseAndIterators) {
    variants values{variant(1), variant("x"), variant(3)};
    values.insert(1, variant("y"));
    EXPECT_EQ(values[1].as_string(), "y");

    values.insert(2, static_cast<size_t>(2), variant(9));
    EXPECT_EQ(values[2].as_int32(), 9);
    EXPECT_EQ(values[3].as_int32(), 9);

    variants extra{variant("p"), variant("q")};
    values.insert(values.begin(), extra.begin(), extra.end());
    EXPECT_EQ(values[0].as_string(), "p");

    values.erase(0);
    EXPECT_EQ(values[0].as_string(), "q");

    auto first = values.begin();
    auto last  = values.begin() + 2;
    values.erase(first, last);
    EXPECT_EQ(values[0].as_string(), "y");

    auto it  = values.begin();
    auto ref = *it;
    EXPECT_TRUE(ref.get().is_string());
    EXPECT_EQ(ref.get().as_string(), "y");
    EXPECT_GT(values.end() - values.begin(), 0);
}

TEST_F(VariantContainersTest, VariantsComparisonAndHash) {
    variants lhs{variant(1), variant(2)};
    variants rhs{variant(1), variant(3)};

    EXPECT_TRUE(lhs < rhs);
    EXPECT_TRUE(rhs > lhs);

    auto hash_value = mc::calculate_array_hash(rhs);
    EXPECT_GT(hash_value, 0U);

    variants copy = lhs.deep_copy(nullptr);
    EXPECT_TRUE(copy == lhs);
    EXPECT_NE(copy.data(), lhs.data());

    lhs.swap(rhs);
    EXPECT_EQ(lhs[1].as_int32(), 3);

    auto* ptr = lhs.get_ptr(0);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->as_int32(), 1);
}

TEST_F(VariantContainersTest, VariantsAllocatorConstruction) {
    // 测试带 allocator 的构造函数
    std::allocator<char> alloc;
    variants             empty_with_alloc(alloc);
    EXPECT_TRUE(empty_with_alloc.empty());

    // 从 variant 构造
    variants source{variant(1), variant(2)};
    variant  array_variant(source);
    variants from_variant(array_variant);
    EXPECT_EQ(from_variant.size(), 2);
    EXPECT_EQ(from_variant[0].as_int32(), 1);

    // allocator 拷贝构造
    variants copy_with_alloc(from_variant, alloc);
    EXPECT_EQ(copy_with_alloc.size(), 2);

    // element_type 信息
    mc::array<int> typed_array{1, 2, 3};
    variants       strong_array(typed_array);
    EXPECT_FALSE(strong_array.empty());
    EXPECT_EQ(strong_array.element_type(), std::type_index(typeid(int)));
    EXPECT_FALSE(strong_array.element_type_name().empty());
}

TEST_F(VariantContainersTest, VariantsResizePopAndForEach) {
    variants values{variant(1), variant(2), variant(3)};

    values.resize(5);
    EXPECT_EQ(values.size(), 5);

    values.resize(7, variant(9));
    EXPECT_EQ(values.size(), 7);
    EXPECT_EQ(values[6].as_int32(), 9);

    values.assign(3, variant(4));
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[1].as_int32(), 4);

    values.assign({variant(5), variant(6)});
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].as_int32(), 5);

    values.reserve(8);
    EXPECT_GE(values.capacity(), 2U);
    EXPECT_GE(values.max_size(), values.size());

    values.shrink_to_fit();
    values.clear();
    EXPECT_TRUE(values.empty());

    variants sum_values{variant(1), variant(2), variant(3)};
    int      sum = 0;
    sum_values.for_each([&sum](const variant& item) {
        sum += item.as_int32();
    });
    EXPECT_EQ(sum, 6);

    sum_values.pop_back();
    EXPECT_EQ(sum_values.size(), 2);
}

TEST_F(VariantContainersTest, VariantsIteratorAdvancedOperations) {
    variants values{variant(10), variant(20), variant(30), variant(40)};

    auto ref = values.at_ref(1);
    ref      = variant(200);
    EXPECT_EQ(values[1].as_int32(), 200);

    auto front_ref = values.front();
    front_ref      = variant(5);
    EXPECT_EQ(values[0].as_int32(), 5);

    auto back_ref = values.back();
    back_ref      = variant(500);
    EXPECT_EQ(values[3].as_int32(), 500);

    const variants const_values = values;
    EXPECT_EQ(const_values.front().as_int32(), 5);
    EXPECT_EQ(const_values.back().as_int32(), 500);

    auto it      = values.begin();
    auto post_it = it++;
    EXPECT_EQ((*post_it).get().as_int32(), 5);
    EXPECT_EQ((*it).get().as_int32(), 200);

    auto pre_it = ++it;
    EXPECT_EQ((*pre_it).get().as_int32(), 30);

    auto diff = values.end() - values.begin();
    EXPECT_EQ(diff, static_cast<ptrdiff_t>(values.size()));

    auto backward = values.end();
    --backward;
    EXPECT_EQ((*backward).get().as_int32(), 500);

    backward -= 2;
    EXPECT_EQ((*backward).get().as_int32(), 200);

    auto forward = values.begin();
    forward += 3;
    EXPECT_EQ((*forward).get().as_int32(), 500);

    auto const_begin = const_values.cbegin();
    auto const_end   = const_values.cend();
    EXPECT_EQ(const_end - const_begin, static_cast<ptrdiff_t>(const_values.size()));

    auto const_post = const_begin++;
    EXPECT_EQ((*const_post).get().as_int32(), 5);
    EXPECT_EQ((*const_begin).get().as_int32(), 200);

    auto const_pre = ++const_begin;
    EXPECT_EQ((*const_pre).get().as_int32(), 30);

    auto const_back = const_values.cend();
    --const_back;
    EXPECT_EQ((*const_back).get().as_int32(), 500);

    auto rev_begin = const_values.crbegin();
    EXPECT_EQ((*rev_begin).get().as_int32(), 500);
    auto rev_post = rev_begin++;
    EXPECT_EQ((*rev_post).get().as_int32(), 500);
    EXPECT_EQ((*rev_begin).get().as_int32(), 30);

    auto rev_next = rev_begin + 1;
    EXPECT_EQ((*rev_next).get().as_int32(), 200);
    EXPECT_NE(rev_begin, const_values.crend());
}

/**
 * @brief 测试 blob 类型
 */
TEST_F(VariantContainersTest, BlobType) {
    // 创建二进制数据
    std::vector<char> binary_data{'H', 'e', 'l', 'l', 'o'};
    blob              b;
    b.data = binary_data;

    // 构造 variant
    variant v(b);
    ASSERT_TRUE(v.is_blob()) << "variant 应该是 blob 类型";

    // 获取 blob
    blob result = v.as_blob();
    ASSERT_EQ(result.data.size(), binary_data.size()) << "blob 大小不匹配";

    // 逐字节比较
    for (size_t i = 0; i < binary_data.size(); ++i) {
        ASSERT_EQ(result.data[i], binary_data[i]) << "字节不匹配，位置: " << i;
    }

    // 直接转换 std::vector<char>
    variant v2;
    to_variant(binary_data, v2);
    std::vector<char> result_vec = v2.as<std::vector<char>>();
    ASSERT_EQ(result_vec, binary_data) << "直接 vector<char> 转换失败";
}

/**
 * @brief 测试嵌套容器
 */
TEST_F(VariantContainersTest, NestedContainers) {
    // 创建嵌套字典
    dict nested_dict;
    nested_dict["nested_array"] = sample_array;
    nested_dict["nested_value"] = 123;

    // 创建嵌套数组
    variants nested_array;
    nested_array.push_back(sample_dict);
    nested_array.push_back(42);

    // 添加到主字典
    dict main_dict;
    main_dict["dict"]  = nested_dict;
    main_dict["array"] = nested_array;

    // 转换成 variant
    variant v(main_dict);

    // 测试嵌套访问
    dict result = v.as<dict>();
    ASSERT_EQ(result.size(), 2) << "主字典大小不匹配";

    // 访问嵌套字典
    dict nested_dict_result = result["dict"].as<dict>();
    ASSERT_EQ(nested_dict_result.size(), 2) << "嵌套字典大小不匹配";
    ASSERT_EQ(nested_dict_result["nested_value"].as<int>(), 123) << "嵌套字典中的值不匹配";

    // 访问嵌套数组
    variants nested_array_result = result["array"].as<variants>();
    ASSERT_EQ(nested_array_result.size(), 2) << "嵌套数组大小不匹配";
    ASSERT_EQ(nested_array_result[1].as<int>(), 42) << "嵌套数组中的值不匹配";

    // 访问嵌套字典中的嵌套数组
    variants nested_array_in_dict = nested_dict_result["nested_array"].as<variants>();
    ASSERT_EQ(nested_array_in_dict.size(), 4) << "嵌套字典中的嵌套数组大小不匹配";
    ASSERT_EQ(nested_array_in_dict[2].as_string(), "array item") << "深度嵌套的值不匹配";

    // 访问嵌套数组中的嵌套字典
    dict nested_dict_in_array = nested_array_result[0].as<dict>();
    ASSERT_TRUE(nested_dict_in_array.contains("int_value")) << "嵌套数组中的嵌套字典缺少键";
    ASSERT_EQ(nested_dict_in_array["int_value"].as<int>(), 42) << "嵌套数组中的嵌套字典值不匹配";
}

/**
 * @brief 测试空和满容量限制的容器
 */
TEST_F(VariantContainersTest, EmptyAndFullContainers) {
    // 测试空字典
    dict    empty_dict;
    variant v_empty_dict(empty_dict);
    ASSERT_TRUE(v_empty_dict.is_object()) << "空字典 variant 应该是对象类型";
    ASSERT_EQ(v_empty_dict.get_object().size(), 0) << "空字典的大小应该是 0";

    // 测试空数组
    variants empty_array;
    variant  v_empty_array(empty_array);
    ASSERT_TRUE(v_empty_array.is_array()) << "空数组 variant 应该是数组类型";
    ASSERT_EQ(v_empty_array.get_array().size(), 0) << "空数组的大小应该是 0";

    // 测试大型字典的限制 (使用 dict 更容易构建)
    constexpr int large_container_size = 256;
    dict          large_dict;
    for (int i = 0; i < large_container_size; ++i) {
        large_dict["key" + std::to_string(i)] = i;
    }
    variant v_large_dict(large_dict);
    ASSERT_EQ(v_large_dict.get_object().size(), large_container_size) << "大型字典大小不匹配";

    variants large_array;
    large_array.reserve(static_cast<size_t>(large_container_size));
    for (int i = 0; i < large_container_size; ++i) {
        large_array.push_back(i);
    }
    variant v_large_array(large_array);
    ASSERT_EQ(v_large_array.get_array().size(), large_container_size) << "大型数组大小不匹配";
}

// 测试 variants::from_variant 拒绝非数组类型
TEST_F(VariantContainersTest, VariantsFromVariantRejectNonArray) {
    // 尝试从非数组 variant 构造 variants，应该抛出异常
    variant v_int(42);
    EXPECT_THROW(variants arr(v_int), mc::invalid_arg_exception);
    
    variant v_string("test");
    EXPECT_THROW(variants arr2(v_string), mc::invalid_arg_exception);
    
    variant v_dict(sample_dict);
    EXPECT_THROW(variants arr3(v_dict), mc::invalid_arg_exception);
}

// 测试 variants 的延迟分配存储
TEST_F(VariantContainersTest, VariantsLazyAllocateStorage) {
    // 创建空的 variants
    variants arr;
    
    // 验证初始为空
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
    
    // 调用 emplace_back，应该触发 ensure_data()
    arr.emplace_back(42);
    EXPECT_FALSE(arr.empty());
    EXPECT_EQ(arr.size(), 1);
    EXPECT_EQ(arr[0], 42);
    
    // 调用 push_back
    arr.push_back(100);
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[1], 100);
    
    // 调用 for_each
    int sum = 0;
    arr.for_each([&sum](const variant& v) {
        sum += v.as_int32();
    });
    EXPECT_EQ(sum, 142);
}

// 测试 insert 空范围直接返回
TEST_F(VariantContainersTest, InsertEmptyRangeNoop) {
    variants arr{1, 2, 3};
    size_t original_size = arr.size();
    
    // 插入空范围（first == last），应该直接返回
    arr.insert(arr.begin(), arr.begin(), arr.begin());
    
    // 验证大小未改变
    EXPECT_EQ(arr.size(), original_size);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

} // namespace test
} // namespace mc