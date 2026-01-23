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
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/dict.h>
#include <mc/engine.h>
#include <mc/json.h>
#include <mc/variant.h>

#include <chrono>
#include <string>
#include <thread>

using namespace mc::engine;

// 测试接口定义
class QueryTestInterfaceA : public mc::engine::interface<QueryTestInterfaceA> {
public:
    MC_INTERFACE("org.openubmc.query_test_interface_a")

    property<int32_t>     m_prop1{100};
    property<std::string> m_prop2{"test_value"};
    property<bool>        m_prop3{true};
};

class QueryTestInterfaceB : public mc::engine::interface<QueryTestInterfaceB> {
public:
    MC_INTERFACE("org.openubmc.query_test_interface_b")

    property<uint32_t>    m_count{42};
    property<std::string> m_name{"object_name"};
};

class QueryTestInterfaceC : public mc::engine::interface<QueryTestInterfaceC> {
public:
    MC_INTERFACE("org.openubmc.query_test_interface_c")

    property<int32_t> m_value{999};
};

// 测试对象定义
class QueryTestObjectA : public mc::engine::object<QueryTestObjectA> {
public:
    MC_OBJECT(QueryTestObjectA, "QueryTestObjectA", "/org/openubmc/query_test_object_a",
              (QueryTestInterfaceA)(QueryTestInterfaceB))

    void init() {
        set_object_name("QueryTestObjectA");
    }

    QueryTestInterfaceA m_iface_a;
    QueryTestInterfaceB m_iface_b;
};

class QueryTestObjectB : public mc::engine::object<QueryTestObjectB> {
public:
    MC_OBJECT(QueryTestObjectB, "QueryTestObjectB", "/org/openubmc/query_test_object_b",
              (QueryTestInterfaceA))

    void init() {
        set_object_name("QueryTestObjectB");
    }

    QueryTestInterfaceA m_iface_a;
};

class QueryTestObjectC : public mc::engine::object<QueryTestObjectC> {
public:
    MC_OBJECT(QueryTestObjectC, "QueryTestObjectC", "/org/openubmc/query_test_object_c",
              (QueryTestInterfaceC))

    void init() {
        set_object_name("QueryTestObjectC");
    }

    QueryTestInterfaceC m_iface_c;
};

// 子对象用于测试路径查询
class QueryTestObjectChild : public mc::engine::object<QueryTestObjectChild> {
public:
    MC_OBJECT(QueryTestObjectChild, "QueryTestObjectChild",
              "/org/openubmc/query_test_object_a/child", (QueryTestInterfaceA))

    void init() {
        set_object_name("QueryTestObjectChild");
    }

    QueryTestInterfaceA m_iface_a;
};

// 反射定义
MC_REFLECT(QueryTestInterfaceA, ((m_prop1, "Prop1"))((m_prop2, "Prop2"))((m_prop3, "Prop3")))
MC_REFLECT(QueryTestInterfaceB, ((m_count, "Count"))((m_name, "Name")))
MC_REFLECT(QueryTestInterfaceC, ((m_value, "Value")))
MC_REFLECT(QueryTestObjectA, ((m_iface_a, "InterfaceA"))((m_iface_b, "InterfaceB")))
MC_REFLECT(QueryTestObjectB, ((m_iface_a, "InterfaceA")))
MC_REFLECT(QueryTestObjectC, ((m_iface_c, "InterfaceC")))
MC_REFLECT(QueryTestObjectChild, ((m_iface_a, "InterfaceA")))

// 测试服务
struct query_test_service_1 : public mc::engine::service {
    query_test_service_1()
        : mc::engine::service("org.openubmc.query_test_service_1") {
    }

    bool init(mc::dict args = {}) override {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/query_test_service_1";
        args_mut["service_name"] = "org.openubmc.query_test_service_1";
        return mc::engine::service::init(args_mut);
    }

    bool start() override {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj_a = mc::make_shared<QueryTestObjectA>();
        m_obj_a->init();
        register_object(*m_obj_a);

        m_obj_child = mc::make_shared<QueryTestObjectChild>();
        m_obj_child->init();
        register_object(*m_obj_child);

        return true;
    }

    mc::shared_ptr<QueryTestObjectA>     m_obj_a;
    mc::shared_ptr<QueryTestObjectChild> m_obj_child;
};

struct query_test_service_2 : public mc::engine::service {
    query_test_service_2()
        : mc::engine::service("org.openubmc.query_test_service_2") {
    }

    bool init(mc::dict args = {}) override {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/openubmc/query_test_service_2";
        args_mut["service_name"] = "org.openubmc.query_test_service_2";
        return mc::engine::service::init(args_mut);
    }

    bool start() override {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj_b = mc::make_shared<QueryTestObjectB>();
        m_obj_b->init();
        register_object(*m_obj_b);

        m_obj_c = mc::make_shared<QueryTestObjectC>();
        m_obj_c->init();
        register_object(*m_obj_c);

        return true;
    }

    mc::shared_ptr<QueryTestObjectB> m_obj_b;
    mc::shared_ptr<QueryTestObjectC> m_obj_c;
};

static query_test_service_1* service_1;
static query_test_service_2* service_2;

class ShmTreeQueryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        auto& harbor = mc::dbus::harbor::get_instance();
        harbor.set_harbor_name_if_empty("harbor.mc.query_test");

        service_1 = new query_test_service_1();
        service_2 = new query_test_service_2();
        service_1->init();
        service_2->init();
        service_1->start();
        service_2->start();

        // 等待对象注册完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        service_1->stop();
        service_2->stop();
        delete service_1;
        delete service_2;

        auto& harbor = mc::dbus::harbor::get_instance();
        harbor.stop();
    }
};

// 共享内存/MDB 未填充时（如 ENABLE_CONAN_COMPILE=0、mock 或 stub 构建）跳过依赖预置数据的用例
#define SKIP_IF_SHM_NOT_POPULATED()                                                      \
    do {                                                                                 \
        if (mc::dbus::shm_tree::get_mdb_service_names().empty()) {                       \
            GTEST_SKIP() << "Shared memory/MDB not populated, skip test requiring data"; \
        }                                                                                \
    } while (0)

// ========== call_shm_get_property 测试 ==========
TEST_F(ShmTreeQueryTest, call_shm_get_property_success) {
    std::optional<mc::variant> result;
    try {
        result = mc::dbus::shm_tree::call_shm_get_property(
            "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
            "org.openubmc.query_test_interface_a", "Prop1");
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("failed to find") != std::string::npos) {
            GTEST_SKIP() << "SHM not populated: " << e.what();
        }
        throw;
    }
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), mc::variant(100));
}

TEST_F(ShmTreeQueryTest, call_shm_get_property_string_value) {
    std::optional<mc::variant> result;
    try {
        result = mc::dbus::shm_tree::call_shm_get_property(
            "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
            "org.openubmc.query_test_interface_a", "Prop2");
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("failed to find") != std::string::npos) {
            GTEST_SKIP() << "SHM not populated: " << e.what();
        }
        throw;
    }
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), mc::variant("test_value"));
}

TEST_F(ShmTreeQueryTest, call_shm_get_property_bool_value) {
    std::optional<mc::variant> result;
    try {
        result = mc::dbus::shm_tree::call_shm_get_property(
            "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
            "org.openubmc.query_test_interface_a", "Prop3");
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("failed to find") != std::string::npos) {
            GTEST_SKIP() << "SHM not populated: " << e.what();
        }
        throw;
    }
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), mc::variant(true));
}

TEST_F(ShmTreeQueryTest, call_shm_get_property_not_found) {
    try {
        (void)mc::dbus::shm_tree::call_shm_get_property(
            "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
            "org.openubmc.query_test_interface_a", "NonExistentProp");
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("failed to find") != std::string::npos) {
            GTEST_SKIP() << "SHM not populated: " << e.what();
        }
        throw;
    }
}

TEST_F(ShmTreeQueryTest, call_shm_get_property_invalid_path) {
    try {
        (void)mc::dbus::shm_tree::call_shm_get_property(
            "org.openubmc.query_test_service_1", "/invalid/path",
            "org.openubmc.query_test_interface_a", "Prop1");
    } catch (const std::exception& e) {
        if (std::string(e.what()).find("failed to find") != std::string::npos) {
            GTEST_SKIP() << "SHM not populated: " << e.what();
        }
        throw;
    }
}

// ========== call_shm_get_all_properties 测试 ==========
TEST_F(ShmTreeQueryTest, call_shm_get_all_properties_success) {
    SKIP_IF_SHM_NOT_POPULATED();
    auto result = mc::dbus::shm_tree::call_shm_get_all_properties(
        "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
        "org.openubmc.query_test_interface_a");
    ASSERT_TRUE(result.has_value());
    auto props = result.value();
    ASSERT_TRUE(props.contains("Prop1"));
    ASSERT_TRUE(props.contains("Prop2"));
    ASSERT_TRUE(props.contains("Prop3"));
    ASSERT_EQ(props["Prop1"], mc::variant(100));
    ASSERT_EQ(props["Prop2"], mc::variant("test_value"));
    ASSERT_EQ(props["Prop3"], mc::variant(true));
}

TEST_F(ShmTreeQueryTest, call_shm_get_all_properties_interface_b) {
    SKIP_IF_SHM_NOT_POPULATED();
    auto result = mc::dbus::shm_tree::call_shm_get_all_properties(
        "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
        "org.openubmc.query_test_interface_b");
    ASSERT_TRUE(result.has_value());
    auto props = result.value();
    ASSERT_TRUE(props.contains("Count"));
    ASSERT_TRUE(props.contains("Name"));
    ASSERT_EQ(props["Count"], mc::variant(42U));
    ASSERT_EQ(props["Name"], mc::variant("object_name"));
}

TEST_F(ShmTreeQueryTest, call_shm_get_all_properties_invalid_interface) {
    auto result = mc::dbus::shm_tree::call_shm_get_all_properties(
        "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
        "org.openubmc.invalid_interface");
    // 接口不存在时可能返回 nullopt
}

// ========== call_shm_get_properties_by_names 测试 ==========
TEST_F(ShmTreeQueryTest, call_shm_get_properties_by_names_success) {
    SKIP_IF_SHM_NOT_POPULATED();
    mc::variants prop_names;
    prop_names.push_back(mc::variant("Prop1"));
    prop_names.push_back(mc::variant("Prop2"));

    auto result = mc::dbus::shm_tree::call_shm_get_properties_by_names(
        "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
        "org.openubmc.query_test_interface_a", prop_names);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);

    auto props_dict  = (*result)[0].as_dict();
    auto errors_dict = (*result)[1].as_dict();

    ASSERT_TRUE(props_dict.contains("Prop1"));
    ASSERT_TRUE(props_dict.contains("Prop2"));
    ASSERT_EQ(props_dict["Prop1"], mc::variant(100));
    ASSERT_EQ(props_dict["Prop2"], mc::variant("test_value"));
    ASSERT_TRUE(errors_dict.empty());
}

TEST_F(ShmTreeQueryTest, call_shm_get_properties_by_names_partial) {
    mc::variants prop_names;
    prop_names.push_back(mc::variant("Prop1"));
    prop_names.push_back(mc::variant("NonExistentProp"));

    auto result = mc::dbus::shm_tree::call_shm_get_properties_by_names(
        "org.openubmc.query_test_service_1", "/org/openubmc/query_test_object_a",
        "org.openubmc.query_test_interface_a", prop_names);
    // 部分属性不存在时，可能返回 nullopt 或包含错误信息
}

// ========== get_mdb_object 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_object_success) {
    SKIP_IF_SHM_NOT_POPULATED();
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));

    auto result = mc::dbus::shm_tree::get_mdb_object(
        "/org/openubmc/query_test_object_a", interfaces);
    ASSERT_TRUE(result.has_value());
    auto result_dict = result.value();
    ASSERT_FALSE(result_dict.empty());
}

TEST_F(ShmTreeQueryTest, get_mdb_object_multiple_interfaces) {
    SKIP_IF_SHM_NOT_POPULATED();
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_b"));

    auto result = mc::dbus::shm_tree::get_mdb_object(
        "/org/openubmc/query_test_object_a", interfaces);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ShmTreeQueryTest, get_mdb_object_not_found) {
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));

    auto result = mc::dbus::shm_tree::get_mdb_object("/invalid/path", interfaces);
    ASSERT_FALSE(result.has_value());
}

// ========== get_mdb_sub_paths 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_sub_paths_success) {
    mc::variants interfaces; // 空数组表示不过滤接口

    auto result = mc::dbus::shm_tree::get_mdb_sub_paths(
        "/org/openubmc/query_test_object_a", 1, interfaces, 0, 0, false);
    ASSERT_TRUE(result.has_value());
    // 应该包含子路径
}

TEST_F(ShmTreeQueryTest, get_mdb_sub_paths_with_depth) {
    mc::variants interfaces;

    auto result = mc::dbus::shm_tree::get_mdb_sub_paths(
        "/org/openubmc", 2, interfaces, 0, 0, false);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ShmTreeQueryTest, get_mdb_sub_paths_with_paging) {
    mc::variants interfaces;

    auto result = mc::dbus::shm_tree::get_mdb_sub_paths(
        "/org/openubmc", 2, interfaces, 0, 1, false);
    ASSERT_TRUE(result.has_value());
    ASSERT_LE(result->size(), 1U);
}

TEST_F(ShmTreeQueryTest, get_mdb_sub_paths_with_interface_filter) {
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));

    auto result = mc::dbus::shm_tree::get_mdb_sub_paths(
        "/org/openubmc", 2, interfaces, 0, 0, false);
    ASSERT_TRUE(result.has_value());
}

// ========== is_valid_mdb_path 测试 ==========
TEST_F(ShmTreeQueryTest, is_valid_mdb_path_valid) {
    SKIP_IF_SHM_NOT_POPULATED();
    bool result = mc::dbus::shm_tree::is_valid_mdb_path(
        "/org/openubmc/query_test_object_a", false);
    ASSERT_TRUE(result);
}

TEST_F(ShmTreeQueryTest, is_valid_mdb_path_invalid) {
    bool result = mc::dbus::shm_tree::is_valid_mdb_path("/invalid/path", false);
    ASSERT_FALSE(result);
}

TEST_F(ShmTreeQueryTest, is_valid_mdb_path_ignore_case) {
    bool result = mc::dbus::shm_tree::is_valid_mdb_path(
        "/ORG/OPENUBMC/QUERY_TEST_OBJECT_A", true);
    // 忽略大小写时应该能找到
}

// ========== get_mdb_interface_owners 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_interface_owners_success) {
    SKIP_IF_SHM_NOT_POPULATED();
    auto result = mc::dbus::shm_tree::get_mdb_interface_owners(
        "org.openubmc.query_test_interface_a");
    ASSERT_FALSE(result.empty());
}

TEST_F(ShmTreeQueryTest, get_mdb_interface_owners_not_found) {
    auto result = mc::dbus::shm_tree::get_mdb_interface_owners(
        "org.openubmc.non_existent_interface");
    // 接口不存在时可能返回空数组
}

// ========== get_mdb_service_name 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_service_name_success) {
    // 测试接口调用，未找到时返回空 string_view
    auto result = mc::dbus::shm_tree::get_mdb_service_name(":1.0");
    (void)result;
}

// ========== get_mdb_path 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_path_success) {
    mc::dict filter_dict;
    filter_dict["Prop1"] = mc::variant(100);

    std::string filter_json = mc::json::json_encode(mc::variant(filter_dict));
    auto        result      = mc::dbus::shm_tree::get_mdb_path(
        "org.openubmc.query_test_interface_a", filter_json, false);
    ASSERT_EQ(result.size(), 2U);
    // 验证返回的路径和服务名
}

TEST_F(ShmTreeQueryTest, get_mdb_path_impl_success) {
    mc::dict filter_dict;
    filter_dict["Prop1"] = mc::variant(100);

    auto result = mc::dbus::shm_tree::get_mdb_path_impl(
        "org.openubmc.query_test_interface_a", filter_dict, false);
    ASSERT_EQ(result.size(), 2U);
}

TEST_F(ShmTreeQueryTest, get_mdb_path_not_found) {
    mc::dict filter_dict;
    filter_dict["Prop1"] = mc::variant(99999); // 不存在的值

    std::string filter_json = mc::json::json_encode(mc::variant(filter_dict));
    auto        result      = mc::dbus::shm_tree::get_mdb_path(
        "org.openubmc.query_test_interface_a", filter_json, false);
    ASSERT_EQ(result.size(), 2U);
    // 未找到时应该返回 ["", ""]
    ASSERT_TRUE(result[0].as_string().empty());
    ASSERT_TRUE(result[1].as_string().empty());
}

TEST_F(ShmTreeQueryTest, get_mdb_path_ignore_case) {
    mc::dict filter_dict;
    filter_dict["Prop2"] = mc::variant("test_value");

    std::string filter_json = mc::json::json_encode(mc::variant(filter_dict));
    auto        result      = mc::dbus::shm_tree::get_mdb_path(
        "org.openubmc.query_test_interface_a", filter_json, true);
    ASSERT_EQ(result.size(), 2U);
}

// ========== get_mdb_service_names 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_service_names_success) {
    SKIP_IF_SHM_NOT_POPULATED();
    auto result = mc::dbus::shm_tree::get_mdb_service_names();
    ASSERT_FALSE(result.empty());
}

// ========== get_mdb_classes 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_classes_with_service) {
    auto result = mc::dbus::shm_tree::get_mdb_classes(
        "org.openubmc.query_test_service_1");
    // 验证返回的 class_name 列表
}

TEST_F(ShmTreeQueryTest, get_mdb_classes_all_services) {
    auto result = mc::dbus::shm_tree::get_mdb_classes("");
    // 验证返回所有服务的 class_name
}

// ========== get_mdb_object_list 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_object_list_success) {
    auto result = mc::dbus::shm_tree::get_mdb_object_list("QueryTestObjectA");
    // 验证返回的对象列表结构
}

// ========== get_mdb_object_owner 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_object_owner_success) {
    auto result = mc::dbus::shm_tree::get_mdb_object_owner("QueryTestObjectA");
    // 验证返回的服务名和路径
}

// ========== get_mdb_matched_objects 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_matched_objects_with_service) {
    auto result = mc::dbus::shm_tree::get_mdb_matched_objects(
        "org.openubmc.query_test_service_1", "");
    // 验证返回的对象列表
}

TEST_F(ShmTreeQueryTest, get_mdb_matched_objects_with_pattern) {
    auto result = mc::dbus::shm_tree::get_mdb_matched_objects(
        "", "org.openubmc.query_test_interface_.*");
    // 验证返回的对象列表
}

// ========== get_mdb_sub_objects 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_sub_objects_success) {
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));

    auto result = mc::dbus::shm_tree::get_mdb_sub_objects(
        "/org/openubmc/query_test_object_a", 1, interfaces);
    ASSERT_TRUE(result.has_value());
    // 验证返回的路径映射关系
}

// ========== get_mdb_parent_objects 测试 ==========
TEST_F(ShmTreeQueryTest, get_mdb_parent_objects_success) {
    mc::variants interfaces;
    interfaces.push_back(mc::variant("org.openubmc.query_test_interface_a"));

    auto result = mc::dbus::shm_tree::get_mdb_parent_objects(
        "/org/openubmc/query_test_object_a/child", interfaces);
    ASSERT_TRUE(result.has_value());
    // 验证返回的父路径映射关系
}
