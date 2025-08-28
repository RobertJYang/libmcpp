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
#include <test_utilities/test_base.h>

using namespace mc;
using namespace mc::debounce;

namespace {
// 库测试类
class median_test : public mc::test::TestBase {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(median_test, EvenWindowSize)
{
    mc::debounce::Median median(5, false);
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
    ASSERT_EQ(value, 2);
}

TEST_F(median_test, OddWindowSize)
{
    mc::debounce::Median median(4, false);
    std::optional<int> value;
    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(2);
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(4);
    ASSERT_EQ(value, 2);
    value = median.get_debounce_val(5);
    ASSERT_EQ(value, 4);
    value = median.get_debounce_val(5);
    ASSERT_EQ(value, 5);
}

TEST_F(median_test, SignedTrue)
{
    mc::debounce::Median median(5, true);
    std::optional<int> value;
    value = median.get_debounce_val(255);   // 255: -1
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(254);   // 254: -2
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(253);   // 253：-3
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(252);   // 252：-4
    ASSERT_EQ(value, std::nullopt);
    value = median.get_debounce_val(1); // -4 -3 -2 -1 1
    ASSERT_EQ(value, 254);
    value = median.get_debounce_val(1); // -4 -3 -2 1 1
    ASSERT_EQ(value, 254);
}

} // namespace