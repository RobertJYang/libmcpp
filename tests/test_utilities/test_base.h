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
#include <mc/log.h>

namespace mc {
namespace test {

/**
 * @brief 测试基类，提供通用的测试功能
 */
class TestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置默认日志级别为 warn，减少测试输出
        mc::log::default_logger().set_level(mc::log::level::warn);
    }

    void TearDown() override {
        // 恢复默认日志级别
        mc::log::default_logger().set_level(mc::log::level::info);
    }
};

} // namespace test
} // namespace mc

#endif // MC_TEST_BASE_H