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

/**
 * @file test_string_public_header.cpp
 * @brief 验证 mc/string.h 单独包含时即可使用公开成员接口
 */

#include <gtest/gtest.h>

#include <mc/string.h>

namespace mc {
namespace test {

TEST(StringPublicHeaderTest, StringHeaderExposesInlineMemberDefinitions)
{
    mc::string value = "prefix_body_tail";

    EXPECT_EQ(value.to_lower(), "prefix_body_tail");
    EXPECT_TRUE(value.starts_with("pre"));
    EXPECT_TRUE(value.ends_with('l'));
    EXPECT_TRUE(value.contains("body"));
    EXPECT_TRUE(value.contains('b'));
    EXPECT_EQ(value.replace_all("body", "core"), "prefix_core_tail");
    EXPECT_TRUE(mc::string::contains("prefix_body_tail", "body"));
    EXPECT_EQ(mc::string::replace_all("prefix_body_tail", "body", "core"), "prefix_core_tail");
}

} // namespace test
} // namespace mc
