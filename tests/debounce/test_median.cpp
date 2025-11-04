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
#include <mc/debounce/median.h>

using mc::debounce::Median;

// 偶数窗口大小测试（窗口=5）
TEST(debounce_median, even_window_size)
{
    Median median(5, false);
    std::optional<int> value;

    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(2);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(4);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(5);
    // 排序后为 [1,1,2,4,5]，去掉最小1和最大5，剩余 [1,2,4]，中位数为 2
    ASSERT_EQ(value, 2);
}

// 奇数窗口大小测试（窗口=4）和窗口滑动行为
TEST(debounce_median, odd_window_size_and_sliding)
{
    Median median(4, false);
    std::optional<int> value;

    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(2);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(4);
    // 排序后为 [1,1,2,4]，去掉最小1和最大4，剩余 [1,2]，中位数为 2
    ASSERT_EQ(value, 2);

    value = median.get_debounce_val(5);
    // 窗口滑动后为 [1,2,4,5]，去掉最小1和最大5，剩余 [2,4]，中位数为 4
    ASSERT_EQ(value, 4);

    value = median.get_debounce_val(5);
    // 窗口滑动后为 [2,4,5,5]，去掉最小2和最大5，剩余 [4,5]，中位数为 5
    ASSERT_EQ(value, 5);
}

// 有符号输入（>127 按补码视为负数）与输出调整
TEST(debounce_median, signed_input_and_adjust_output)
{
    Median median(5, true);
    std::optional<int> value;

    value = median.get_debounce_val(255); // 255: -1
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(254); // 254: -2
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(253); // 253: -3
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(252); // 252: -4
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1); // 输入: [-4,-3,-2,-1,1]
    // 排序后为 [-4,-3,-2,-1,1]，去掉最小-4和最大1，剩余 [-3,-2,-1]，中位数为 -2
    // 输出调整：-2 + 256 = 254
    ASSERT_EQ(value, 254);

    value = median.get_debounce_val(1); // 窗口滑动后为 [-4,-3,-2,1,1]
    // 排序后为 [-4,-3,-2,1,1]，去掉最小-4和最大1，剩余 [-3,-2,1]，中位数为 -2
    // 输出调整：-2 + 256 = 254
    ASSERT_EQ(value, 254);
}

// 清除状态测试，合并到基础测试中避免单独触发清理问题
TEST(debounce_median, clear_state)
{
    Median median(5, false);
    EXPECT_FALSE(median.get_debounce_val(100).has_value());
    EXPECT_FALSE(median.get_debounce_val(0).has_value());
    EXPECT_FALSE(median.get_debounce_val(10).has_value());
    EXPECT_FALSE(median.get_debounce_val(20).has_value());
    auto v = median.get_debounce_val(30);
    ASSERT_TRUE(v.has_value());
    // 排序 [0,10,20,30,100] -> 去头尾 [10,20,30]，中位数为 20
    EXPECT_EQ(v.value(), 20);
    median.clear_debounce_val();
    EXPECT_FALSE(median.get_debounce_val(1).has_value());
}

// 构造参数校验
TEST(debounce_median, invalid_size_throw)
{
    EXPECT_THROW(Median(0, false), std::runtime_error);
    EXPECT_THROW(Median(-2, true), std::runtime_error);
}


