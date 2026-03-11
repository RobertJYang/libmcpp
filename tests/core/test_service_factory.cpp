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

#include <algorithm>
#include <mc/core/service_factory.h>
#include <mc/filesystem.h>
#include <mc/memory.h>
#include <mc/variant.h>

namespace {

class basic_service : public mc::core::service_base {
public:
    explicit basic_service(std::string name)
        : mc::core::service_base(std::move(name))
    {
    }

    bool init(mc::dict args) override
    {
        m_args = std::move(args);
        set_state(mc::core::service_state::running);
        return true;
    }

    bool stop() override
    {
        set_state(mc::core::service_state::stopped);
        return true;
    }

    mc::dict m_args;
};

class options_service : public mc::core::service_base {
public:
    using mc::core::service_base::service_base;

    struct register_options {
        void operator()(boost::program_options::options_description& cli,
                        boost::program_options::options_description& cfg) const
        {
            cli.add_options()("sample", boost::program_options::value<int>()->default_value(42),
                              "sample option");
            auto tmp_dir = mc::filesystem::temp_directory_path();
            cfg.add_options()("path",
                              boost::program_options::value<std::string>()->default_value(tmp_dir.string()),
                              "config option");
        }
    };

    bool init(mc::dict) override
    {
        return true;
    }
};

class failing_service : public mc::core::service_base {
public:
    using mc::core::service_base::service_base;

    bool init(mc::dict args) override
    {
        if (args.contains("allow") && args["allow"].as_bool()) {
            return true;
        }
        return false;
    }
};

} // namespace

TEST(service_factory_test, register_and_create_service_success)
{
    mc::core::service_factory factory;
    factory.register_service<basic_service>("basic");

    EXPECT_TRUE(factory.has_service("basic"));

    mc::dict args;
    args["token"] = 123;
    auto service  = factory.create_service("basic", "instance-basic", args);
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "instance-basic");
    EXPECT_EQ(service->get_state(), mc::core::service_state::running);

    auto derived = mc::static_pointer_cast<basic_service>(service);
    ASSERT_TRUE(derived->m_args.contains("token"));
    EXPECT_EQ(derived->m_args["token"].as_int32(), 123);

    auto types = factory.get_service_types();
    ASSERT_EQ(types.size(), 1U);
    EXPECT_EQ(types[0], "basic");
}

TEST(service_factory_test, register_service_registers_options_when_available)
{
    mc::core::service_factory factory;
    factory.register_service<options_service>("options");

    ASSERT_TRUE(factory.has_service("options"));

    auto& opts = factory.get_service_options();
    ASSERT_NE(opts, nullptr);

    const auto& cli_options = opts->cli.options();
    auto        cli_found   = std::any_of(cli_options.begin(), cli_options.end(), [](const auto& opt) {
        return opt->long_name() == "sample";
    });
    EXPECT_TRUE(cli_found);

    const auto& cfg_options = opts->cfg.options();
    auto        cfg_found   = std::any_of(cfg_options.begin(), cfg_options.end(), [](const auto& opt) {
        return opt->long_name() == "path";
    });
    EXPECT_TRUE(cfg_found);
}

TEST(service_factory_test, create_service_handles_missing_or_failed_services)
{
    mc::core::service_factory factory;
    factory.register_service<failing_service>("failing");

    auto missing = factory.create_service("unknown", "name", {});
    EXPECT_EQ(missing, nullptr);

    mc::dict empty_args;
    auto     failed = factory.create_service("failing", "fail-instance", empty_args);
    EXPECT_EQ(failed, nullptr);

    mc::dict success_args;
    success_args["allow"] = true;
    auto created          = factory.create_service("failing", "ok-instance", success_args);
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->name(), "ok-instance");
}
