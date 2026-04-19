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
#include <mc/core/application.h>
#include <mc/engine.h>
#include <mc/log.h>
#include <test_utilities/test_base.h>

#include <thread>

int main(int argc, char* argv[])
{
    mc::test::dbus_daemon_manager dbus_daemon;
    dbus_daemon.start();

    ilog("服务连接示例启动");

    test::test_service service("org.openubmc.test_service");
    service.init({});
    service.start();

    mc::core::app().start();
    mc::core::app().exec();

    ilog("服务连接示例结束");
    return 0;
}