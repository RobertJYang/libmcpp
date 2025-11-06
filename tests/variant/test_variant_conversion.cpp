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

#include <mc/variant.h>
#include <sstream>

namespace {

// 测试 variant_base::copy 以及 to_variant/from_variant 转换函数
class VariantConversionTest : public ::testing::Test {};

TEST_F(VariantConversionTest, CopyOnVariousTypes) {
    // 数值
    mc::variant v_num = 123;
    auto        c1    = v_num.copy();
    EXPECT_TRUE(c1 == v_num);

    // 字符串
    mc::variant v_str = std::string("hello");
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

TEST_F(VariantConversionTest, ToVariantOverloads) {
    // bool -> variant
    mc::variant v1;
    mc::to_variant(true, v1);
    EXPECT_TRUE(v1.is_bool());
    EXPECT_TRUE(v1 == true);

    // const char* -> variant
    mc::variant  v2;
    const char*  s1 = "abc";
    mc::to_variant(s1, v2);
    EXPECT_TRUE(v2.is_string());
    EXPECT_TRUE(v2 == std::string("abc"));

    // char* -> variant（可变 C 字符串）
    char buf[] = {'x', 'y', 'z', '\0'};
    mc::variant v3;
    mc::to_variant(buf, v3);
    EXPECT_TRUE(v3.is_string());
    EXPECT_TRUE(v3 == std::string("xyz"));

    // variants -> variant
    mc::variants arr{1, 2, 3};
    mc::variant  v4;
    mc::to_variant(arr, v4);
    EXPECT_TRUE(v4.is_array());
    EXPECT_TRUE(v4 == arr);

    // const char* 为 nullptr -> 空字符串
    mc::variant  v5;
    const char*  pnull = nullptr;
    mc::to_variant(pnull, v5);
    EXPECT_TRUE(v5.is_string());
    EXPECT_EQ(v5.as_string(), std::string(""));
}

TEST_F(VariantConversionTest, FromVariantOverloads) {
    // string -> const char* / char*
    mc::variant vs = std::string("hello");
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

TEST_F(VariantConversionTest, StreamOutputFormatting) {
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

    mc::variant vstr = std::string("abc");
    os << vstr; // string

    mc::variant varr = mc::variants{1, 2, 3};
    os << varr; // array

    mc::variant vobj = mc::dict{{"k", 1}};
    os << vobj; // object

    // blob 输出 blob[size]
    mc::blob     b{"xy", 2};
    mc::variant  vblob = b;
    std::ostringstream os_blob;
    os_blob << vblob;
    auto blob_out = os_blob.str();
    EXPECT_NE(blob_out.find("blob[2]"), std::string::npos);
}

TEST_F(VariantConversionTest, CopyBlobShouldBeEqual) {
    const char data[] = {'A', 'B'};
    mc::blob   b{data, sizeof(data)};
    mc::variant vb = b;
    auto        c   = vb.copy();
    EXPECT_TRUE(c == vb);
}

} // namespace


