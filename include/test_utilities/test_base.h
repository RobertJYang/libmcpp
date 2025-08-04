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
#include <mc/core/application.h>
#include <mc/engine.h>
#include <mc/log.h>
#include <mc/singleton.h>
#include <test_utilities/dbus_daemon_manager.h>

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

class MC_API TestWithRuntime : public TestBase {
protected:
    static mc::runtime::runtime_context& get_runtime() {
        return mc::runtime::get_runtime_context();
    }

    static void reset_runtime() {
        mc::runtime::reset_runtime_context();
    }

    static void SetUpTestSuite() {
        mc::runtime::reset_runtime_context();
    }

    static void TearDownTestSuite() {
        reset_runtime();
    }
};

class MC_API TestWithDbusDaemon : public TestWithRuntime {
protected:
    static dbus_daemon_manager& get_dbus_daemon() {
        return mc::singleton<dbus_daemon_manager>::instance();
    }

    static void SetUpTestSuite() {
        TestWithRuntime::SetUpTestSuite();
        ASSERT_TRUE(get_dbus_daemon().start()) << "启动 DBus 守护进程失败";
    }

    static void TearDownTestSuite() {
        TestWithRuntime::TearDownTestSuite();
    };
};

class MC_API TestWithApplication : public TestWithDbusDaemon {
protected:
    static void SetUpTestSuite() {
        TestWithDbusDaemon::SetUpTestSuite();

        mc::core::app().start();
    }

    static void TearDownTestSuite() {
        mc::core::app().stop();
        mc::core::application::reset_for_test();
        TestWithDbusDaemon::TearDownTestSuite();
    };
};

class MC_API TestWithEngine : public TestWithApplication {
protected:
    static mc::engine::engine& get_engine() {
        return mc::engine::get_engine();
    }

    static void SetUpTestSuite() {
        TestWithApplication::SetUpTestSuite();
    }

    static void TearDownTestSuite() {
        mc::engine::engine::reset_for_test();
        TestWithApplication::TearDownTestSuite();
    };
};

} // namespace test
} // namespace mc

#endif // MC_TEST_BASE_H