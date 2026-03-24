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

#include <mc/exception.h>
#include <mc/variant.h>
#include <sstream>

namespace {

// 测试 variant_base::copy 以及 to_variant/from_variant 转换函数
class VariantConversionTest : public ::testing::Test {};

TEST_F(VariantConversionTest, CopyOnVariousTypes)
{
    // 数值
    mc::variant v_num = 123;
    auto        c1    = v_num.copy();
    EXPECT_TRUE(c1 == v_num);

    // 字符串
    mc::variant v_str = mc::string("hello");
    auto        c2    = v_str.copy();
    EXPECT_TRUE(c2 == v_str);

    // 数组
    mc::variant v_arr = mc::variants{1, 2, 3};
    auto        c3    = v_arr.copy();
    EXPECT_TRUE(c3 == v_arr);

    // 对象
    mc::variant v_obj = mc::dict{{"k1", 1}, {"k2", 2}};
    auto        c4    = v_obj.copy();
    EXPECT_TRUE(c4 == v_obj);
}

TEST_F(VariantConversionTest, ToVariantOverloads)
{
    // bool -> variant
    mc::variant v1;
    mc::to_variant(true, v1);
    EXPECT_TRUE(v1.is_bool());
    EXPECT_TRUE(v1 == true);

    // const char* -> variant
    mc::variant v2;
    const char* s1 = "abc";
    mc::to_variant(s1, v2);
    EXPECT_TRUE(v2.is_string());
    EXPECT_TRUE(v2 == mc::string("abc"));

    // char* -> variant（可变 C 字符串）
    char        buf[] = {'x', 'y', 'z', '\0'};
    mc::variant v3;
    mc::to_variant(buf, v3);
    EXPECT_TRUE(v3.is_string());
    EXPECT_TRUE(v3 == mc::string("xyz"));

    // variants -> variant
    mc::variants arr{1, 2, 3};
    mc::variant  v4;
    mc::to_variant(arr, v4);
    EXPECT_TRUE(v4.is_array());
    EXPECT_TRUE(v4 == arr);

    // const char* 为 nullptr -> 空字符串
    mc::variant v5;
    const char* pnull = nullptr;
    mc::to_variant(pnull, v5);
    EXPECT_TRUE(v5.is_string());
    EXPECT_EQ(v5.as_string(), mc::string(""));
}

TEST_F(VariantConversionTest, FromVariantOverloads)
{
    // string -> const char* / char*
    mc::variant vs  = mc::string("hello");
    const char* pcs = nullptr;
    mc::from_variant(vs, pcs);
    ASSERT_NE(pcs, nullptr);
    EXPECT_STREQ(pcs, "hello");

    char* pms = nullptr;
    mc::from_variant(vs, pms);
    ASSERT_NE(pms, nullptr);
    EXPECT_STREQ(pms, "hello");

    // bool -> bool
    mc::variant vb = true;
    bool        outb{};
    mc::from_variant(vb, outb);
    EXPECT_TRUE(outb);

    // array -> variants
    mc::variant  va = mc::variants{1, 2};
    mc::variants arr;
    mc::from_variant(va, arr);
    EXPECT_EQ(arr.size(), 2u);

    // dict <- variant
    mc::variant vobj = mc::dict{{"a", 1}, {"b", 2}};
    mc::dict    out;
    mc::from_variant(vobj, out);
    EXPECT_EQ(out.size(), 2u);
}

TEST_F(VariantConversionTest, FromNumericVariantToStdString)
{
    mc::variant int_variant = 42;
    std::string int_string;
    mc::from_variant(int_variant, int_string);
    EXPECT_EQ(int_string, "42");

    mc::variant double_variant = 60.8;
    std::string double_string;
    mc::from_variant(double_variant, double_string);
    EXPECT_EQ(double_string, "60.8");
}

TEST_F(VariantConversionTest, VariantHeaderExposesCompleteDictStdFindBridge)
{
    mc::dict d{{"alpha", 1}, {"beta", 2}};

    std::string key = "beta";
    auto        it  = d.find(key);

    ASSERT_NE(it, d.end());
    EXPECT_EQ(it->value.as<int>(), 2);
}

TEST_F(VariantConversionTest, StreamOutputFormatting)
{
    // 覆盖 operator<< 的多个分支
    std::ostringstream os;

    mc::variant vnull;
    os << vnull; // null

    mc::variant vbool = true;
    os << vbool; // bool

    mc::variant vint = 42;
    os << vint; // int

    mc::variant vuint = static_cast<uint64_t>(7);
    os << vuint; // uint

    mc::variant vdouble = 3.14;
    os << vdouble; // double

    mc::variant vstr = mc::string("abc");
    os << vstr; // string

    mc::variant varr = mc::variants{1, 2, 3};
    os << varr; // array

    mc::variant vobj = mc::dict{{"k", 1}};
    os << vobj; // object

    // blob 输出 blob[size]
    mc::blob           b{"xy", 2};
    mc::variant        vblob = b;
    std::ostringstream os_blob;
    os_blob << vblob;
    mc::string blob_out = os_blob.str();
    EXPECT_NE(blob_out.find("blob[2]"), mc::string::npos);
}

TEST_F(VariantConversionTest, CopyBlobShouldBeEqual)
{
    const char  data[] = {'A', 'B'};
    mc::blob    b{data, sizeof(data)};
    mc::variant vb = b;
    auto        c  = vb.copy();
    EXPECT_TRUE(c == vb);
}

// 测试 std::vector 转换时元素类型不匹配抛异常
TEST_F(VariantConversionTest, StdVectorConversionTypeMismatch)
{
    // 输入字符串数组
    mc::variants str_array;
    str_array.push_back("hello");
    str_array.push_back("world");
    str_array.push_back("test");
    mc::variant v(str_array);

    // 尝试转换为 std::vector<int>，应该抛出异常
    std::vector<int> int_vec;
    EXPECT_THROW(mc::from_variant(v, int_vec), mc::exception);
}

// 测试 from_variant 从字符串转换为ay数组
TEST_F(VariantConversionTest, VectorByteFromString)
{
    const char*         raw_str1 = "abcd";
    mc::variant         var1     = mc::string(raw_str1, 4);
    std::vector<int8_t> arr1;
    mc::from_variant(var1, arr1);
    EXPECT_EQ(arr1.size(), 4);
    EXPECT_EQ(arr1[0], 97);
    EXPECT_EQ(arr1[1], 98);
    EXPECT_EQ(arr1[2], 99);
    EXPECT_EQ(arr1[3], 100);

    const char*          raw_str2 = "\x01\x02\x03\x00\x04";
    mc::variant          var3     = mc::string(raw_str2, 5);
    std::vector<uint8_t> arr_unsigned;
    mc::from_variant(var3, arr_unsigned);
    EXPECT_EQ(arr_unsigned.size(), 5);
    EXPECT_EQ(arr_unsigned[0], 1);
    EXPECT_EQ(arr_unsigned[1], 2);
    EXPECT_EQ(arr_unsigned[2], 3);
    EXPECT_EQ(arr_unsigned[3], 0);
    EXPECT_EQ(arr_unsigned[4], 4);
}

} // namespace
