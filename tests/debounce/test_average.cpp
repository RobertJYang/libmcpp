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
#include <mc/debounce/average.h>

using mc::debounce::Average;

// 基础窗口平均（无符号）
TEST(debounce_average, basic_window_unsigned)
{
    Average avg(3, false);
    EXPECT_FALSE(avg.get_debounce_val(1).has_value());
    EXPECT_FALSE(avg.get_debounce_val(2).has_value());
    auto v = avg.get_debounce_val(7);
    ASSERT_TRUE(v.has_value());
    // 实现采用去掉最小与最大后再平均（窗口=3 等价于取中间值）
    EXPECT_EQ(v.value(), 2);
}

// 窗口裁剪最小最大后再平均（窗口>=3）
TEST(debounce_average, trim_min_max_then_average)
{
    Average avg(5, false);
    EXPECT_FALSE(avg.get_debounce_val(100).has_value());
    EXPECT_FALSE(avg.get_debounce_val(0).has_value());
    EXPECT_FALSE(avg.get_debounce_val(10).has_value());
    EXPECT_FALSE(avg.get_debounce_val(20).has_value());
    auto v = avg.get_debounce_val(30);
    ASSERT_TRUE(v.has_value());
    // 排序后为 [0,10,20,30,100]，去掉最小最大，平均 (10+20+30)/3 = 20
    EXPECT_EQ(v.value(), 20);
}

// 偶数窗口测试（窗口=4）和窗口滑动行为
TEST(debounce_average, even_window_size_and_sliding)
{
    Average avg(4, false);
    std::optional<int> value;

    value = avg.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);

    value = avg.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);

    value = avg.get_debounce_val(2);
    ASSERT_EQ(value, std::nullopt);

    value = avg.get_debounce_val(4);
    ASSERT_TRUE(value.has_value());
    // 排序后为 [1,1,2,4]，去掉最小1和最大4，平均 (1+2)/2 = 1
    EXPECT_EQ(value.value(), 1);

    value = avg.get_debounce_val(5);
    ASSERT_TRUE(value.has_value());
    // 窗口滑动后为 [1,2,4,5]，去掉最小1和最大5，平均 (2+4)/2 = 3
    EXPECT_EQ(value.value(), 3);

    value = avg.get_debounce_val(5);
    ASSERT_TRUE(value.has_value());
    // 窗口滑动后为 [2,4,5,5]，去掉最小2和最大5，平均 (4+5)/2 = 4
    EXPECT_EQ(value.value(), 4);
}

// 有符号输入（>127 按补码视为负数）与输出调整（窗口=3）
TEST(debounce_average, signed_input_and_adjust_output)
{
    Average avg(3, true);
    // 输入 200 视为 200-256=-56
    EXPECT_FALSE(avg.get_debounce_val(200).has_value());
    EXPECT_FALSE(avg.get_debounce_val(1).has_value());
    auto v = avg.get_debounce_val(1);
    ASSERT_TRUE(v.has_value());
    // 窗口=3 时等价于取中间值，结果为 1，无需调整
    EXPECT_EQ(v.value(), 1);
}

// 有符号输入完整测试（窗口=5，多个负数）
TEST(debounce_average, signed_input_multiple_negatives)
{
    Average avg(5, true);
    // 输入 255 视为 255-256=-1，254 视为 -2，253 视为 -3，252 视为 -4
    EXPECT_FALSE(avg.get_debounce_val(255).has_value()); // -1
    EXPECT_FALSE(avg.get_debounce_val(254).has_value()); // -2
    EXPECT_FALSE(avg.get_debounce_val(253).has_value()); // -3
    EXPECT_FALSE(avg.get_debounce_val(252).has_value()); // -4
    auto v = avg.get_debounce_val(1); // 1
    ASSERT_TRUE(v.has_value());
    // 排序后为 [-4,-3,-2,-1,1]，去掉最小-4和最大1，平均 (-3-2-1)/3 = -2
    // 输出调整：-2 + 256 = 254
    EXPECT_EQ(v.value(), 254);

    // 继续输入，测试窗口滑动
    auto v2 = avg.get_debounce_val(1);
    ASSERT_TRUE(v2.has_value());
    // 窗口滑动后为 [-3,-2,-1,1,1]，去掉最小-3和最大1，平均 (-2-1)/2 = -1.5 -> -1（整数除法）
    // 输出调整：-1 + 256 = 255
    EXPECT_EQ(v2.value(), 255);
}

// 清除状态后应回到无效
TEST(debounce_average, clear_state)
{
    Average avg(2, false);
    EXPECT_FALSE(avg.get_debounce_val(10).has_value());
    auto v1 = avg.get_debounce_val(20);
    ASSERT_TRUE(v1.has_value());
    avg.clear_debounce_val();
    EXPECT_FALSE(avg.get_debounce_val(5).has_value());
}

// 构造参数校验
TEST(debounce_average, invalid_size_throw)
{
    EXPECT_THROW(Average(0, false), std::runtime_error);
    EXPECT_THROW(Average(-1, true), std::runtime_error);
}
