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

#include <gtest/gtest.h>
#include <mc/dbus/error.h>
#include <mc/dbus/signature.h>
#include <mc/dbus/type_code.h>

using namespace mc::dbus;

/**
 * 签名类单元测试
 */
class signature_test : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置代码
    }

    void TearDown() override {
        // 清理代码
    }
};

// /**
//  * 测试签名构造函数
//  */
// TEST_F(signature_test, constructor) {
//     // 默认构造函数创建空签名
//     signature empty_sig;
//     EXPECT_EQ("", empty_sig.str());
//     EXPECT_TRUE(empty_sig.is_empty());

//     // 使用字符串构造签名
//     signature sig1(type_code::string_type); // 字符串
//     EXPECT_EQ(std::string(1, type_to_char(type_code::string_type)), sig1.str());

//     // 复合类型
//     signature sig3("a{sv}"); // 字符串到变体的字典
//     EXPECT_EQ("a{sv}", sig3.str());
// }

/**
 * 测试无效签名
 */
TEST_F(signature_test, invalid_signature) {
    // 字典项键类型必须是基本类型
    EXPECT_THROW(signature("{(i)s}").validate(), invalid_signature_exception); // 结构体作为键

    // 无效的字符
    EXPECT_THROW(signature("z").validate(), invalid_signature_exception); // 无效类型字符

    // 超长签名
    std::string long_sig(max_signature_length + 1,
                         type_to_char(type_code::int32_type)); // 最大长度是255
    EXPECT_THROW(signature{long_sig}.validate(), invalid_signature_exception);
}

// /**
//  * 测试签名操作符
//  */
// TEST_F(signature_test, operators) {
//     signature sig1(type_code::string_type);
//     signature sig2(type_code::int32_type);

//     // 追加运算符
//     sig1 += sig2;
//     EXPECT_EQ(signature{type_code::string_type} + type_code::int32_type, sig1);

//     // 追加字符
//     sig1 += type_code::uint32_type;
//     EXPECT_EQ(signature(type_code::string_type) + type_code::int32_type + type_code::uint32_type,
//               sig1);

//     // 连接运算符
//     signature sig3 = sig1 + signature("a{sv}");
//     EXPECT_EQ(signature(type_code::string_type) + type_code::int32_type + type_code::uint32_type
//     +
//                   "a{sv}",
//               sig3);

//     // 比较运算符
//     EXPECT_TRUE(sig1 == signature("siu"));
//     EXPECT_FALSE(sig1 == sig3);
//     EXPECT_TRUE(sig1 != sig3);
//     EXPECT_FALSE(sig1 != signature("siu"));
// }

// /**
//  * 测试签名工具方法
//  */
// TEST_F(signature_test, utility_methods) {
//     // 验证基本类型检查
//     EXPECT_TRUE(signature::is_basic_type(type_code::string_type));
//     EXPECT_TRUE(signature::is_basic_type(type_code::int32_type));
//     EXPECT_TRUE(signature::is_basic_type(type_code::boolean_type));
//     EXPECT_FALSE(signature::is_basic_type(type_code::array_start));
//     EXPECT_FALSE(signature::is_basic_type(type_code::struct_start));

//     // 验证容器类型检查
//     EXPECT_TRUE(signature::is_container_type(type_code::array_start));
//     EXPECT_TRUE(signature::is_container_type(type_code::struct_start));
//     EXPECT_TRUE(signature::is_container_type(type_code::dict_entry_start));
//     EXPECT_TRUE(signature::is_container_type(type_code::variant_type));
//     EXPECT_FALSE(signature::is_container_type(type_code::string_type));
//     EXPECT_FALSE(signature::is_container_type(type_code::int32_type));

//     // 验证完整类型检查
//     EXPECT_TRUE(signature::is_complete_type(type_code::string_type));
//     EXPECT_TRUE(signature::is_complete_type(type_code::array_start));
//     EXPECT_TRUE(signature::is_complete_type(type_code::struct_start));
//     EXPECT_FALSE(signature::is_complete_type('z')); // 无效字符

//     // 验证单一完整类型检查
//     EXPECT_TRUE(
//         signature::is_single_complete_type(std::string(1,
//         type_to_char(type_code::string_type))));
//     EXPECT_TRUE(signature::is_single_complete_type("a{sv}"));
//     EXPECT_FALSE(signature::is_single_complete_type("si"));

//     // 获取完整类型长度
//     EXPECT_EQ(1, signature::get_complete_type_length(
//                      std::string(1, type_to_char(type_code::string_type))));
//     EXPECT_EQ(5, signature::get_complete_type_length("a{sv}"));
//     EXPECT_EQ(1, signature::get_complete_type_length("si"));

//     // 获取所有完整类型
//     signature              sig("si");
//     std::vector<signature> types = sig.get_complete_types();
//     ASSERT_EQ(2, types.size());
//     EXPECT_EQ("s", types[0].str());
//     EXPECT_EQ("i", types[1].str());
// }

// /**
//  * 测试签名迭代器
//  */
// TEST_F(signature_test, iterator) {
//     signature          sig("si(ii)a{sv}");
//     signature_iterator iter(sig);

//     // 验证迭代器初始状态
//     EXPECT_TRUE(iter.is_valid());
//     EXPECT_FALSE(iter.at_end());
//     EXPECT_EQ(type_to_char(type_code::string_type), iter.current_type_char());
//     EXPECT_EQ(type_code::string_type, iter.current_type_code());
//     EXPECT_TRUE(iter.is_basic());
//     EXPECT_FALSE(iter.is_container());
//     EXPECT_EQ("s", iter.current_type());

//     // 迭代第一个元素
//     iter.next();
//     EXPECT_TRUE(iter.is_valid());
//     EXPECT_EQ(type_code::int32_type, iter.current_type_code());
//     EXPECT_TRUE(iter.is_basic());

//     // 迭代第二个元素
//     iter.next();
//     EXPECT_TRUE(iter.is_valid());
//     EXPECT_EQ(type_code::struct_start, iter.current_type_code());
//     EXPECT_FALSE(iter.is_basic());
//     EXPECT_TRUE(iter.is_container());
//     EXPECT_EQ("(ii)", iter.current_type());

//     // 迭代第三个元素
//     iter.next();
//     EXPECT_TRUE(iter.is_valid());
//     EXPECT_EQ(type_code::array_start, iter.current_type_code());
//     EXPECT_FALSE(iter.is_basic());
//     EXPECT_TRUE(iter.is_container());
//     EXPECT_EQ("a{sv}", iter.current_type());

//     // 获取数组内容的迭代器
//     signature_iterator content_iter = iter.get_content_iterator();
//     EXPECT_TRUE(content_iter.is_valid());
//     EXPECT_EQ(type_code::dict_entry_start, content_iter.current_type_code());

//     // 获取字典键的迭代器
//     signature_iterator key_iter = content_iter.get_dict_key_iterator();
//     EXPECT_TRUE(key_iter.is_valid());
//     EXPECT_EQ(type_code::string_type, key_iter.current_type_code());

//     // 获取字典值的迭代器
//     signature_iterator value_iter = content_iter.get_dict_value_iterator();
//     EXPECT_TRUE(value_iter.is_valid());
//     EXPECT_EQ(type_code::variant_type, value_iter.current_type_code());

//     // 迭代到结束
//     iter.next();
//     EXPECT_TRUE(iter.at_end());
//     EXPECT_FALSE(iter.is_valid());
// }