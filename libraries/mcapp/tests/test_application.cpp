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
#include <mc/app/application.h>
#include <mc/app/service.h>
#include <mc/array.h>
#include <mc/engine.h>
#include <mc/engine/service.h>
#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/reflect.h>
#include <test_utilities/base.h>

#include <fstream>
#include <memory>

namespace test_mcapp {

struct sample_service_config {
    MC_REFLECTABLE("mc.test.SampleServiceConfig")

    std::string greeting{"default-greeting"};
};

class sample_service : public mc::app::service {
public:
    using config_type = sample_service_config;

    explicit sample_service(std::string name) : mc::app::service(std::move(name))
    {}

    bool on_configure() override
    {
        mc::from_variant(properties(), m_config);
        return true;
    }

    bool on_start() override
    {
        ++m_start_count;
        return true;
    }

    bool on_stop() override
    {
        ++m_stop_count;
        return true;
    }

    sample_service_config m_config;
    int                   m_start_count{0};
    int                   m_stop_count{0};
};

} // namespace test_mcapp

MC_REFLECT(test_mcapp::sample_service_config, (greeting))

class application_test : public mc::test::TestWithRuntime {
protected:
    void SetUp() override
    {
        // Сбрасываем глобальный engine + SHM-регион, чтобы между кейсами не оставались
        // object_id / именованные мьютексы из прошлого прогона процесса (POSIX shm_open
        // персистентен между запусками — без сброса возникает «索引键冲突» либо
        // «mutex lock failed» при глобальной деструкции).
        mc::engine::engine::reset_for_test();
        mc::app::base_app::reset_for_test();
        m_app = std::make_unique<mc::app::application>();
        m_app->registry().register_service<test_mcapp::sample_service>("sample_service");
        m_temp_dir = create_test_directory();
    }

    void TearDown() override
    {
        if (m_app) {
            m_app->stop();
        }
        m_app.reset();
        mc::app::base_app::reset_for_test();
        mc::engine::engine::reset_for_test();
    }

    mc::filesystem::path write_config(const mc::string& content)
    {
        auto          path = m_temp_dir.path() / "mcapp_test_config.json";
        std::ofstream file(path.string());
        file << content;
        file.close();
        return path;
    }

    mc::filesystem::path write_file(mc::string_view relative_path, const mc::string& content)
    {
        auto path = m_temp_dir.path() / std::string(relative_path);
        mc::filesystem::create_directories(path.parent_path());
        std::ofstream file(path.string());
        file << content;
        file.close();
        return path;
    }

    std::unique_ptr<mc::app::application> m_app;
    mc::test::scoped_test_directory       m_temp_dir;
};

TEST_F(application_test, initialize_builds_service_plan_from_config)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"},
            "threads": 2,
            "work_threads": 4
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha"},
            "type": "sample_service",
            "enabled": true,
            "properties": {
                "greeting": "hello"
            }
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(m_app->initialize(options));

    const auto& plan = m_app->plan();
    ASSERT_EQ(plan.services.size(), 1U);
    EXPECT_EQ(plan.application.name, "sample-app");
    EXPECT_EQ(plan.application.io_threads, 2U);
    EXPECT_EQ(plan.application.work_threads, 4U);
    EXPECT_EQ(plan.services[0].name, "mc.test.alpha");
    EXPECT_EQ(plan.services[0].type, "sample_service");

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "hello");
}

TEST_F(application_test, command_line_overrides_service_properties)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha"},
            "type": "sample_service",
            "enabled": true,
            "properties": {
                "greeting": "hello"
            }
        }
    ])");

    std::string          config_arg   = config_path.string();
    std::string          override_arg = "--service.mc.test.alpha.greeting=\"world\"";
    char                 arg0[]       = "mcapp_test";
    char                 arg1[]       = "--config";
    char*                argv[]       = {arg0, arg1, config_arg.data(), override_arg.data(), nullptr};
    mc::app::app_options options{4, argv};

    ASSERT_TRUE(m_app->initialize(options));

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "world");
}

TEST_F(application_test, initialize_merges_manifest_imports_service_dirs_and_cli_with_precedence)
{
    write_file("imported.json", R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "threads": 1
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha"},
            "type": "sample_service",
            "enabled": true,
            "properties": {
                "greeting": "from-import"
            }
        }
    ])");
    write_file("services.d/20-alpha.json", R"({
        "api_version": "v1",
        "kind": "Service",
        "meta": {"name": "mc.test.alpha"},
        "enabled": true,
        "properties": {
            "greeting": "from-service-dir"
        }
    })");
    auto config_path = write_file("manifest.json", R"({
        "api_version": "v2",
        "kind": "ApplicationManifest",
        "meta": {"name": "manifest-app"},
        "threads": 2,
        "work_threads": 3,
        "imports": ["imported.json"],
        "service_dirs": ["services.d"],
        "services": [
            {
                "api_version": "v1",
                "kind": "Service",
                "meta": {"name": "mc.test.alpha"},
                "type": "sample_service",
                "enabled": true,
                "properties": {
                    "greeting": "from-manifest"
                }
            }
        ]
    })");

    std::string config_arg   = config_path.string();
    std::string override_arg = "--service.mc.test.alpha.greeting=\"from-cli\"";
    char        arg0[]       = "mcapp_test";
    char        arg1[]       = "--config";
    char        arg2[]       = "--threads";
    char*       argv[] = {arg0, arg1, config_arg.data(), arg2, const_cast<char*>("6"), override_arg.data(), nullptr};
    mc::app::app_options options{6, argv};

    ASSERT_TRUE(m_app->initialize(options));

    const auto& plan = m_app->plan();
    ASSERT_EQ(plan.services.size(), 1U);
    EXPECT_EQ(plan.application.name, "manifest-app");
    EXPECT_EQ(plan.application.io_threads, 6U);
    EXPECT_EQ(plan.application.work_threads, 3U);
    EXPECT_EQ(plan.application.modules.size(), 0U);

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "from-cli");
}

TEST_F(application_test, start_and_stop_manage_service_lifecycle)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha"},
            "type": "sample_service",
            "enabled": true,
            "properties": {
                "greeting": "hello"
            }
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(m_app->initialize(options));
    ASSERT_TRUE(m_app->start());

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->state(), mc::app::service_state::running);
    EXPECT_EQ(typed->m_start_count, 1);

    ASSERT_TRUE(m_app->stop());
    EXPECT_EQ(typed->state(), mc::app::service_state::stopped);
    EXPECT_EQ(typed->m_stop_count, 1);
}

TEST_F(application_test, initialize_applies_descriptor_default_properties_before_service_config)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha"},
            "type": "sample_service",
            "enabled": true
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(m_app->initialize(options));

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "default-greeting");
}

TEST_F(application_test, initialize_keeps_module_loading_in_bootstrap_flow)
{
    auto config_path = write_config(R"({
        "api_version": "v1",
        "kind": "Application",
        "meta": {"name": "sample-app"},
        "modules": ["mc.test.nonexistent.module"]
    })");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    EXPECT_FALSE(m_app->initialize(options));
}

TEST_F(application_test, initialize_creates_root_service_and_derives_service_path_from_name)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {"name": "mc.test.alpha.beta"},
            "type": "sample_service",
            "enabled": true
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(m_app->initialize(options));

    auto root = m_app->root_service();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->path(), "/");

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "mc.test.alpha.beta");
    EXPECT_EQ(service->path(), "/mc/test/alpha/beta");

    auto children = root->get_children();
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children[0].get(), service.get());
    ASSERT_NE(service->get_parent(), nullptr);
    EXPECT_EQ(service->get_parent().get(), root.get());
}

TEST_F(application_test, initialize_keeps_explicit_service_path)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {
                "name": "mc.test.alpha.beta",
                "path": "/services/custom"
            },
            "type": "sample_service",
            "enabled": true
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(m_app->initialize(options));

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->path(), "/services/custom");
}

TEST_F(application_test, initialize_allows_cli_override_for_dotted_service_name_without_breaking_path_rule)
{
    write_file("services.d/20-alpha.json", R"({
        "api_version": "v1",
        "kind": "Service",
        "meta": {"name": "mc.test.alpha.beta"},
        "type": "sample_service",
        "enabled": true,
        "properties": {
            "greeting": "from-service-dir"
        }
    })");

    auto config_path = write_file("manifest.json", R"({
        "api_version": "v1",
        "kind": "ApplicationManifest",
        "meta": {"name": "manifest-app"},
        "service_dirs": ["services.d"],
        "services": [
            {
                "api_version": "v1",
                "kind": "Service",
                "meta": {"name": "mc.test.alpha.beta"},
                "type": "sample_service",
                "enabled": true,
                "properties": {
                    "greeting": "from-manifest"
                }
            }
        ]
    })");

    std::string config_arg   = config_path.string();
    std::string override_arg = "--service.mc.test.alpha.beta.greeting=\"from-cli\"";
    char        arg0[]       = "mcapp_test";
    char        arg1[]       = "--config";
    char*       argv[]       = {arg0, arg1, config_arg.data(), override_arg.data(), nullptr};
    mc::app::app_options options{4, argv};

    ASSERT_TRUE(m_app->initialize(options));

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->path(), "/mc/test/alpha/beta");

    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "from-cli");
}

TEST_F(application_test, initialize_rejects_invalid_explicit_service_path)
{
    auto config_path = write_config(R"([
        {
            "api_version": "v1",
            "kind": "Application",
            "meta": {"name": "sample-app"}
        },
        {
            "api_version": "v1",
            "kind": "Service",
            "meta": {
                "name": "mc.test.alpha.beta",
                "path": "invalid/path"
            },
            "type": "sample_service",
            "enabled": true
        }
    ])");

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    EXPECT_FALSE(m_app->initialize(options));
}

