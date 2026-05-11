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

#include <mc/quark.h>

namespace {

mc::quark _via_macro_a()
{
    return MC_QUARK("foo.macro.same.literal");
}

mc::quark _via_macro_b()
{
    return MC_QUARK("foo.macro.same.literal");
}

} // namespace

TEST(quark_macro, multiple_call_sites_same_id)
{
    auto a = MC_QUARK("foo.macro.cs.a");
    auto b = MC_QUARK("foo.macro.cs.a");
    EXPECT_EQ(a.id(), b.id());
}

TEST(quark_macro, cross_function_same_literal_dedup)
{
    EXPECT_EQ(_via_macro_a().id(), _via_macro_b().id());
}

TEST(quark_macro, macro_matches_try_from)
{
    auto by_macro   = MC_QUARK("foo.macro.match.runtime");
    auto by_lookup  = mc::quark::try_from(mc::string_view("foo.macro.match.runtime"));
    EXPECT_EQ(by_macro.id(), by_lookup.id());
}

TEST(quark_macro, udl_matches_try_from)
{
    using namespace mc;
    auto by_udl    = "foo.macro.match.udl"_q;
    auto by_lookup = mc::quark::try_from(mc::string_view("foo.macro.match.udl"));
    EXPECT_EQ(by_udl.id(), by_lookup.id());
}

TEST(quark_macro, multiple_macros_share_global_table)
{
    auto a = MC_QUARK("foo.macro.share.A");
    auto b = MC_QUARK("foo.macro.share.B");
    auto c = MC_QUARK("foo.macro.share.A");

    EXPECT_NE(a.id(), b.id());
    EXPECT_EQ(a.id(), c.id());
    EXPECT_EQ(a.view(), mc::string_view("foo.macro.share.A"));
    EXPECT_EQ(b.view(), mc::string_view("foo.macro.share.B"));
}

TEST(quark_macro, macro_in_loop_does_not_realloc)
{
    auto first_id = MC_QUARK("foo.macro.loop.literal").id();
    for (int i = 0; i < 1000; ++i) {
        auto q = MC_QUARK("foo.macro.loop.literal");
        EXPECT_EQ(q.id(), first_id);
    }
}
