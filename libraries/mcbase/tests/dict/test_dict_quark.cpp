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

#include <mc/dict.h>
#include <mc/quark.h>
#include <mc/string.h>

using namespace mc;

namespace {

mc::dict _make_quark_keyed_dict()
{
    mc::dict d;
    d["dict.quark.key.alpha"_q] = 1;
    d["dict.quark.key.beta"_q]  = 2;
    d["dict.quark.key.gamma"_q] = 3;
    return d;
}

} // namespace

TEST(dict_quark, lookup_with_quark)
{
    mc::dict d = _make_quark_keyed_dict();

    EXPECT_TRUE(d.contains("dict.quark.key.alpha"_q));
    EXPECT_TRUE(d.contains("dict.quark.key.beta"_q));
    EXPECT_TRUE(d.contains("dict.quark.key.gamma"_q));
    EXPECT_FALSE(d.contains(mc::quark::try_from(mc::string_view("dict.quark.key.never"))));
}

TEST(dict_quark, lookup_quark_keyed_via_native_string)
{
    mc::dict d = _make_quark_keyed_dict();

    EXPECT_TRUE(d.contains(mc::string("dict.quark.key.alpha")));
    EXPECT_TRUE(d.contains(mc::string_view("dict.quark.key.beta")));
    EXPECT_TRUE(d.contains("dict.quark.key.gamma"));
}

TEST(dict_quark, lookup_heap_keyed_via_quark)
{
    mc::dict d;
    d["dict.quark.heap.alpha"] = 11;
    d["dict.quark.heap.beta"]  = 22;

    EXPECT_TRUE(d.contains("dict.quark.heap.alpha"_q));
    EXPECT_TRUE(d.contains("dict.quark.heap.beta"_q));
}

TEST(dict_quark, retrieved_values_match)
{
    mc::dict d = _make_quark_keyed_dict();
    EXPECT_EQ(d["dict.quark.key.alpha"_q].as_int32(), 1);
    EXPECT_EQ(d.at("dict.quark.key.beta"_q).as_int32(), 2);
    EXPECT_EQ(d.get("dict.quark.key.gamma"_q, mc::variant{0}).as_int32(), 3);
}

TEST(dict_quark, find_and_erase_with_quark)
{
    mc::dict d = _make_quark_keyed_dict();

    auto it = d.find("dict.quark.key.beta"_q);
    ASSERT_NE(it, d.end());

    EXPECT_GE(d.find_index("dict.quark.key.alpha"_q), 0);
    EXPECT_LT(d.find_index(mc::quark::try_from(mc::string_view("dict.quark.key.never"))), 0);

    EXPECT_TRUE(d.erase("dict.quark.key.alpha"_q));
    EXPECT_FALSE(d.contains("dict.quark.key.alpha"_q));
}
