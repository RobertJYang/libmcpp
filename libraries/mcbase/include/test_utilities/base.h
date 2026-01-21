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

#ifndef MC_TEST_BASE_H
#define MC_TEST_BASE_H

#include <gtest/gtest.h>
#include <mc/filesystem.h>
#include <mc/log.h>
#include <mc/runtime.h>
#include <string>

namespace mc {
namespace test {

/**
 * @brief 测试基类，提供通用的测试功能
 */
class MC_API TestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 设置默认日志级别为 warn，减少测试输出
        mc::log::default_logger().set_level(mc::log::level::warn);
    }

    static void TearDownTestSuite() {
        // 恢复默认日志级别
        mc::log::default_logger().set_level(mc::log::level::info);
    }

    void SetUp() override {
        // 每个测试用例开始时确保日志级别为 warn
        mc::log::default_logger().set_level(mc::log::level::warn);
    }

    void TearDown() override {
        // 每个测试用例结束时恢复日志级别为 info
        mc::log::default_logger().set_level(mc::log::level::info);
    }
};

/**
 * @brief 带 runtime 支持的测试基类
 */
class MC_API TestWithRuntime : public TestBase {
protected:
    static mc::runtime::runtime_context& get_runtime() {
        return mc::runtime::get_runtime_context();
    }

    static void reset_runtime() {
        mc::runtime::reset_runtime_context();
    }

    static void SetUpTestSuite() {
        TestBase::SetUpTestSuite();
        mc::runtime::reset_runtime_context();
    }

    static void TearDownTestSuite() {
        reset_runtime();
        TestBase::TearDownTestSuite();
    }
};

/**
 * @brief 获取当前可执行文件的路径
 * @return 可执行文件的绝对路径，失败时返回空字符串
 */
MC_API std::string get_executable_path();

/**
 * @brief 获取构建根目录
 *
 * 通过可执行文件路径推断构建根目录。测试可执行文件通常位于 builddir/tests/ 目录下，
 * 因此构建根目录是可执行文件路径的父目录的父目录。
 *
 * 如果无法获取可执行文件路径，则尝试使用环境变量 MC_BUILD_ROOT 或回退到当前路径。
 *
 * @return 构建根目录路径
 */
MC_API mc::filesystem::path get_build_root();

} // namespace test
} // namespace mc

#endif // MC_TEST_BASE_H
