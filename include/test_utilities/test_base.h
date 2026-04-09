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

#ifndef MC_LIBMCPP_TEST_BASE_H
#define MC_LIBMCPP_TEST_BASE_H

#include <mc/core/application.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/engine.h>
#include <mc/singleton.h>

#include <test_utilities/base.h>
#include <test_utilities/dbus_daemon_manager.h>

namespace mc {
namespace test {

class MC_API TestWithDbusDaemon : public TestWithRuntime {
protected:
    static dbus_daemon_manager& get_dbus_daemon()
    {
        return mc::singleton<dbus_daemon_manager>::instance();
    }

    static void SetUpTestSuite()
    {
        mc::dbus::harbor::reset_for_test();
        TestWithRuntime::SetUpTestSuite();
        ASSERT_TRUE(get_dbus_daemon().start()) << "启动 DBus 守护进程失败";
    }

    static void TearDownTestSuite()
    {
        mc::dbus::harbor::reset_for_test();
        TestWithRuntime::TearDownTestSuite();
    };
};

class MC_API TestWithApplication : public TestWithDbusDaemon {
protected:
    static void SetUpTestSuite()
    {
        TestWithDbusDaemon::SetUpTestSuite();

        mc::core::app().start();
    }

    static void TearDownTestSuite()
    {
        mc::core::app().stop();
        mc::core::application::reset_for_test();
        TestWithDbusDaemon::TearDownTestSuite();
    };
};

class MC_API TestWithEngine : public TestWithApplication {
protected:
    static mc::engine::engine& get_engine()
    {
        return mc::engine::get_engine();
    }

    static void SetUpTestSuite()
    {
        mc::engine::engine::reset_for_test();
        TestWithApplication::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        TestWithApplication::TearDownTestSuite();
        mc::engine::engine::reset_for_test();
    }
};
} // namespace test
} // namespace mc

#endif // MC_LIBMCPP_TEST_BASE_H
