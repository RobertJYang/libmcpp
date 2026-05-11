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

#include <sstream>
#include <string>
#include <unordered_set>

#include <mc/quark.h>
#include <mc/string_view.h>

using namespace mc;

TEST(quark_basic, default_constructed_is_invalid)
{
    mc::quark q;
    EXPECT_EQ(q.id(), mc::quark::invalid_id);
    EXPECT_FALSE(q.valid());
    EXPECT_FALSE(static_cast<bool>(q));
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0U);
    EXPECT_STREQ(q.c_str(), "");
}

TEST(quark_basic, empty_literal_maps_to_empty_id)
{
    auto q = ""_q;
    EXPECT_EQ(q.id(), mc::quark::empty_id);
    EXPECT_TRUE(q.valid());
    EXPECT_TRUE(static_cast<bool>(q));
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0U);
    EXPECT_STREQ(q.c_str(), "");
}

TEST(quark_basic, same_literal_returns_same_id)
{
    auto a = "foo.basic.dedup"_q;
    auto b = "foo.basic.dedup"_q;
    auto c = MC_QUARK("foo.basic.dedup");

    EXPECT_EQ(a.id(), b.id());
    EXPECT_EQ(a.id(), c.id());
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
}

TEST(quark_basic, different_literals_get_different_ids)
{
    auto a = "foo.basic.diff.a"_q;
    auto b = "foo.basic.diff.b"_q;
    EXPECT_NE(a.id(), b.id());
    EXPECT_NE(a, b);
}

TEST(quark_basic, view_returns_original_bytes)
{
    auto q = "hello-quark-view"_q;
    auto v = q.view();
    EXPECT_EQ(v.size(), 16U);
    EXPECT_EQ(std::string(v.data(), v.size()), "hello-quark-view");
    EXPECT_STREQ(q.c_str(), "hello-quark-view");
}

TEST(quark_basic, c_str_is_null_terminated)
{
    auto        q      = "foo.basic.cstr"_q;
    const auto* cstr   = q.c_str();
    const auto  length = q.size();
    EXPECT_EQ(cstr[length], '\0');
}

TEST(quark_basic, hash_matches_std_hash)
{
    auto                 q = "foo.basic.hash"_q;
    std::hash<mc::quark> hasher;
    EXPECT_EQ(hasher(q), q.hash());

    auto same = "foo.basic.hash"_q;
    EXPECT_EQ(hasher(q), hasher(same));
}

TEST(quark_basic, try_from_returns_invalid_for_unseen)
{
    auto unseen = mc::quark::try_from(mc::string_view("foo.basic.never_interned_xyz"));
    EXPECT_EQ(unseen.id(), mc::quark::invalid_id);

    auto seen = "foo.basic.try_from_seen"_q;
    auto back = mc::quark::try_from(mc::string_view("foo.basic.try_from_seen"));
    EXPECT_EQ(seen.id(), back.id());
}

TEST(quark_basic, ostream_writes_view)
{
    auto              q = "foo.basic.ostream"_q;
    std::stringstream ss;
    ss << q;
    EXPECT_EQ(ss.str(), "foo.basic.ostream");
}

TEST(quark_basic, hash_of_invalid_is_zero)
{
    mc::quark invalid;
    EXPECT_EQ(invalid.hash(), 0U);
}

TEST(quark_basic, invalid_id_constant_is_zero)
{
    EXPECT_EQ(mc::quark::invalid_id, 0U);
    EXPECT_EQ(mc::quark::empty_id, 1U);
}
