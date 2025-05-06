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

#include <dbus_daemon_manager.h>
#include <gtest/gtest.h>
#include <mc/core/application.h>
#include <mc/engine.h>
#include <mc/log.h>
#include <mc/singleton.h>

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

class TestWithDbusDaemon : public TestBase {
protected:
    static dbus_daemon_manager& get_dbus_daemon() {
        return mc::singleton<dbus_daemon_manager>::instance();
    }

    static void SetUpTestSuite() {
        ASSERT_TRUE(get_dbus_daemon().start()) << "启动 DBus 守护进程失败";
    }

    static void TearDownTestSuite() {};
};

class TestWithEngine : public TestWithDbusDaemon {
protected:
    static mc::engine::engine& get_engine() {
        return mc::engine::get_engine();
    }

    static void SetUpTestSuite() {
        TestWithDbusDaemon::SetUpTestSuite();
        get_engine().start();
    }

    static void TearDownTestSuite() {
        get_engine().stop();
        get_engine().join();

        mc::singleton<mc::engine::engine>::reset_for_test();
        TestWithDbusDaemon::TearDownTestSuite();
    };
};

class TestWithApplication : public TestWithEngine {
protected:
    static void SetUpTestSuite() {
        TestWithEngine::SetUpTestSuite();

        mc::core::app().start();
    }

    static void TearDownTestSuite() {
        mc::core::app().stop();
        mc::singleton<mc::core::application>::reset_for_test();

        TestWithEngine::TearDownTestSuite();
    };
};

} // namespace test
} // namespace mc

#endif // MC_TEST_BASE_H