/**
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

#include "test_service.h"
#include <mc/app/application.h>
#include <mc/log.h>
#include <test_utilities/test_base.h>

#include <csignal>

namespace {

mc::app::application* g_app = nullptr;

void handle_stop_signal(int)
{
    if (g_app != nullptr) {
        g_app->quit();
    }
}

} // namespace

int main(int argc, char* argv[])
{
    mc::test::dbus_daemon_manager dbus_daemon;
    dbus_daemon.start();

    ilog("服务连接示例启动");

    mc::app::application app;
    g_app = &app;
    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    app.register_service<test::test_service>("test.task_service");

    mc::app::service_plan plan;
    plan.application.io_threads   = 2;
    plan.application.work_threads = 1;
    mc::app::service_definition service;
    service.name = "org.openubmc.test_service";
    service.path = "/org/openubmc/test_service";
    service.type = "test.task_service";
    plan.services.push_back(std::move(service));

    if (!app.initialize_with_plan(std::move(plan))) {
        elog("服务连接示例初始化失败");
        return 1;
    }
    if (!app.start()) {
        elog("服务连接示例启动失败");
        return 1;
    }

    ilog("服务连接示例运行中，按 Ctrl+C 退出");
    auto exit_code = app.exec();
    g_app          = nullptr;
    ilog("服务连接示例结束");
    return exit_code;
}
