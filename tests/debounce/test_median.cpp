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

// 测试有符号输入（>127 按补码视为负数）与输出调整，包括窗口滑动和多种负数场景
TEST(debounce_median, signed_input_and_adjust_output)
{
    Median median(5, true); // is_signed = true
    std::optional<int> value;

    // 输入一系列值，使得计算出的中位数为负数
    // 输入: 255(-1), 254(-2), 253(-3), 252(-4), 251(-5)
    // 排序后为 [-5,-4,-3,-2,-1]，去掉最小-5和最大-1，剩余 [-4,-3,-2]，中位数为 -3
    // 输出调整：-3 + 256 = 253
    value = median.get_debounce_val(255); // -1
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(254); // -2
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(253); // -3
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(252); // -4
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(251); // -5
    // 排序后为 [-5,-4,-3,-2,-1]，去掉最小-5和最大-1，剩余 [-4,-3,-2]，中位数为 -3
    // adjust_signed(-3) = -3 + 256 = 253
    ASSERT_EQ(value, 253);

    // 再测试一个场景：输入混合值，使得中位数为负数
    median.clear_debounce_val();
    // 输入: 255(-1), 254(-2), 253(-3), 0, 1
    // 排序后为 [-3,-2,-1,0,1]，去掉最小-3和最大1，剩余 [-2,-1,0]，中位数为 -1
    // 输出调整：-1 + 256 = 255
    value = median.get_debounce_val(255); // -1
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(254); // -2
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(253); // -3
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(0);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1);
    // 排序后为 [-3,-2,-1,0,1]，去掉最小-3和最大1，剩余 [-2,-1,0]，中位数为 -1
    // adjust_signed(-1) = -1 + 256 = 255
    ASSERT_EQ(value, 255);
}


