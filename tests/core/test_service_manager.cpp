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
#include <mc/core/service_manager.h>
#include <mc/core/config_schema.h>
#include <mc/core/service_factory.h>
#include <mc/core/supervisor.h>
#include <mc/core/supervisor_manager.h>
#include <mc/core/config_manager.h>
#include <mc/variant.h>
#include <mc/dict.h>

using namespace mc;

// 测试辅助函数：创建服务配置
config::service_config make_service_config(const std::string& name, const std::vector<std::string>& deps) {
    config::service_config cfg;
    cfg.api_version = "v1";
    cfg.kind = "Service";
    cfg.meta.name = name;
    cfg.meta.labels = dict{{"supervisor", "main"}};
    cfg.type = "test";
    cfg.dependencies = deps;
    return cfg;
}

// 测试用监督器
class test_supervisor : public supervisor {
public:
    test_supervisor() : m_config{} {
        m_config.meta.name = "main";
        m_config.strategy = config::supervisor_strategy::one_for_one;
        m_config.max_restarts = 3;
    }
    
    bool init(const config::supervisor_config& config) override {
        m_config = config;
        return true;
    }
    
    bool start() override { return true; }
    bool stop() override { return true; }
    void cleanup() override {}
    
    bool add_service(service_ptr service) override { return true; }
    bool remove_service(const std::string& name) override { return true; }
    service_ptr get_service(const std::string& name) const override { return nullptr; }
    
    bool add_child(supervisor_ptr child) override { return true; }
    bool remove_child(const std::string& name) override { return true; }
    supervisor_ptr get_child(const std::string& name) const override { return nullptr; }
    
    bool is_healthy() const override { return true; }
    const config::supervisor_config& get_config() const override { return m_config; }
    
    bool restart_all_services() override { return true; }
    bool restart_dependent_services(const std::string& service_name) override { return true; }
    
    std::string name() const override { return m_config.meta.name; }

private:
    config::supervisor_config m_config;
};

// 测试用服务
class test_service : public service {
public:
    test_service(const std::string& name) : m_name(name), m_supervisor(nullptr) {}
    
    bool init(dict args) override { return true; }
    bool start() override { return true; }
    bool stop() override { return true; }
    void cleanup() override {}
    
    service_state get_state() const override { return service_state::stopped; }
    const std::string& name() const override { return m_name; }
    bool is_healthy() const override { return true; }
    
    void set_supervisor(std::shared_ptr<supervisor> supervisor) override {
        m_supervisor = supervisor;
    }
    
    std::shared_ptr<supervisor> get_supervisor() const override {
        return m_supervisor;
    }
    
private:
    std::string m_name;
    std::shared_ptr<supervisor> m_supervisor;
};

// 测试用服务工厂
class test_service_factory : public service_factory {
public:
    service_ptr create_service(const std::string& service_name, std::string object_name,
                               mc::dict args) override {
        return std::make_shared<test_service>(object_name);
    }
};

class service_manager_test : public ::testing::Test {
protected:
    void SetUp() override {
        supervisor_mgr.add_supervisor("main", std::make_shared<test_supervisor>());
        factory = std::make_shared<test_service_factory>();
    }

    void add_configs(const std::vector<config::service_config>& configs) {
        for (const auto& config : configs) {
            variant v;
            to_variant(config, v);
            config_mgr.add_config(v);
        }
    }

    supervisor_manager supervisor_mgr;
    std::shared_ptr<service_factory> factory;
    service_manager manager;
    config_manager config_mgr;
};

// 测试无依赖的服务
TEST_F(service_manager_test, no_dependencies) {
    std::vector<config::service_config> configs = {
        make_service_config("service1", {}),
        make_service_config("service2", {}),
        make_service_config("service3", {})
    };
    
    add_configs(configs);
    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
    
    // 获取服务名称列表，顺序不重要
    auto names = manager.get_service_names();
    ASSERT_EQ(names.size(), 3);
    ASSERT_TRUE(std::find(names.begin(), names.end(), "service1") != names.end());
    ASSERT_TRUE(std::find(names.begin(), names.end(), "service2") != names.end());
    ASSERT_TRUE(std::find(names.begin(), names.end(), "service3") != names.end());
}

// 测试简单的依赖关系
TEST_F(service_manager_test, simple_dependency) {
    std::vector<config::service_config> configs = {
        make_service_config("service2", {"service1"}),  // service2 依赖 service1
        make_service_config("service1", {})  // service1 无依赖
    };
    
    add_configs(configs);
    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
    
    // 启动服务，验证启动顺序
    ASSERT_TRUE(manager.start_services());
    
    // 获取服务名称列表，service1 必须在 service2 之前
    auto names = manager.get_service_names();
    ASSERT_EQ(names.size(), 2);
    auto service1_pos = std::find(names.begin(), names.end(), "service1");
    auto service2_pos = std::find(names.begin(), names.end(), "service2");
    ASSERT_TRUE(service1_pos != names.end());
    ASSERT_TRUE(service2_pos != names.end());
    ASSERT_TRUE(service1_pos < service2_pos);
}

// 测试多重依赖关系
TEST_F(service_manager_test, multiple_dependencies) {
    std::vector<config::service_config> configs = {
        make_service_config("service3", {"service2"}),  // service3 依赖 service2
        make_service_config("service2", {"service1"}),  // service2 依赖 service1
        make_service_config("service1", {})  // service1 无依赖
    };
    
    add_configs(configs);
    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
    
    // 启动服务，验证启动顺序
    ASSERT_TRUE(manager.start_services());
    
    // 获取服务名称列表，必须按照依赖顺序排列
    auto names = manager.get_service_names();
    ASSERT_EQ(names.size(), 3);
    auto service1_pos = std::find(names.begin(), names.end(), "service1");
    auto service2_pos = std::find(names.begin(), names.end(), "service2");
    auto service3_pos = std::find(names.begin(), names.end(), "service3");
    ASSERT_TRUE(service1_pos != names.end());
    ASSERT_TRUE(service2_pos != names.end());
    ASSERT_TRUE(service3_pos != names.end());
    ASSERT_TRUE(service1_pos < service2_pos);
    ASSERT_TRUE(service2_pos < service3_pos);
}

// 测试多个独立的依赖链
TEST_F(service_manager_test, multiple_dependency_chains) {
    std::vector<config::service_config> configs = {
        make_service_config("service2", {"service1"}),  // 第一条链：service2 依赖 service1
        make_service_config("service1", {}),
        make_service_config("service4", {"service3"}),  // 第二条链：service4 依赖 service3
        make_service_config("service3", {})
    };
    
    add_configs(configs);
    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
    
    // 启动服务，验证启动顺序
    ASSERT_TRUE(manager.start_services());
    
    // 获取服务名称列表，验证每条链的依赖顺序
    auto names = manager.get_service_names();
    ASSERT_EQ(names.size(), 4);
    auto service1_pos = std::find(names.begin(), names.end(), "service1");
    auto service2_pos = std::find(names.begin(), names.end(), "service2");
    auto service3_pos = std::find(names.begin(), names.end(), "service3");
    auto service4_pos = std::find(names.begin(), names.end(), "service4");
    ASSERT_TRUE(service1_pos != names.end());
    ASSERT_TRUE(service2_pos != names.end());
    ASSERT_TRUE(service3_pos != names.end());
    ASSERT_TRUE(service4_pos != names.end());
    ASSERT_TRUE(service1_pos < service2_pos);
    ASSERT_TRUE(service3_pos < service4_pos);
}

// 测试循环依赖检测
TEST_F(service_manager_test, circular_dependency) {
    std::vector<config::service_config> configs = {
        make_service_config("service1", {"service2"}),  // service1 依赖 service2
        make_service_config("service2", {"service1"})   // service2 依赖 service1，形成循环
    };
    
    add_configs(configs);
    ASSERT_FALSE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
} 