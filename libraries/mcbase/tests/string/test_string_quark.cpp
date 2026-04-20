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

#include <mc/quark.h>
#include <mc/string.h>

using namespace mc;

TEST(string_quark, default_string_is_not_quark)
{
    mc::string s;
    EXPECT_FALSE(s.is_quark());
    EXPECT_TRUE(s.empty());

    auto q = s.to_quark();
    EXPECT_TRUE(q.valid());
    EXPECT_EQ(q.id(), mc::quark::empty_id);
}

TEST(string_quark, construct_from_quark_keeps_quark_mode)
{
    auto       q = "string.quark.basic"_q;
    mc::string s(q);
    EXPECT_TRUE(s.is_quark());
    EXPECT_EQ(s.size(), 18U);
    EXPECT_STREQ(s.c_str(), "string.quark.basic");
    EXPECT_EQ(s.to_quark().id(), q.id());
}

TEST(string_quark, from_quark_static_helper)
{
    auto       q = "string.quark.from_helper"_q;
    mc::string s = mc::string::from_quark(q);
    EXPECT_TRUE(s.is_quark());
    EXPECT_EQ(s, mc::string("string.quark.from_helper"));
}

TEST(string_quark, view_and_data_match)
{
    auto       q = "string.quark.view"_q;
    mc::string s(q);
    auto       v = s.view();
    EXPECT_EQ(std::string(v.data(), v.size()), "string.quark.view");
    EXPECT_STREQ(s.data(), "string.quark.view");
}

TEST(string_quark, equal_two_quark_backed_uses_id_fast_path)
{
    mc::string a("string.quark.fast.eq"_q);
    mc::string b("string.quark.fast.eq"_q);
    EXPECT_TRUE(a == b);

    mc::string c("string.quark.fast.eq2"_q);
    EXPECT_FALSE(a == c);
}

TEST(string_quark, equal_quark_vs_heap_falls_back_to_view)
{
    mc::string q_str("string.quark.mixed"_q);
    mc::string heap_str("string.quark.mixed");
    EXPECT_FALSE(heap_str.is_quark());
    EXPECT_TRUE(q_str == heap_str);
    EXPECT_TRUE(heap_str == q_str);
}

TEST(string_quark, hash_matches_view_hash)
{
    mc::string q_str("string.quark.hash"_q);
    mc::string heap_str("string.quark.hash");
    EXPECT_EQ(q_str.hash(), heap_str.hash());
}

TEST(string_quark, mutation_triggers_cow_to_heap)
{
    auto       q = "string.quark.cow"_q;
    mc::string s(q);
    EXPECT_TRUE(s.is_quark());

    s.append(".tail");
    EXPECT_FALSE(s.is_quark());
    EXPECT_EQ(s, mc::string("string.quark.cow.tail"));

    auto re = "string.quark.cow"_q;
    EXPECT_EQ(re.id(), q.id());
    EXPECT_STREQ(re.c_str(), "string.quark.cow");
}

TEST(string_quark, copy_share_quark_storage_then_cow_on_write)
{
    mc::string a("string.quark.share"_q);
    mc::string b = a;
    EXPECT_TRUE(a.is_quark());
    EXPECT_TRUE(b.is_quark());
    EXPECT_EQ(a.to_quark(), b.to_quark());

    b.push_back('x');
    EXPECT_TRUE(a.is_quark());
    EXPECT_FALSE(b.is_quark());
    EXPECT_EQ(a, mc::string("string.quark.share"));
    EXPECT_EQ(b, mc::string("string.quark.sharex"));
}

TEST(string_quark, to_quark_lookup_only_when_heap)
{
    mc::string heap_unseen("string.quark.lookup.never_seen.xyz");
    EXPECT_FALSE(heap_unseen.to_quark().valid());

    (void)"string.quark.lookup.seen"_q;
    mc::string heap_seen("string.quark.lookup.seen");
    EXPECT_TRUE(heap_seen.to_quark().valid());
}

TEST(string_quark, untrusted_input_to_quark_never_writes_table)
{
    const std::string user_input = "string.quark.untrusted.attacker_controlled.xyz";
    mc::string        s(user_input.c_str());
    EXPECT_FALSE(s.to_quark().valid());
    EXPECT_FALSE(s.to_quark().valid());
    EXPECT_FALSE(mc::quark::try_from(mc::string_view(user_input.c_str())).valid());
}

TEST(string_quark, clear_on_quark_string_drops_quark_mode)
{
    mc::string s("string.quark.clear"_q);
    EXPECT_TRUE(s.is_quark());
    s.clear();
    EXPECT_FALSE(s.is_quark());
    EXPECT_TRUE(s.empty());
}

TEST(string_quark, empty_quark_makes_empty_string)
{
    mc::string s(""_q);
    EXPECT_TRUE(s.is_quark());
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0U);
}
