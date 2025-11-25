/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* openUBMC is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*         http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include <gtest/gtest.h>
#include <mc/debounce/binary_continue.h>

using mc::debounce::BinaryContinue;

// 针对二值输入，分别对 0/1 达到计数后稳定
TEST(debounce_binary_continue, become_stable_high_low)
{
    BinaryContinue bc(2, 3); // 1 需要 2 次，0 需要 3 次

    // 连续两个 1 -> 稳定为 1
    EXPECT_FALSE(bc.get_debounce_val(1).has_value());
    auto v1 = bc.get_debounce_val(1);
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1.value(), 1);

    // 切换到 0，需要 3 次。在达阈值前，保持上一次稳定输出 1
    {
        auto t1 = bc.get_debounce_val(0);
        ASSERT_TRUE(t1.has_value());
        EXPECT_EQ(t1.value(), 1);
    }
    {
        auto t2 = bc.get_debounce_val(0);
        ASSERT_TRUE(t2.has_value());
        EXPECT_EQ(t2.value(), 1);
    }
    auto v0 = bc.get_debounce_val(0);
    ASSERT_TRUE(v0.has_value());
    EXPECT_EQ(v0.value(), 0);
}

// 清除状态
TEST(debounce_binary_continue, clear_state)
{
    BinaryContinue bc(1, 1);
    auto v = bc.get_debounce_val(1);
    ASSERT_TRUE(v.has_value());
    bc.clear_debounce_val();
    // 实现将计数清零，清除后首次输入即会立刻稳定
    auto v2 = bc.get_debounce_val(1);
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), 1);
}

// 构造参数校验
TEST(debounce_binary_continue, invalid_count_throw)
{
    EXPECT_THROW(BinaryContinue(0, 1), std::runtime_error);
    EXPECT_THROW(BinaryContinue(1, 0), std::runtime_error);
}

// 测试稳定值重置不稳定计数
TEST(debounce_binary_continue, BinaryContinueStableValueResetsUnstableCount)
{
    BinaryContinue bc(2, 3); // 1 需要 2 次，0 需要 3 次

    // 先让值稳定为 1（连续输入 2 次 1）
    EXPECT_FALSE(bc.get_debounce_val(1).has_value());
    auto v1 = bc.get_debounce_val(1);
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1.value(), 1);

    // 现在继续输入相同的稳定值（1），这应该触发 m_unstable_val_count = 0 的分支
    // 当 m_stable_val == new_val 时，m_unstable_val_count 应该被重置为 0
    auto v2 = bc.get_debounce_val(1);
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), 1);

    // 再次输入相同的稳定值，验证仍然稳定
    auto v3 = bc.get_debounce_val(1);
    ASSERT_TRUE(v3.has_value());
    EXPECT_EQ(v3.value(), 1);

    // 现在切换到 0，需要 3 次才能稳定
    // 由于之前 m_unstable_val_count 被重置为 0，现在应该从 0 开始计数
    auto t1 = bc.get_debounce_val(0);
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(t1.value(), 1); // 仍然输出上一次稳定值 1
    auto t2 = bc.get_debounce_val(0);
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2.value(), 1); // 仍然输出上一次稳定值 1
    auto v0 = bc.get_debounce_val(0);
    ASSERT_TRUE(v0.has_value());
    EXPECT_EQ(v0.value(), 0); // 现在稳定为 0

    // 继续输入相同的稳定值（0），验证 m_unstable_val_count 被重置
    auto v4 = bc.get_debounce_val(0);
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(v4.value(), 0);
}


