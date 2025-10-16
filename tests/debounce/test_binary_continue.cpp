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
#include <mc/debounce/binary_continue.h>
#include <test_utilities/test_base.h>

using namespace mc;
using namespace mc::debounce;

namespace {
// 库测试类
class binary_continue_test : public mc::test::TestBase {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(binary_continue_test, GetValue) {
    mc::debounce::BinaryContinue binary_continue(3, 3);
    std::optional<int>           value;
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = binary_continue.get_debounce_val(0);
    ASSERT_EQ(value, std::nullopt);
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, std::nullopt);
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, 1);
    value = binary_continue.get_debounce_val(1);
    ASSERT_EQ(value, 1);
    value = binary_continue.get_debounce_val(0);
    ASSERT_EQ(value, 1);
    value = binary_continue.get_debounce_val(0);
    ASSERT_EQ(value, 1);
    value = binary_continue.get_debounce_val(0);
    ASSERT_EQ(value, 0);
}

} // namespace