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

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <unordered_set>

#include <mc/quark.h>
#include <mc/string_view.h>

using namespace mc;

TEST(quark_compat, equals_const_char_ptr)
{
    auto q = "foo.compat.cstr"_q;
    EXPECT_TRUE(q == "foo.compat.cstr");
    EXPECT_FALSE(q != "foo.compat.cstr");
    EXPECT_TRUE("foo.compat.cstr" == q);
    EXPECT_FALSE("foo.compat.cstr" != q);

    EXPECT_TRUE(q != "foo.compat.cstr.other");
}

TEST(quark_compat, equals_std_string)
{
    auto              q = "foo.compat.std_string"_q;
    const std::string s = "foo.compat.std_string";
    EXPECT_TRUE(q == s);
    EXPECT_FALSE(q != s);
}

TEST(quark_compat, equals_std_string_view)
{
    auto                   q  = "foo.compat.std_string_view"_q;
    const std::string_view sv = "foo.compat.std_string_view";
    EXPECT_TRUE(q == sv);
    EXPECT_FALSE(q != sv);
}

TEST(quark_compat, equals_mc_string_view)
{
    auto                  q  = "foo.compat.mc_string_view"_q;
    const mc::string_view sv = "foo.compat.mc_string_view";
    EXPECT_TRUE(q == sv);
    EXPECT_TRUE(sv == q);
}

TEST(quark_compat, implicit_conversion_to_views_and_string)
{
    auto q = "foo.compat.implicit"_q;

    mc::string_view  mv = q;
    std::string_view sv = q;
    std::string      ss = q;

    EXPECT_EQ(std::string(mv.data(), mv.size()), "foo.compat.implicit");
    EXPECT_EQ(std::string(sv.data(), sv.size()), "foo.compat.implicit");
    EXPECT_EQ(ss, "foo.compat.implicit");
}

TEST(quark_compat, can_be_used_as_unordered_set_key)
{
    std::unordered_set<mc::quark> set;
    auto                          a = "foo.compat.set.a"_q;
    auto                          b = "foo.compat.set.b"_q;
    set.insert(a);
    set.insert(b);
    set.insert("foo.compat.set.a"_q);

    EXPECT_EQ(set.size(), 2U);
    EXPECT_EQ(set.count(a), 1U);
    EXPECT_EQ(set.count(b), 1U);
}

TEST(quark_compat, try_from_finds_existing_literal)
{
    (void)"foo.compat.tryfrom.exists"_q;
    auto q = mc::quark::try_from(mc::string_view("foo.compat.tryfrom.exists"));
    EXPECT_TRUE(q.valid());
    EXPECT_EQ(q.view(), mc::string_view("foo.compat.tryfrom.exists"));
}

TEST(quark_compat, try_from_returns_invalid_for_unknown_input)
{
    auto q = mc::quark::try_from(mc::string_view("foo.compat.tryfrom.never_registered_xyz"));
    EXPECT_FALSE(q.valid());
    EXPECT_EQ(q.id(), mc::quark::invalid_id);
}
