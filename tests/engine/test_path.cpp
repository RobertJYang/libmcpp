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
#include <mc/engine/path.h>
#include <mc/exception.h>

using namespace mc::engine;

/**
 * 路径类单元测试
 */
class path_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 设置代码
    }

    void TearDown() override
    {
        // 清理代码
    }
};

/**
 * 测试路径构造函数
 */
TEST_F(path_test, constructor)
{
    // 默认构造函数创建根路径
    path root_path;
    EXPECT_EQ("/", root_path.str());

    // 使用字符串构造路径
    path p1("/org/freedesktop/DBus");
    EXPECT_EQ("/org/freedesktop/DBus", p1.str());

    // 使用C风格字符串构造路径
    path p2("/org/example/test");
    EXPECT_EQ("/org/example/test", p2.str());
}

/**
 * 测试无效路径
 */
TEST_F(path_test, invalid_path)
{
    // 不以'/'开头的路径无效
    EXPECT_THROW(path("org/example"), mc::invalid_arg_exception);

    // 包含非法字符的路径无效
    EXPECT_THROW(path("/org/example/test$"), mc::invalid_arg_exception);

    // 以'/'结尾的路径无效（除了根路径）
    EXPECT_THROW(path("/org/example/"), mc::invalid_arg_exception);

    // 包含连续'/'的路径无效
    EXPECT_THROW(path("/org//example"), mc::invalid_arg_exception);

    // 空路径无效
    EXPECT_THROW(path(""), mc::invalid_arg_exception);
}

/**
 * 测试路径比较操作符
 */
TEST_F(path_test, comparison_operators)
{
    path p1("/org/freedesktop/DBus");
    path p2("/org/freedesktop/DBus");
    path p3("/org/example");

    // 等于
    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);

    // 不等于
    EXPECT_FALSE(p1 != p2);
    EXPECT_TRUE(p1 != p3);

    // 小于（用于排序）
    EXPECT_TRUE(p3 < p1);
    EXPECT_FALSE(p1 < p3);
}

/**
 * 测试路径连接操作
 */
TEST_F(path_test, path_joining)
{
    path root("/");
    path p1("/org");

    // 从根路径连接
    path p2 = root / "freedesktop";
    EXPECT_EQ("/freedesktop", p2.str());

    // 连接多个元素
    path p3 = p1 / "freedesktop" / "DBus";
    EXPECT_EQ("/org/freedesktop/DBus", p3.str());

    // 无效元素
    EXPECT_THROW(p1 / "invalid/element", mc::invalid_arg_exception);
    EXPECT_THROW(p1 / "invalid$element", mc::invalid_arg_exception);
    EXPECT_THROW(p1 / "", mc::invalid_arg_exception);
}

/**
 * 测试父路径和基本名称方法
 */
TEST_F(path_test, parent_and_basename)
{
    path p1("/org/freedesktop/DBus");

    // 父路径
    path parent = p1.parent();
    EXPECT_EQ("/org/freedesktop", parent.str());

    // 根路径的父路径是自身
    path root("/");
    EXPECT_EQ("/", root.parent().str());

    // 基本名称
    EXPECT_EQ("DBus", p1.basename());

    // 根路径的基本名称是空字符串
    EXPECT_EQ("", root.basename());
}

/**
 * 测试路径验证方法
 */
TEST_F(path_test, validation)
{
    // 有效路径
    EXPECT_TRUE(path::is_valid("/"));
    EXPECT_TRUE(path::is_valid("/org"));
    EXPECT_TRUE(path::is_valid("/org/freedesktop"));
    EXPECT_TRUE(path::is_valid("/org/freedesktop/DBus"));
    EXPECT_TRUE(path::is_valid("/org/freedesktop/DBus_Test"));
    EXPECT_TRUE(path::is_valid("/org/freedesktop/DBus123"));

    // 无效路径
    EXPECT_FALSE(path::is_valid(""));
    EXPECT_FALSE(path::is_valid("org"));
    EXPECT_FALSE(path::is_valid("/org/"));
    EXPECT_FALSE(path::is_valid("/org//freedesktop"));
    EXPECT_FALSE(path::is_valid("/org/freedesktop$"));
    EXPECT_FALSE(path::is_valid("/org/freedesktop/"));

    // 有效元素
    EXPECT_TRUE(path::is_valid_element("org"));
    EXPECT_TRUE(path::is_valid_element("freedesktop"));
    EXPECT_TRUE(path::is_valid_element("DBus"));
    EXPECT_TRUE(path::is_valid_element("DBus_Test"));
    EXPECT_TRUE(path::is_valid_element("DBus123"));

    // 无效元素
    EXPECT_FALSE(path::is_valid_element(""));
    EXPECT_FALSE(path::is_valid_element("org/"));
    EXPECT_FALSE(path::is_valid_element("org$"));
    EXPECT_FALSE(path::is_valid_element("org freedesktop"));
}