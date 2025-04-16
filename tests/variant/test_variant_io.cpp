/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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
 * @file test_variant_io.cpp
 * @brief 测试variant的流输出功能
 */
#include <gtest/gtest.h>
#include <mc/variant.h>
#include <mc/dict.h>
#include <sstream>

using namespace mc;

// 测试基本数据类型的variant流输出
TEST(VariantIOTest, BasicTypes) {
    std::stringstream ss;
    
    // 测试null类型
    variant null_var;
    ss << null_var;
    EXPECT_EQ(ss.str(), "null");
    ss.str("");
    
    // 测试布尔类型
    variant bool_true(true);
    ss << bool_true;
    EXPECT_EQ(ss.str(), "true");
    ss.str("");
    
    variant bool_false(false);
    ss << bool_false;
    EXPECT_EQ(ss.str(), "false");
    ss.str("");
    
    // 测试整数类型
    variant int8_var(int8_t(42));
    ss << int8_var;
    EXPECT_EQ(ss.str(), "42");
    ss.str("");
    
    variant uint8_var(uint8_t(200));
    ss << uint8_var;
    EXPECT_EQ(ss.str(), "200");
    ss.str("");
    
    variant int16_var(int16_t(-300));
    ss << int16_var;
    EXPECT_EQ(ss.str(), "-300");
    ss.str("");
    
    variant uint16_var(uint16_t(40000));
    ss << uint16_var;
    EXPECT_EQ(ss.str(), "40000");
    ss.str("");
    
    variant int32_var(int32_t(-70000));
    ss << int32_var;
    EXPECT_EQ(ss.str(), "-70000");
    ss.str("");
    
    variant uint32_var(uint32_t(3000000000));
    ss << uint32_var;
    EXPECT_EQ(ss.str(), "3000000000");
    ss.str("");
    
    variant int64_var(int64_t(-9000000000));
    ss << int64_var;
    EXPECT_EQ(ss.str(), "-9000000000");
    ss.str("");
    
    variant uint64_var(uint64_t(9000000000));
    ss << uint64_var;
    EXPECT_EQ(ss.str(), "9000000000");
    ss.str("");
    
    // 测试浮点类型
    variant double_var(3.14159);
    ss << double_var;
    // 浮点数输出可能因平台不同而略有差异，所以使用包含关系检查
    EXPECT_TRUE(ss.str().find("3.14159") != std::string::npos);
    ss.str("");
    
    // 测试字符串类型
    variant string_var("测试字符串");
    ss << string_var;
    EXPECT_EQ(ss.str(), "测试字符串");
    ss.str("");
}

// 测试复杂数据类型的variant流输出
TEST(VariantIOTest, ComplexTypes) {
    std::stringstream ss;
    
    // 测试数组类型
    variants array_data;
    array_data.push_back(1);
    array_data.push_back("字符串");
    array_data.push_back(true);
    variant array_var(array_data);
    ss << array_var;
    // 转换为字符串后应包含所有元素
    std::string array_str = ss.str();
    EXPECT_TRUE(array_str.find("1") != std::string::npos);
    EXPECT_TRUE(array_str.find("字符串") != std::string::npos);
    EXPECT_TRUE(array_str.find("true") != std::string::npos);
    ss.str("");
    
    // 测试字典类型
    dict dict_data{{"姓名", "张三"}, {"年龄", 30}, {"是否学生", false}};
    variant dict_var(dict_data);
    ss << dict_var;
    // 转换为字符串后应包含所有键值对
    std::string dict_str = ss.str();
    EXPECT_TRUE(dict_str.find("姓名") != std::string::npos);
    EXPECT_TRUE(dict_str.find("张三") != std::string::npos);
    EXPECT_TRUE(dict_str.find("年龄") != std::string::npos);
    EXPECT_TRUE(dict_str.find("30") != std::string::npos);
    EXPECT_TRUE(dict_str.find("是否学生") != std::string::npos);
    EXPECT_TRUE(dict_str.find("false") != std::string::npos);
    std::cout << "dict_str: " << dict_str << std::endl;
    std::cout << "dict_var: " << dict_var << std::endl;
    std::cout << "type: " << dict_var.get_type()  << std::endl;
    std::cout << "ss: " << ss.str() << std::endl;
    ss.str("");
    
    // 测试二进制数据类型
    std::vector<uint8_t> binary_data{0x01, 0x02, 0x03, 0xFF};
    blob blob_data;
    auto* char_data = reinterpret_cast<const char*>(binary_data.data());
    blob_data.data.assign(char_data, char_data + binary_data.size());
    variant blob_var(blob_data);
    ss << blob_var;
    std::cout << "blob_str: " << blob_data << std::endl;
    std::cout << "blob_str ss: " << ss.str() << std::endl;
    EXPECT_EQ(ss.str(), "blob[4]");
    ss.str("");
}

// 测试嵌套数据结构的variant流输出
TEST(VariantIOTest, NestedTypes) {
    std::stringstream ss;
    
    // 创建嵌套的字典结构
    dict nested_dict{
        {"个人信息", dict{
            {"姓名", "张三"},
            {"年龄", 30},
            {"联系方式", dict{
                {"电话", "12345678901"},
                {"邮箱", "zhangsan@example.com"}
            }}
        }},
        {"爱好", variants{
            "阅读",
            "篮球",
            "编程"
        }},
        {"工作经历", variants{
            dict{{"公司", "A公司"}, {"年份", 2018}},
            dict{{"公司", "B公司"}, {"年份", 2020}}
        }}
    };
    
    variant nested_var(nested_dict);
    ss << nested_var;
    
    // 检查输出字符串中包含关键信息
    std::string nested_str = ss.str();
    EXPECT_TRUE(nested_str.find("个人信息") != std::string::npos);
    EXPECT_TRUE(nested_str.find("张三") != std::string::npos);
    EXPECT_TRUE(nested_str.find("联系方式") != std::string::npos);
    EXPECT_TRUE(nested_str.find("12345678901") != std::string::npos);
    EXPECT_TRUE(nested_str.find("爱好") != std::string::npos);
    EXPECT_TRUE(nested_str.find("阅读") != std::string::npos);
    EXPECT_TRUE(nested_str.find("工作经历") != std::string::npos);
    EXPECT_TRUE(nested_str.find("A公司") != std::string::npos);
    EXPECT_TRUE(nested_str.find("2020") != std::string::npos);
}

// 测试在其他流操作中使用variant
TEST(VariantIOTest, StreamOperations) {
    std::stringstream ss;
    
    // 在复杂的流操作中测试variant输出
    variant int_var(42);
    variant string_var("测试");
    variant bool_var(true);
    
    ss << "整数: " << int_var << ", 字符串: " << string_var << ", 布尔值: " << bool_var;
    EXPECT_EQ(ss.str(), "整数: 42, 字符串: 测试, 布尔值: true");
}

// 测试type_id枚举类型的流输出
TEST(VariantIOTest, TypeIDOutput) {
    std::stringstream ss;
    
    // 测试各种type_id枚举值的输出
    ss << type_id::null_type;
    EXPECT_EQ(ss.str(), "0");  // null_type的值为0
    ss.str("");
    
    ss << type_id::bool_type;
    EXPECT_EQ(ss.str(), "10");  // bool_type的实际值为10
    ss.str("");
    
    ss << type_id::int8_type;
    EXPECT_EQ(ss.str(), "1");  // int8_type的实际值为1
    ss.str("");
    
    ss << type_id::int16_type;
    EXPECT_EQ(ss.str(), "3");  // int16_type的值为3
    ss.str("");
    
    ss << type_id::int32_type;
    EXPECT_EQ(ss.str(), "5");  // int32_type的实际值为5
    ss.str("");
    
    ss << type_id::int64_type;
    EXPECT_EQ(ss.str(), "7");  // int64_type的实际值为7
    ss.str("");
    
    ss << type_id::uint8_type;
    EXPECT_EQ(ss.str(), "2");  // uint8_type的实际值为2
    ss.str("");
    
    ss << type_id::uint16_type;
    EXPECT_EQ(ss.str(), "4");  // uint16_type的实际值为4
    ss.str("");
    
    ss << type_id::uint32_type;
    EXPECT_EQ(ss.str(), "6");  // uint32_type的实际值为6
    ss.str("");
    
    ss << type_id::uint64_type;
    EXPECT_EQ(ss.str(), "8");  // uint64_type的实际值为8
    ss.str("");
    
    ss << type_id::double_type;
    EXPECT_EQ(ss.str(), "9");  // double_type的实际值为9
    ss.str("");
    
    ss << type_id::string_type;
    EXPECT_EQ(ss.str(), "11");  // string_type的值为11
    ss.str("");
    
    ss << type_id::array_type;
    EXPECT_EQ(ss.str(), "12");  // array_type的值为12
    ss.str("");
    
    ss << type_id::object_type;
    EXPECT_EQ(ss.str(), "13");  // object_type的值为13
    ss.str("");
    
    ss << type_id::blob_type;
    EXPECT_EQ(ss.str(), "14");  // blob_type的值为14
    ss.str("");
    
    // 测试在正常语句中使用type_id
    variant var_int(42);
    variant var_str("测试");
    
    ss << "整数类型: " << var_int.get_type() << ", 字符串类型: " << var_str.get_type();
    EXPECT_EQ(ss.str(), "整数类型: 5, 字符串类型: 11");  // int32_type=5, string_type=11
    ss.str("");
    
    // 测试组合使用type_id和variant
    ss << "类型: " << var_int.get_type() << ", 值: " << var_int;
    EXPECT_EQ(ss.str(), "类型: 5, 值: 42");  // int32_type=5
    ss.str("");
    
    ss << "类型: " << var_str.get_type() << ", 值: " << var_str;
    EXPECT_EQ(ss.str(), "类型: 11, 值: 测试");  // string_type=11
}

// 测试直接使用blob类型的流输出运算符
TEST(VariantIOTest, BlobDirectOutput) {
    std::stringstream ss;
    
    // 创建不同大小的blob对象进行测试
    // 测试空blob
    blob_base<> empty_blob;
    ss << empty_blob;
    EXPECT_EQ(ss.str(), "blob[0]");
    ss.str("");
    
    // 测试带有数据的blob
    std::vector<uint8_t> binary_data{0x01, 0x02, 0x03, 0xFF};
    blob_base<> data_blob;
    auto* char_data = reinterpret_cast<const char*>(binary_data.data());
    data_blob.data.assign(char_data, char_data + binary_data.size());
    ss << data_blob;
    EXPECT_EQ(ss.str(), "blob[4]");
    ss.str("");
    
    // 测试在复杂的流操作中使用blob
    ss << "二进制数据: " << data_blob << ", 大小: " << data_blob.data.size() << " 字节";
    EXPECT_EQ(ss.str(), "二进制数据: blob[4], 大小: 4 字节");
} 