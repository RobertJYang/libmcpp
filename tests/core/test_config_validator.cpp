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
#include <mc/core/config_schema.h>
#include <test_utilities/test_base.h>

using namespace mc;
using namespace mc::config;

class config_validator_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        TestBase::SetUp();
    }

    void TearDown() override
    {
        TestBase::TearDown();
    }
};

// 测试应用程序配置验证 - 成功情况
TEST_F(config_validator_test, validate_app_config_success)
{
    app_config config;
    config.kind         = "Application";
    auto tmp_dir        = mc::filesystem::temp_directory_path();
    auto plugin_dir     = (tmp_dir / "plugins").string();
    config.api_version  = "v1";
    config.meta.name    = "test_app";
    config.plugin_dir   = plugin_dir;
    config.plugins      = {"plugin1", "plugin2"};
    config.threads      = 4;
    config.work_threads = 8;

    EXPECT_TRUE(config_validator::validate_app_config(config));
}

// 测试应用程序配置验证 - 缺少kind
TEST_F(config_validator_test, validate_app_config_missing_kind)
{
    app_config config;
    config.kind        = "";
    config.api_version = "v1";
    config.meta.name   = "test_app";

    EXPECT_FALSE(config_validator::validate_app_config(config));
}

// 测试应用程序配置验证 - 缺少api_version
TEST_F(config_validator_test, validate_app_config_missing_api_version)
{
    app_config config;
    config.kind        = "Application";
    config.api_version = "";
    config.meta.name   = "test_app";

    EXPECT_FALSE(config_validator::validate_app_config(config));
}

// 测试应用程序配置验证 - 缺少name
TEST_F(config_validator_test, validate_app_config_missing_name)
{
    app_config config;
    config.kind        = "Application";
    config.api_version = "v1";
    config.meta.name   = "";

    EXPECT_FALSE(config_validator::validate_app_config(config));
}

// 测试应用程序配置验证 - 缺少plugin_dir（警告）
TEST_F(config_validator_test, validate_app_config_missing_plugin_dir)
{
    app_config config;
    config.kind        = "Application";
    config.api_version = "v1";
    config.meta.name   = "test_app";
    config.plugin_dir  = "";

    EXPECT_TRUE(config_validator::validate_app_config(config));
}

// 测试监督器配置验证 - 成功情况
TEST_F(config_validator_test, validate_supervisor_config_success)
{
    supervisor_config config;
    config.kind         = "Supervisor";
    config.api_version  = "v1";
    config.meta.name    = "test_supervisor";
    config.strategy     = supervisor_strategy::one_for_one;
    config.max_restarts = 3;
    config.services     = {"service1", "service2"};

    EXPECT_TRUE(config_validator::validate_supervisor_config(config));
}

// 测试监督器配置验证 - 缺少kind
TEST_F(config_validator_test, validate_supervisor_config_missing_kind)
{
    supervisor_config config;
    config.kind        = "";
    config.api_version = "v1";
    config.meta.name   = "test_supervisor";

    EXPECT_FALSE(config_validator::validate_supervisor_config(config));
}

// 测试监督器配置验证 - 缺少api_version
TEST_F(config_validator_test, validate_supervisor_config_missing_api_version)
{
    supervisor_config config;
    config.kind        = "Supervisor";
    config.api_version = "";
    config.meta.name   = "test_supervisor";

    EXPECT_FALSE(config_validator::validate_supervisor_config(config));
}

// 测试监督器配置验证 - 缺少name
TEST_F(config_validator_test, validate_supervisor_config_missing_name)
{
    supervisor_config config;
    config.kind        = "Supervisor";
    config.api_version = "v1";
    config.meta.name   = "";

    EXPECT_FALSE(config_validator::validate_supervisor_config(config));
}

// 测试监督器配置验证 - max_restarts为负数
TEST_F(config_validator_test, validate_supervisor_config_negative_max_restarts)
{
    supervisor_config config;
    config.kind         = "Supervisor";
    config.api_version  = "v1";
    config.meta.name    = "test_supervisor";
    config.max_restarts = -1;

    EXPECT_FALSE(config_validator::validate_supervisor_config(config));
}

// 测试服务配置验证 - 成功情况
TEST_F(config_validator_test, validate_service_config_success)
{
    service_config config;
    config.kind         = "Service";
    config.api_version  = "v1";
    config.meta.name    = "test_service";
    config.type         = "test_type";
    config.dependencies = {"dep1", "dep2"};
    config.properties   = dict{{"key1", "value1"}};

    EXPECT_TRUE(config_validator::validate_service_config(config));
}

// 测试服务配置验证 - 缺少kind
TEST_F(config_validator_test, validate_service_config_missing_kind)
{
    service_config config;
    config.kind        = "";
    config.api_version = "v1";
    config.meta.name   = "test_service";
    config.type        = "test_type";

    EXPECT_FALSE(config_validator::validate_service_config(config));
}

// 测试服务配置验证 - 缺少api_version
TEST_F(config_validator_test, validate_service_config_missing_api_version)
{
    service_config config;
    config.kind        = "Service";
    config.api_version = "";
    config.meta.name   = "test_service";
    config.type        = "test_type";

    EXPECT_FALSE(config_validator::validate_service_config(config));
}

// 测试服务配置验证 - 缺少name
TEST_F(config_validator_test, validate_service_config_missing_name)
{
    service_config config;
    config.kind        = "Service";
    config.api_version = "v1";
    config.meta.name   = "";
    config.type        = "test_type";

    EXPECT_FALSE(config_validator::validate_service_config(config));
}

// 测试服务配置验证 - 缺少type
TEST_F(config_validator_test, validate_service_config_missing_type)
{
    service_config config;
    config.kind        = "Service";
    config.api_version = "v1";
    config.meta.name   = "test_service";
    config.type        = "";

    EXPECT_FALSE(config_validator::validate_service_config(config));
}

// 测试插件配置验证 - 成功情况
TEST_F(config_validator_test, validate_plugin_config_success)
{
    plugin_config config;
    config.kind        = "Plugin";
    config.api_version = "v1";
    config.meta.name   = "test_plugin";
    config.version     = "1.0.0";
    config.properties  = dict{{"key1", "value1"}};

    EXPECT_TRUE(config_validator::validate_plugin_config(config));
}

// 测试插件配置验证 - 缺少kind
TEST_F(config_validator_test, validate_plugin_config_missing_kind)
{
    plugin_config config;
    config.kind        = "";
    config.api_version = "v1";
    config.meta.name   = "test_plugin";

    EXPECT_FALSE(config_validator::validate_plugin_config(config));
}

// 测试插件配置验证 - 缺少api_version
TEST_F(config_validator_test, validate_plugin_config_missing_api_version)
{
    plugin_config config;
    config.kind        = "Plugin";
    config.api_version = "";
    config.meta.name   = "test_plugin";

    EXPECT_FALSE(config_validator::validate_plugin_config(config));
}

// 测试插件配置验证 - 缺少name
TEST_F(config_validator_test, validate_plugin_config_missing_name)
{
    plugin_config config;
    config.kind        = "Plugin";
    config.api_version = "v1";
    config.meta.name   = "";

    EXPECT_FALSE(config_validator::validate_plugin_config(config));
}

// 测试插件配置验证 - 缺少version（警告）
TEST_F(config_validator_test, validate_plugin_config_missing_version)
{
    plugin_config config;
    config.kind        = "Plugin";
    config.api_version = "v1";
    config.meta.name   = "test_plugin";
    config.version     = "";

    EXPECT_TRUE(config_validator::validate_plugin_config(config));
}
