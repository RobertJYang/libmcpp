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
#include <mc/debounce/continue.h>

using mc::debounce::Continue;

// 完整功能测试：连续相同值稳定、稳定后继续输入相同值、稳定后输入不同值需重新累积
TEST(debounce_continue, get_value)
{
    Continue ctn(3);
    std::optional<int> value;

    // 输入 3 次 1，第 3 次才稳定
    value = ctn.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = ctn.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = ctn.get_debounce_val(1);
    ASSERT_EQ(value, 1);

    // 稳定后继续输入相同值，仍然返回稳定值
    value = ctn.get_debounce_val(1);
    ASSERT_EQ(value, 1);

    // 稳定后输入不同值 2，在达到阈值前保持输出之前的稳定值 1
    value = ctn.get_debounce_val(2);
    ASSERT_EQ(value, 1);
    value = ctn.get_debounce_val(2);
    ASSERT_EQ(value, 1);

    // 输入 2 次 2 后，新值稳定为 2
    value = ctn.get_debounce_val(2);
    ASSERT_EQ(value, 2);

    // 稳定后输入不同值 0，在达到阈值前保持输出之前的稳定值 2
    value = ctn.get_debounce_val(0);
    ASSERT_EQ(value, 2);
}

// 清除状态
TEST(debounce_continue, clear_state)
{
    Continue ctn(2);
    EXPECT_FALSE(ctn.get_debounce_val(5).has_value());
    auto v = ctn.get_debounce_val(5);
    ASSERT_TRUE(v.has_value());
    ctn.clear_debounce_val();
    // 清除后首次输入即会稳定（因为 clear_debounce_val 会将 m_count 设为 0，导致阈值判断立即满足）
    auto v2 = ctn.get_debounce_val(5);
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), 5);
}

// 构造参数校验
TEST(debounce_continue, invalid_count_throw)
{
    EXPECT_THROW(Continue(0), std::runtime_error);
    EXPECT_THROW(Continue(-1), std::runtime_error);
}


