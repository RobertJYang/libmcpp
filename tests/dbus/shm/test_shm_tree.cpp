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

#include <mc/dbus/harbor.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/dbus/shm/local_msg.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/variant.h>

#include <future>
#include <memory>
#include <optional>
#include <thread>

namespace {

class dummy_service : public mc::engine::service {
public:
    dummy_service() : mc::engine::service("org.openubmc.test_shm_service") {
    }
};

class dummy_object : public mc::engine::object {
public:
    dummy_object(mc::engine::service& svc, std::string path)
        : mc::engine::object(svc, std::move(path)) {
    }
};

class shm_tree_test : public ::testing::Test {
protected:
    void SetUp() override {
        auto& harbor = mc::dbus::harbor::get_instance();
        harbor.set_harbor_name_if_empty("harbor.mc.test");

        m_unique_name = ":unique.test";
        m_service     = std::make_unique<dummy_service>();
        m_object      = std::make_unique<dummy_object>(*m_service, "/object/test/path");
        m_tree        = std::make_unique<mc::dbus::shm_tree>(harbor.get_harbor_name(),
                                                            m_service->name(), m_unique_name);
        harbor.register_unique_name(m_unique_name, m_service->name());
    }

    void TearDown() override {
        auto service_name = m_service == nullptr ? std::string() : m_service->name();
        m_tree.reset();
        m_object.reset();
        m_service.reset();
        auto& harbor = mc::dbus::harbor::get_instance();
        if (!service_name.empty()) {
            harbor.unregister_service(service_name);
        }
        harbor.stop();
    }

    std::unique_ptr<dummy_service>               m_service;
    std::unique_ptr<dummy_object>                m_object;
    std::unique_ptr<mc::dbus::shm_tree>          m_tree;
    std::string                                  m_unique_name;
};

TEST_F(shm_tree_test, register_and_unregister_object) {
    ASSERT_NO_THROW(m_tree->register_object(*m_object));
    ASSERT_NO_THROW(m_tree->unregister_object(m_object->get_object_path()));
}

TEST_F(shm_tree_test, get_property_missing_tree_throws) {
    EXPECT_THROW(
        mc::dbus::shm_tree::get_property("invalid.service", "/invalid", "iface", "prop"),
        mc::exception);
}

TEST_F(shm_tree_test, timeout_call_missing_queue_returns_nullopt) {
    mc::variants args;
    auto         result = m_tree->timeout_call(mc::milliseconds(5), "invalid.service", "/path",
                                               "iface", "Method", "", args);
    EXPECT_EQ(result, std::nullopt);
}

TEST_F(shm_tree_test, add_and_remove_match) {
    mc::dbus::match_rule rule = mc::dbus::match_rule::new_signal("Signal", "iface");
    bool                 invoked = false;
    auto                 cb      = [&](const mc::dbus::local_msg&) { invoked = true; };

    EXPECT_NO_THROW(m_tree->add_match(rule, cb, 1));
    EXPECT_NO_THROW(m_tree->remove_match(1));
    EXPECT_FALSE(invoked);
}

TEST_F(shm_tree_test, remove_match_unknown_id_safe) {
    // 不存在的匹配项，删除应当直接返回
    EXPECT_NO_THROW(m_tree->remove_match(999));
}

TEST_F(shm_tree_test, timeout_call_success_returns_value) {
    auto& harbor = mc::dbus::harbor::get_instance();

    auto future = std::async(std::launch::async, [this]() {
        mc::variants call_args;
        return m_tree->timeout_call(mc::milliseconds(50), m_service->name(),
                                    m_object->get_object_path(), "iface.mock", "Echo", "", call_args);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    mc::dbus::local_msg reply(m_service->name(), m_object->get_object_path(), "iface.mock", "Echo");
    reply.method_return();
    mc::variants reply_args;
    reply_args.emplace_back(mc::variant(123));
    reply.append_return_args("i", reply_args);

    bool delivered = false;
    for (uint32_t serial = 1; serial <= 5; ++serial) {
        if (harbor.reply_shm_msg(m_unique_name, serial, reply)) {
            delivered = true;
            break;
        }
    }
    EXPECT_TRUE(delivered);

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1U);
    EXPECT_TRUE((*result)[0].is_int32());
    EXPECT_EQ((*result)[0].as_int32(), 123);
}

TEST_F(shm_tree_test, timeout_call_wait_timeout_throws) {
    mc::variants args;
    EXPECT_THROW(
        m_tree->timeout_call(mc::milliseconds(1), m_service->name(),
                             m_object->get_object_path(), "iface.timeout", "Slow", "", args),
        mc::exception);
}

TEST_F(shm_tree_test, set_property_inner_handles_string_value) {
    auto prop = std::make_shared<shm::property>("s");
    mc::variant value("hello");

    mc::dbus::shm_tree::set_property_inner(prop, value);

    auto stored = prop->get_data();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(std::string(stored.value()), std::string("hello"));
}

TEST_F(shm_tree_test, set_property_inner_handles_null) {
    auto prop = std::make_shared<shm::property>("s");

    mc::dbus::shm_tree::set_property_inner(prop, mc::variant());

    auto stored = prop->get_data();
    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->empty());
}

} // namespace
