/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include <mc/app/service_registry.h>
#include <mc/reflect.h>

namespace test_mcapp {

struct registry_test_config {
    MC_REFLECTABLE("mc.test.RegistryConfig")

    std::string greeting{"default-greeting"};
};

class registry_test_service : public mc::app::service {
public:
    using config_type = registry_test_config;

    explicit registry_test_service(std::string name) : mc::app::service(std::move(name))
    {}

    bool on_start() override
    {
        return true;
    }

    bool on_stop() override
    {
        return true;
    }
};

class global_registry_test_service : public mc::app::service {
public:
    explicit global_registry_test_service(std::string name) : mc::app::service(std::move(name))
    {}

    bool on_start() override
    {
        return true;
    }

    bool on_stop() override
    {
        return true;
    }
};

} // namespace test_mcapp

MC_REFLECT(test_mcapp::registry_test_config, (greeting))
MC_APP_REGISTER_GLOBAL_SERVICE(test_mcapp::global_registry_test_service, "global_registry_test_service")

TEST(service_registry_test, register_service_exposes_descriptor_and_creator)
{
    mc::app::service_registry registry;
    registry.register_service<test_mcapp::registry_test_service>("registry_test_service");

    auto descriptor = registry.find("registry_test_service");
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->type_name, "registry_test_service");

    auto service = registry.create_service("registry_test_service", "mc.test.demo");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "mc.test.demo");
}

TEST(service_registry_test, validate_properties_uses_config_type)
{
    mc::app::service_registry registry;
    registry.register_service<test_mcapp::registry_test_service>("registry_test_service");

    auto descriptor = registry.find("registry_test_service");
    ASSERT_NE(descriptor, nullptr);

    mc::dict good_properties;
    good_properties["greeting"] = "hello";
    EXPECT_TRUE(registry.validate_service_properties("registry_test_service", good_properties));

    mc::dict bad_properties;
    bad_properties["greeting"] = mc::dict{};
    EXPECT_FALSE(registry.validate_service_properties("registry_test_service", bad_properties));
}

TEST(service_registry_test, register_service_exposes_config_metadata)
{
    mc::app::service_registry registry;
    registry.register_service<test_mcapp::registry_test_service>("registry_test_service");

    auto descriptor = registry.find("registry_test_service");
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->config.version, "v1");
    ASSERT_TRUE(descriptor->config.default_properties.contains("greeting"));
    EXPECT_EQ(descriptor->config.default_properties["greeting"].as<std::string>(), "default-greeting");
    ASSERT_TRUE(descriptor->config.schema.contains("greeting"));

    auto greeting_schema = descriptor->config.schema["greeting"].as<mc::dict>();
    EXPECT_EQ(greeting_schema["signature"].as<std::string>(), "s");
}

TEST(service_registry_test, import_global_services_keeps_registration_available)
{
    mc::app::service_registry registry;
    registry.import_global_services();

    ASSERT_TRUE(registry.has_service("global_registry_test_service"));

    auto service = registry.create_service("global_registry_test_service", "mc.test.global_demo");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "mc.test.global_demo");
}
