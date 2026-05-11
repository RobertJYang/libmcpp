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

#include <mc/engine/match/path_pattern.h>

#include <gtest/gtest.h>

namespace {

using mc::engine::match::path_pattern;
using mc::engine::match::split_path_segments;

TEST(path_pattern_split, empty_input_returns_empty)
{
    EXPECT_TRUE(split_path_segments("").empty());
}

TEST(path_pattern_split, single_root_returns_empty)
{
    EXPECT_TRUE(split_path_segments("/").empty());
    EXPECT_TRUE(split_path_segments("////").empty());
}

TEST(path_pattern_split, normalises_consecutive_separators)
{
    auto segs = split_path_segments("//foo///bar/");
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_EQ(segs[0], "foo");
    EXPECT_EQ(segs[1], "bar");
}

TEST(path_pattern_split, no_leading_slash_is_supported)
{
    auto segs = split_path_segments("foo/bar/baz");
    ASSERT_EQ(segs.size(), 3u);
    EXPECT_EQ(segs[0], "foo");
    EXPECT_EQ(segs[1], "bar");
    EXPECT_EQ(segs[2], "baz");
}

TEST(path_pattern_well_formed, basic_supported_patterns_are_well_formed)
{
    EXPECT_TRUE(path_pattern::is_well_formed(""));
    EXPECT_TRUE(path_pattern::is_well_formed("/"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/bar"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/*"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/*/bar"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/**"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/bar/"));
    EXPECT_TRUE(path_pattern::is_well_formed("/foo/{name}"));
}

TEST(path_pattern_well_formed, intra_segment_wildcard_is_rejected)
{
    EXPECT_FALSE(path_pattern::is_well_formed("/Cpu*"));
    EXPECT_FALSE(path_pattern::is_well_formed("/foo/bar*"));
    EXPECT_FALSE(path_pattern::is_well_formed("/foo*/bar"));
}

TEST(path_pattern_well_formed, intermediate_doublestar_is_rejected)
{
    EXPECT_FALSE(path_pattern::is_well_formed("/foo/**/bar"));
    EXPECT_FALSE(path_pattern::is_well_formed("/**/bar"));
}

TEST(path_pattern_well_formed, typed_capture_is_rejected)
{
    EXPECT_FALSE(path_pattern::is_well_formed("/foo/{name:int}"));
    EXPECT_FALSE(path_pattern::is_well_formed("/foo/{name=default}"));
}

TEST(path_pattern_match, full_wildcard_matches_anything)
{
    path_pattern pat("");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::full_wildcard);
    EXPECT_TRUE(pat.matches(""));
    EXPECT_TRUE(pat.matches("/"));
    EXPECT_TRUE(pat.matches("/Chassis/0"));
    EXPECT_TRUE(pat.matches("/a/b/c/d"));
}

TEST(path_pattern_match, exact_pattern_requires_same_segments)
{
    path_pattern pat("/Chassis/0/Cpu");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::exact);
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu"));
    EXPECT_TRUE(pat.matches("Chassis/0/Cpu"));     // 缺前导 '/' 也按 segment 等价
    EXPECT_TRUE(pat.matches("//Chassis//0//Cpu/")); // 多余分隔符等价
    EXPECT_FALSE(pat.matches("/Chassis/0"));
    EXPECT_FALSE(pat.matches("/Chassis/0/Cpu/2"));
    EXPECT_FALSE(pat.matches("/Other/0/Cpu"));
}

TEST(path_pattern_match, single_segment_wildcard)
{
    path_pattern pat("/Chassis/*/Cpu");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::exact);
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu"));
    EXPECT_TRUE(pat.matches("/Chassis/blade-1/Cpu"));
    EXPECT_FALSE(pat.matches("/Chassis/0/1/Cpu"));   // 中间多一段
    EXPECT_FALSE(pat.matches("/Chassis/Cpu"));        // 中间少一段
    EXPECT_FALSE(pat.matches("/Chassis/0/Other"));
}

TEST(path_pattern_match, doublestar_matches_zero_or_more_suffix_segments)
{
    path_pattern pat("/Chassis/0/**");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::prefix_doublestar);
    EXPECT_TRUE(pat.matches("/Chassis/0"));
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu"));
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu/2/Core"));
    EXPECT_FALSE(pat.matches("/Chassis"));
    EXPECT_FALSE(pat.matches("/Chassis/1/Cpu"));
}

TEST(path_pattern_match, trailing_slash_is_doublestar_shorthand)
{
    path_pattern pat("/Chassis/0/");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::prefix_doublestar);
    EXPECT_TRUE(pat.matches("/Chassis/0"));
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu/2"));
    EXPECT_FALSE(pat.matches("/Chassis/1"));
}

TEST(path_pattern_match, brace_capture_is_treated_as_single_wildcard)
{
    path_pattern pat("/Chassis/{slot}/Cpu");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::exact);
    ASSERT_EQ(pat.segments().size(), 3u);
    EXPECT_EQ(pat.segments()[1], "*");
    EXPECT_TRUE(pat.matches("/Chassis/0/Cpu"));
    EXPECT_TRUE(pat.matches("/Chassis/blade-1/Cpu"));
    EXPECT_FALSE(pat.matches("/Chassis/0/1/Cpu"));
}

TEST(path_pattern_match, illegal_pattern_falls_back_to_full_wildcard)
{
    path_pattern pat("/Cpu*");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::full_wildcard);
    EXPECT_TRUE(pat.matches("/anything/at/all"));
}

TEST(path_pattern_match, doublestar_only_pattern_is_full_wildcard_like)
{
    path_pattern pat("/**");
    EXPECT_EQ(pat.get_kind(), path_pattern::kind::prefix_doublestar);
    EXPECT_TRUE(pat.matches(""));
    EXPECT_TRUE(pat.matches("/"));
    EXPECT_TRUE(pat.matches("/anything"));
    EXPECT_TRUE(pat.matches("/a/b/c"));
}

} // namespace
