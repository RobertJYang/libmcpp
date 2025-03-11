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
#include <gtest/gtest.h>
#include <mc/variant.h>
#include <mc/dict.h>
#include "test_variant_helpers.h"
#include <stdexcept>

namespace mc {
namespace test {

class VariantContainersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
        sample_dict = create_sample_dict();
        sample_array = create_sample_array();
    }

    void TearDown() override {
        // 在每个测试后执行
    }

    dict create_sample_dict() {
        mutable_dict m_dict;
        m_dict["int_value"] = 42;
        m_dict["double_value"] = 3.14;
        m_dict["string_value"] = "test string";
        m_dict["bool_value"] = true;
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

    dict sample_dict;
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
    dict result = v.as<dict>();
    
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
    variant v(sample_dict);
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
    ASSERT_TRUE(d.at(index).value.as_bool()) << "通过索引访问的值不匹配";
}

/**
 * @brief 测试 mutable_dict 转换
 */
TEST_F(VariantContainersTest, MutableDictConversion) {
    mutable_dict m_dict;
    m_dict["key1"] = 1;
    m_dict["key2"] = "value2";
    
    variant v(m_dict);
    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    
    // 转换回 mutable_dict
    mutable_dict result = v.as<mutable_dict>();
    ASSERT_EQ(result.size(), m_dict.size()) << "mutable_dict 大小不匹配";
    ASSERT_EQ(result["key1"].as<int>(), 1) << "值不匹配";
    ASSERT_EQ(result["key2"].as_string(), "value2") << "值不匹配";
    
    // 修改 mutable_dict 并测试
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
    variant v(sample_array);
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

/**
 * @brief 测试 blob 类型
 */
TEST_F(VariantContainersTest, BlobType) {
    // 创建二进制数据
    std::vector<char> binary_data{'H', 'e', 'l', 'l', 'o'};
    blob b;
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
    variant v2; to_variant(binary_data, v2);
    std::vector<char> result_vec = v2.as<std::vector<char>>();
    ASSERT_EQ(result_vec, binary_data) << "直接 vector<char> 转换失败";
}

/**
 * @brief 测试嵌套容器
 */
TEST_F(VariantContainersTest, NestedContainers) {
    // 创建嵌套字典
    mutable_dict nested_dict;
    nested_dict["nested_array"] = sample_array;
    nested_dict["nested_value"] = 123;
    
    // 创建嵌套数组
    variants nested_array;
    nested_array.push_back(sample_dict);
    nested_array.push_back(42);
    
    // 添加到主字典
    mutable_dict main_dict;
    main_dict["dict"] = nested_dict;
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
    dict empty_dict;
    variant v_empty_dict(empty_dict);
    ASSERT_TRUE(v_empty_dict.is_object()) << "空字典 variant 应该是对象类型";
    ASSERT_EQ(v_empty_dict.get_object().size(), 0) << "空字典的大小应该是 0";
    
    // 测试空数组
    variants empty_array;
    variant v_empty_array(empty_array);
    ASSERT_TRUE(v_empty_array.is_array()) << "空数组 variant 应该是数组类型";
    ASSERT_EQ(v_empty_array.get_array().size(), 0) << "空数组的大小应该是 0";
    
    // 测试大型字典的限制 (使用 mutable_dict 更容易构建)
    mutable_dict large_dict;
    for (int i = 0; i < 1000; ++i) {
        large_dict["key" + std::to_string(i)] = i;
    }
    variant v_large_dict(large_dict);
    ASSERT_EQ(v_large_dict.get_object().size(), 1000) << "大型字典大小不匹配";
    
    // 测试大型数组的限制
    variants large_array;
    for (int i = 0; i < 1000; ++i) {
        large_array.push_back(i);
    }
    variant v_large_array(large_array);
    ASSERT_EQ(v_large_array.get_array().size(), 1000) << "大型数组大小不匹配";
}

} // namespace test
} // namespace mc 