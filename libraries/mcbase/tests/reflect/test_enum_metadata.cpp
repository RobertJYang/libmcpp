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

#include <array>
#include <mc/reflect/metadata.h>
#include <mc/variant.h>
#include <string>
#include <vector>

namespace {
enum class color {
    red   = 1,
    green = 2,
    blue  = 3,
};

constexpr std::array members{
    mc::reflect::enum_member_info{"red", color::red},
    mc::reflect::enum_member_info{"green", color::green},
    mc::reflect::enum_member_info{"blue", color::blue},
};

mc::reflect::enum_metadata make_color_metadata()
{
    return mc::reflect::enum_metadata("color", mc::reflect::enum_values(members));
}
} // namespace

TEST(enum_metadata_test, value_lookup_and_names)
{
    auto meta  = make_color_metadata();
    auto names = meta.get_names();
    ASSERT_EQ(names.size(), 3U);
    EXPECT_EQ(names[0], "red");
    EXPECT_EQ(names[1], "green");
    EXPECT_EQ(names[2], "blue");

    auto values = meta.get_values();
    ASSERT_EQ(values.count, members.size());
    EXPECT_EQ(values.members[0].value, static_cast<uint32_t>(color::red));

    EXPECT_TRUE(meta.has_value("red"));
    EXPECT_TRUE(meta.has_value(static_cast<uint32_t>(color::green)));
    EXPECT_EQ(meta.get_value("green"), static_cast<uint32_t>(color::green));

    auto maybe_none = meta.try_get_value("purple");
    EXPECT_FALSE(maybe_none.has_value());
}

TEST(enum_metadata_test, to_string_and_variant_helpers)
{
    auto meta = make_color_metadata();

    auto as_string = meta.get_name(static_cast<uint32_t>(color::green));
    EXPECT_EQ(as_string, "green");

    auto variant_value = meta.get_value_from_variant(mc::variant("blue"));
    EXPECT_EQ(variant_value, static_cast<uint32_t>(color::blue));

    auto maybe_name = meta.try_get_name(static_cast<uint32_t>(color::red));
    ASSERT_TRUE(maybe_name.has_value());
    EXPECT_EQ(*maybe_name, "red");

    auto maybe_value = meta.try_get_value_from_variant(mc::variant("unknown"));
    EXPECT_FALSE(maybe_value.has_value());
}

TEST(enum_metadata_test, get_values_snapshot)
{
    auto meta   = make_color_metadata();
    auto values = meta.get_values();

    ASSERT_EQ(values.count, members.size());
    std::vector<std::string_view> collected;
    for (size_t i = 0; i < values.count; ++i) {
        collected.emplace_back(values.members[i].name);
    }
    EXPECT_EQ(collected[0], "red");
    EXPECT_EQ(collected[1], "green");
    EXPECT_EQ(collected[2], "blue");
}
