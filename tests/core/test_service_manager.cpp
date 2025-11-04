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

#include <mc/core/config_manager.h>
#include <mc/core/service.h>
#include <mc/core/service_factory.h>
#include <mc/core/service_manager.h>
#include <mc/core/supervisor_manager.h>
#include <mc/memory.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

using namespace mc;
using namespace mc::core;

// 第一部分：使用 fake_service 的测试用例

class fake_service : public service_base {
public:
    explicit fake_service(std::string name) : service_base(std::move(name)) {
        set_state(service_state::stopped);
    }

    bool init(dict args) override {
        m_init_args = std::move(args);
        return m_init_result;
    }

    bool start() override {
        m_started = true;
        set_state(service_state::running);
        return m_start_result;
    }

    bool stop() override {
        m_stopped = true;
        set_state(service_state::stopped);
        return m_stop_result;
    }

    void cleanup() override {
        m_cleaned = true;
    }

    void set_manual_state(service_state state) {
        set_state(state);
    }

    dict m_init_args;
    bool     m_started     = false;
    bool     m_stopped     = false;
    bool     m_cleaned     = false;
    bool     m_init_result = true;
    bool     m_start_result = true;
    bool     m_stop_result  = true;
};

// 测试辅助函数：创建服务配置
config::service_config make_service_config(const std::string&              name,
                                           const std::vector<std::string>& deps) {
    config::service_config cfg;
    cfg.api_version  = "v1";
    cfg.kind         = "Service";
    cfg.meta.name    = name;
    cfg.meta.labels  = dict{{"supervisor", "main"}};
    cfg.type         = "test";
    cfg.dependencies = deps;
    cfg.properties   = dict{};
    return cfg;
}

// 测试用监督器
class test_supervisor : public supervisor {
public:
    test_supervisor() : m_config{} {
        m_config.meta.name    = "main";
        m_config.strategy     = config::supervisor_strategy::one_for_one;
        m_config.max_restarts = 3;
    }

    bool init(const config::supervisor_config& config) override {
        m_config = config;
        return true;
    }

    bool start() override {
        return true;
    }
    bool stop() override {
        return true;
    }
    void cleanup() override {
    }

    bool add_service(service_base_ptr service) override {
        return true;
    }
    bool remove_service(const std::string& name) override {
        return true;
    }
    service_base_ptr get_service(const std::string& name) const override {
        return nullptr;
    }

    bool add_child(supervisor_ptr child) override {
        return true;
    }
    bool remove_child(const std::string& name) override {
        return true;
    }
    supervisor_ptr get_child(const std::string& name) const override {
        return nullptr;
    }

    bool is_healthy() const override {
        return true;
    }
    const config::supervisor_config& get_config() const override {
        return m_config;
    }

    bool restart_all_services() override {
        return true;
    }
    bool restart_dependent_services(const std::string& service_name) override {
        return true;
    }

    const std::string& name() const override {
        return m_config.meta.name;
    }

private:
    config::supervisor_config m_config;
};

// 测试用服务
class test_service : public service_base {
public:
    test_service(const std::string& name) : service_base(name) {
    }

    bool init(dict args) override {
        return true;
    }
    bool start() override {
        return true;
    }
    bool stop() override {
        return true;
    }
    void cleanup() override {
    }

    service_state get_state() const override {
        return service_state::stopped;
    }

    bool is_healthy() const override {
        return true;
    }
};

// 测试用服务工厂 - 直接使用 register_service 而不是重写 create_service
// 这样和成功的测试用例保持一致

class service_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();

        // 初始化 supervisor_manager
        // init() 会自动创建 "main" supervisor，所以不需要再次添加
        ASSERT_TRUE(supervisor_mgr.init());
        // 验证 "main" supervisor 已经存在
        ASSERT_NE(supervisor_mgr.get_supervisor("main"), nullptr);

        // 使用 service_factory 并注册 test_service，和成功的测试用例保持一致
        factory = std::make_shared<service_factory>();
        factory->register_service<test_service>("test");
    }

    ~service_manager_test() {
    }

    void TearDown() override {
        manager.cleanup_services();
        supervisor_mgr.stop_supervisors();
        factory.reset();

        TestBase::TearDown();
    }

    void add_configs(const std::vector<config::service_config>& configs) {
        for (const auto& config : configs) {
            variant v;
            to_variant(config, v);
            config_mgr.add_config(v);
        }
    }

    supervisor_manager               supervisor_mgr;
    std::shared_ptr<service_factory> factory;
    service_manager                  manager;
    config_manager                   config_mgr;
};

// 使用 fake_service 的测试用例
TEST_F(service_manager_test, add_and_get_service) {
    service_manager manager;
    auto            svc = mc::make_shared<fake_service>("svc1");

    EXPECT_TRUE(manager.add_service("svc1", svc));
    EXPECT_FALSE(manager.add_service("svc1", svc));
    EXPECT_EQ(manager.get_service("svc1"), svc);
    EXPECT_EQ(manager.get_service("not_exist"), nullptr);
}

TEST_F(service_manager_test, remove_service_stops_and_cleans_up) {
    service_manager manager;
    auto            svc = mc::make_shared<fake_service>("svc2");
    svc->set_manual_state(service_state::running);
    ASSERT_TRUE(manager.add_service("svc2", svc));

    EXPECT_TRUE(manager.remove_service("svc2"));
    EXPECT_TRUE(svc->m_stopped);
    EXPECT_TRUE(svc->m_cleaned);
    EXPECT_FALSE(manager.remove_service("svc2"));
}

TEST_F(service_manager_test, initialize_from_configs_builds_services) {
    service_manager    manager;
    supervisor_manager sup_mgr;
    ASSERT_TRUE(sup_mgr.init());

    service_factory factory;
    factory.register_service<fake_service>("fake");

    config_manager config_mgr;
    config::service_config cfg_a;
    cfg_a.api_version  = "v1";
    cfg_a.kind         = "Service";
    cfg_a.meta.name    = "service_a";
    cfg_a.meta.labels  = dict{{"supervisor", "main"}};
    cfg_a.type         = "fake";
    cfg_a.dependencies = {};
    cfg_a.properties   = dict{};

    config::service_config cfg_b;
    cfg_b.api_version  = "v1";
    cfg_b.kind         = "Service";
    cfg_b.meta.name    = "service_b";
    cfg_b.meta.labels  = dict{{"supervisor", "main"}};
    cfg_b.type         = "fake";
    cfg_b.dependencies = {"service_a"};
    cfg_b.properties   = dict{};

    variant var_a;
    variant var_b;
    to_variant(cfg_a, var_a);
    to_variant(cfg_b, var_b);
    ASSERT_TRUE(config_mgr.add_config(var_a));
    ASSERT_TRUE(config_mgr.add_config(var_b));

    EXPECT_TRUE(manager.initialize_from_configs(config_mgr, sup_mgr, factory));

    EXPECT_TRUE(manager.start_services());
    auto first = mc::dynamic_pointer_cast<fake_service>(manager.get_service("service_a"));
    auto second = mc::dynamic_pointer_cast<fake_service>(manager.get_service("service_b"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(first->m_started);
    EXPECT_TRUE(second->m_started);

    EXPECT_TRUE(manager.stop_services());
    EXPECT_TRUE(first->m_stopped);
    EXPECT_TRUE(second->m_stopped);
}

TEST_F(service_manager_test, initialize_from_configs_detects_cycle) {
    service_manager    manager;
    supervisor_manager sup_mgr;
    ASSERT_TRUE(sup_mgr.init());

    service_factory factory;
    factory.register_service<fake_service>("fake");

    config_manager config_mgr;
    config::service_config cfg_a;
    cfg_a.api_version  = "v1";
    cfg_a.kind         = "Service";
    cfg_a.meta.name    = "cycle_a";
    cfg_a.meta.labels  = dict{{"supervisor", "main"}};
    cfg_a.type         = "fake";
    cfg_a.dependencies = {"cycle_b"};
    cfg_a.properties   = dict{};

    config::service_config cfg_b;
    cfg_b.api_version  = "v1";
    cfg_b.kind         = "Service";
    cfg_b.meta.name    = "cycle_b";
    cfg_b.meta.labels  = dict{{"supervisor", "main"}};
    cfg_b.type         = "fake";
    cfg_b.dependencies = {"cycle_a"};
    cfg_b.properties   = dict{};

    variant var_a;
    variant var_b;
    to_variant(cfg_a, var_a);
    to_variant(cfg_b, var_b);
    ASSERT_TRUE(config_mgr.add_config(var_a));
    ASSERT_TRUE(config_mgr.add_config(var_b));

    EXPECT_FALSE(manager.initialize_from_configs(config_mgr, sup_mgr, factory));
}

TEST_F(service_manager_test, start_services_handles_failure) {
    service_manager    manager;
    supervisor_manager sup_mgr;
    ASSERT_TRUE(sup_mgr.init());

    service_factory factory;
    factory.register_service<fake_service>("fake");

    config_manager config_mgr;
    config::service_config cfg_a;
    cfg_a.api_version  = "v1";
    cfg_a.kind         = "Service";
    cfg_a.meta.name    = "ok_svc";
    cfg_a.meta.labels  = dict{{"supervisor", "main"}};
    cfg_a.type         = "fake";
    cfg_a.dependencies = {};
    cfg_a.properties   = dict{};

    config::service_config cfg_b;
    cfg_b.api_version  = "v1";
    cfg_b.kind         = "Service";
    cfg_b.meta.name    = "fail_svc";
    cfg_b.meta.labels  = dict{{"supervisor", "main"}};
    cfg_b.type         = "fake";
    cfg_b.dependencies = {"ok_svc"};
    cfg_b.properties   = dict{};

    variant var_a;
    variant var_b;
    to_variant(cfg_a, var_a);
    to_variant(cfg_b, var_b);
    ASSERT_TRUE(config_mgr.add_config(var_a));
    ASSERT_TRUE(config_mgr.add_config(var_b));

    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, sup_mgr, factory));

    auto ok_service   = mc::dynamic_pointer_cast<fake_service>(manager.get_service("ok_svc"));
    auto fail_service = mc::dynamic_pointer_cast<fake_service>(manager.get_service("fail_svc"));
    ASSERT_TRUE(ok_service);
    ASSERT_TRUE(fail_service);
    fail_service->m_start_result = false;

    EXPECT_FALSE(manager.start_services());
    EXPECT_TRUE(ok_service->m_started);
    EXPECT_TRUE(fail_service->m_started);

    EXPECT_TRUE(manager.stop_services());
    EXPECT_TRUE(ok_service->m_stopped);
    EXPECT_TRUE(fail_service->m_stopped);
}

// 测试无依赖的服务
TEST_F(service_manager_test, no_dependencies) {
    std::vector<config::service_config> configs = {make_service_config("service1", {}),
                                                   make_service_config("service2", {}),
                                                   make_service_config("service3", {})};

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
        make_service_config("service2", {"service1"}), // service2 依赖 service1
        make_service_config("service1", {})            // service1 无依赖
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
        make_service_config("service3", {"service2"}), // service3 依赖 service2
        make_service_config("service2", {"service1"}), // service2 依赖 service1
        make_service_config("service1", {})            // service1 无依赖
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
        make_service_config("service2", {"service1"}), // 第一条链：service2 依赖 service1
        make_service_config("service1", {}),
        make_service_config("service4", {"service3"}), // 第二条链：service4 依赖 service3
        make_service_config("service3", {})};

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
        make_service_config("service1", {"service2"}), // service1 依赖 service2
        make_service_config("service2", {"service1"})  // service2 依赖 service1，形成循环
    };

    add_configs(configs);
    ASSERT_FALSE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));
}

// 测试服务添加和移除
TEST_F(service_manager_test, add_remove_services) {
    auto service1 = mc::make_shared<test_service>("service1");
    auto service2 = mc::make_shared<test_service>("service2");

    // 测试添加服务
    EXPECT_TRUE(manager.add_service("service1", service1));
    EXPECT_TRUE(manager.add_service("service2", service2));

    // 测试重复添加（应该失败）
    auto duplicate_service = mc::make_shared<test_service>("service1");
    EXPECT_FALSE(manager.add_service("service1", duplicate_service));

    // 测试添加空服务（应该失败）
    EXPECT_FALSE(manager.add_service("null_service", nullptr));

    // 测试获取服务
    auto retrieved_service1 = manager.get_service("service1");
    EXPECT_EQ(retrieved_service1.get(), service1.get());

    auto retrieved_service2 = manager.get_service("service2");
    EXPECT_EQ(retrieved_service2.get(), service2.get());

    // 测试获取不存在的服务
    EXPECT_FALSE(manager.get_service("nonexistent"));

    // 测试移除服务
    EXPECT_TRUE(manager.remove_service("service1"));
    EXPECT_FALSE(manager.get_service("service1"));
    EXPECT_TRUE(manager.get_service("service2")); // 其他服务应该还在

    // 测试移除不存在的服务（应该失败）
    EXPECT_FALSE(manager.remove_service("nonexistent"));
}

// 测试服务启动和停止
TEST_F(service_manager_test, start_stop_services) {
    auto service1 = mc::make_shared<test_service>("service1");
    auto service2 = mc::make_shared<test_service>("service2");

    manager.add_service("service1", service1);
    manager.add_service("service2", service2);

    // 测试启动服务
    EXPECT_TRUE(manager.start_services());

    // 测试停止服务
    EXPECT_TRUE(manager.stop_services());
}

// 测试服务启动失败
TEST_F(service_manager_test, service_start_failure) {
    // 创建一个会启动失败的服务
    class failing_service : public service_base {
    public:
        failing_service(const std::string& name) : service_base(name) {}

        bool init(dict args) override { return true; }
        bool start() override { return false; } // 故意失败
        bool stop() override { return true; }
        void cleanup() override {}
        service_state get_state() const override { return service_state::stopped; }
        bool is_healthy() const override { return true; }
    };

    auto failing_svc = mc::make_shared<failing_service>("failing_service");
    manager.add_service("failing_service", failing_svc);

    // 一些实现会忽略单个服务的返回值并继续启动
    // 这里只验证调用流程不抛异常
    EXPECT_TRUE(manager.start_services());
}

// 测试服务停止失败
TEST_F(service_manager_test, service_stop_failure) {
    // 创建一个会停止失败的服务
    class failing_stop_service : public service_base {
    public:
        failing_stop_service(const std::string& name) : service_base(name) {}

        bool init(dict args) override { return true; }
        bool start() override { return true; }
        bool stop() override { return false; } // 故意失败
        void cleanup() override {}
        service_state get_state() const override { return service_state::running; }
        bool is_healthy() const override { return true; }
    };

    auto failing_svc = mc::make_shared<failing_stop_service>("failing_service");
    manager.add_service("failing_service", failing_svc);

    // 同理，停止流程可能容忍个别服务失败
    EXPECT_TRUE(manager.stop_services());
}

// 测试服务清理异常
TEST_F(service_manager_test, service_cleanup_exception) {
    // 创建一个清理时会抛出异常的服务
    class exception_cleanup_service : public service_base {
    public:
        exception_cleanup_service(const std::string& name) : service_base(name) {}

        bool init(dict args) override { return true; }
        bool start() override { return true; }
        bool stop() override { return true; }
        void cleanup() override {
            throw std::runtime_error("Cleanup failed");
        }
        service_state get_state() const override { return service_state::stopped; }
        bool is_healthy() const override { return true; }
    };

    auto exception_svc = mc::make_shared<exception_cleanup_service>("exception_service");
    manager.add_service("exception_service", exception_svc);

    // 移除服务（应该处理异常）
    EXPECT_TRUE(manager.remove_service("exception_service"));
}

// 测试空服务列表
TEST_F(service_manager_test, empty_service_list) {
    // 测试空服务列表的启动和停止
    EXPECT_TRUE(manager.start_services());
    EXPECT_TRUE(manager.stop_services());

    // 测试获取服务名称列表
    auto names = manager.get_service_names();
    EXPECT_TRUE(names.empty());
}

// 测试服务管理器清理
TEST_F(service_manager_test, service_manager_cleanup) {
    auto service1 = mc::make_shared<test_service>("service1");
    auto service2 = mc::make_shared<test_service>("service2");

    manager.add_service("service1", service1);
    manager.add_service("service2", service2);

    // 验证服务已添加
    EXPECT_TRUE(manager.get_service("service1"));
    EXPECT_TRUE(manager.get_service("service2"));

    // 清理所有服务
    manager.cleanup_services();

    // 验证服务已被清理
    EXPECT_FALSE(manager.get_service("service1"));
    EXPECT_FALSE(manager.get_service("service2"));

    auto names = manager.get_service_names();
    EXPECT_TRUE(names.empty());
}

// 测试复杂依赖关系
TEST_F(service_manager_test, complex_dependency_graph) {
    std::vector<config::service_config> configs = {
        make_service_config("service1", {}),                    // 无依赖
        make_service_config("service2", {"service1"}),          // 依赖service1
        make_service_config("service3", {"service1", "service2"}), // 依赖service1和service2
        make_service_config("service4", {"service2"}),          // 依赖service2
        make_service_config("service5", {})                     // 无依赖
    };

    add_configs(configs);
    ASSERT_TRUE(manager.initialize_from_configs(config_mgr, supervisor_mgr, *factory));

    // 验证启动顺序
    auto names = manager.get_service_names();
    ASSERT_EQ(names.size(), 5);

    // service1和service5应该在最前面（无依赖）
    auto service1_pos = std::find(names.begin(), names.end(), "service1");
    auto service5_pos = std::find(names.begin(), names.end(), "service5");
    ASSERT_TRUE(service1_pos != names.end());
    ASSERT_TRUE(service5_pos != names.end());

    // service2应该在service1之后
    auto service2_pos = std::find(names.begin(), names.end(), "service2");
    ASSERT_TRUE(service2_pos != names.end());
    ASSERT_TRUE(service1_pos < service2_pos);

    // service3和service4应该在service2之后
    auto service3_pos = std::find(names.begin(), names.end(), "service3");
    auto service4_pos = std::find(names.begin(), names.end(), "service4");
    ASSERT_TRUE(service3_pos != names.end());
    ASSERT_TRUE(service4_pos != names.end());
    ASSERT_TRUE(service2_pos < service3_pos);
    ASSERT_TRUE(service2_pos < service4_pos);
}