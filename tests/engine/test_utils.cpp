/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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
#include <mc/engine/utils.h>

class utils_test : public ::testing::Test {
protected:
    utils_test()
    {}

    void SetUp() override
    {}

    void TearDown() override
    {}
};

TEST_F(utils_test, test_is_valid_interface_name)
{
    // 测试无效接口名
    EXPECT_FALSE(mc::is_valid_interface_name(""));
    EXPECT_FALSE(mc::is_valid_interface_name(".invalid"));
    EXPECT_FALSE(mc::is_valid_interface_name("invalid."));
    EXPECT_FALSE(mc::is_valid_interface_name("invalid..name"));
    EXPECT_FALSE(mc::is_valid_interface_name("invalid.123"));
    EXPECT_FALSE(mc::is_valid_interface_name("invalid.name$"));
    EXPECT_FALSE(mc::is_valid_interface_name("Invalid Interface Name"));
    EXPECT_FALSE(mc::is_valid_interface_name("no_dots"));

    // 测试有效接口名
    EXPECT_TRUE(mc::is_valid_interface_name("org.test.TestInterface"));
    EXPECT_TRUE(mc::is_valid_interface_name("com.example.MyInterface"));
    EXPECT_TRUE(mc::is_valid_interface_name("a.b.c"));
    EXPECT_TRUE(mc::is_valid_interface_name("x.y"));
    EXPECT_TRUE(mc::is_valid_interface_name("interface.with_underscore.name123"));
}
