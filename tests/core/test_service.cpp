/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <mc/core/service.h>

#include <map>
#include <string>
#include <vector>

namespace {

class test_service_base : public mc::core::service_base {
public:
    using service_base::service_base;

    void set_state_public(mc::core::service_state state) {
        set_state(state);
    }

    void set_dependencies_public(const std::vector<std::string>& deps) {
        set_dependencies(deps);
    }
};

} // namespace

TEST(service_base_test, default_initialization) {
    test_service_base svc;
    EXPECT_EQ(svc.name(), "");
    EXPECT_EQ(svc.get_state(), mc::core::service_state::stopped);
    EXPECT_TRUE(svc.get_dependencies().empty());

    auto& config_ref1 = svc.get_config();
    auto& config_ref2 = svc.get_config();
    EXPECT_EQ(&config_ref1, &config_ref2);
}

TEST(service_base_test, constructor_and_mutators) {
    test_service_base svc("initial_name");
    EXPECT_EQ(svc.name(), "initial_name");

    svc.set_name("updated_name");
    EXPECT_EQ(svc.name(), "updated_name");

    svc.set_state_public(mc::core::service_state::running);
    EXPECT_EQ(svc.get_state(), mc::core::service_state::running);

    std::vector<std::string> deps{"db", "net"};
    svc.set_dependencies_public(deps);
    EXPECT_EQ(svc.get_dependencies(), deps);
}

TEST(service_base_test, default_behavior_methods) {
    test_service_base svc;

    mc::dict args;
    EXPECT_TRUE(svc.init(args));
    EXPECT_TRUE(svc.start());
    EXPECT_TRUE(svc.is_healthy());
    EXPECT_TRUE(svc.stop());

    svc.cleanup();

    std::map<std::string, std::string> ctx;
    svc.on_dump(ctx, "dump.log");
    svc.on_detach_debug_console(ctx);
    EXPECT_EQ(svc.on_reboot_prepare(ctx), 0);
    EXPECT_EQ(svc.on_reboot_process(ctx), 0);
    EXPECT_EQ(svc.on_reboot_action(ctx), 0);
    svc.on_reboot_cancel(ctx);

    EXPECT_EQ(svc.get_state(), mc::core::service_state::stopped);
    EXPECT_TRUE(svc.get_dependencies().empty());
}
