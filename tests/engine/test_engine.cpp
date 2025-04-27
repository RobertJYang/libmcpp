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
#include <mc/dbus/connection.h>
#include <mc/engine.h>
#include <mc/singleton.h>
#include <test_utilities/test_base.h>

namespace {

class engine_test : public mc::test::TestBaseWithEngine {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

struct test_service : public mc::engine::service {
    test_service() : mc::engine::service("org.openubmc.test_service") {
    }
};

} // namespace

TEST_F(engine_test, test_engine) {
    test_service service;

    service.init();
    service.start();

    auto strand = mc::engine::make_strand();
    auto conn   = mc::dbus::connection::open_session_bus(strand);
    conn->start();

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn->send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());

    std::set<std::string> names;
    reply >> names;
    EXPECT_GE(names.count("org.openubmc.test_service"), 1);

    service.stop();
}
