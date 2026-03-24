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

/**
 * @file test_pretty_name.cpp
 * @brief 测试pretty_name功能
 */
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/pretty_name.h>
#include <stdio.h>
#include <string>

// 自定义类型
struct custom_type {
    int value;
};

// 自定义模板类型
template <typename T>
struct templated_type {
    T value;
};

// 测试用的自定义类型
struct test_user {
    int         id;
    mc::string name;
    int         age;
};

// 模板类型
template <typename T>
struct wrapper {
    T value;
};

// pretty_name测试套件
class pretty_name_test : public ::testing::Test {
protected:
    void SetUp() override
    {}
    void TearDown() override
    {}
};

// 测试基本类型的名称
TEST_F(pretty_name_test, basic_types)
{
    // 基本类型
    mc::string int_name    = mc::pretty_name<int>();
    mc::string double_name = mc::pretty_name<double>();
    mc::string bool_name   = mc::pretty_name<bool>();

    // 检查名称中是否包含基本类型的核心部分
    EXPECT_TRUE(int_name.find("int") != mc::string::npos);
    EXPECT_TRUE(double_name.find("double") != mc::string::npos);
    EXPECT_TRUE(bool_name.find("bool") != mc::string::npos);
}

// 测试自定义类型的名称
TEST_F(pretty_name_test, custom_types)
{
    mc::string user_name = mc::pretty_name<test_user>();
    EXPECT_TRUE(user_name.find("test_user") != mc::string::npos);
}

// 测试模板类型的名称
TEST_F(pretty_name_test, template_types)
{
    mc::string int_wrapper = mc::pretty_name<wrapper<int>>();
    EXPECT_TRUE(int_wrapper.find("wrapper") != mc::string::npos);
    EXPECT_TRUE(int_wrapper.find("int") != mc::string::npos);

    mc::string string_wrapper = mc::pretty_name<wrapper<mc::string>>();
    EXPECT_TRUE(string_wrapper.find("wrapper") != mc::string::npos);
    EXPECT_TRUE(string_wrapper.find("string") != mc::string::npos);
}
