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

#include <mc/engine/engine.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/memory.h>

namespace mc::test::engine_tests {

class service_test_interface : public ::mc::engine::interface<service_test_interface> {
public:
    MC_INTERFACE("org.openubmc.test.service.interface")

    ::mc::engine::property<int32_t> m_value{0};
};

class service_test_object : public ::mc::engine::object<service_test_object> {
public:
    MC_OBJECT(service_test_object, "ServiceObject", "/org/openubmc/test_service_object", (service_test_interface))

    void init(std::string_view path, std::string_view name)
    {
        set_object_name(name);
        set_object_path(path);
    }

    service_test_interface m_iface;
};

} // namespace mc::test::engine_tests

MC_REFLECT(mc::test::engine_tests::service_test_interface, ((m_value, "Value")))
MC_REFLECT(mc::test::engine_tests::service_test_object, ((m_iface, "Interface")))

namespace {

using mc::test::engine_tests::service_test_object;

class ResolverGuard {
public:
    ResolverGuard() : m_previous(::mc::engine::engine::get_path_resolver())
    {}

    ~ResolverGuard()
    {
        ::mc::engine::engine::set_path_resolver(m_previous);
    }

private:
    ::mc::engine::path_resolver m_previous;
};

TEST(ServiceUtilsTest, ResolveObjectPathWithFallback)
{
    auto parent = ::mc::make_shared<service_test_object>();
    parent->init("/org/openubmc/parent", "Parent");

    auto child = ::mc::make_shared<service_test_object>();
    child->init("/org/openubmc/child", "Child");
    child->set_parent(parent);
    child->set_owner(parent.get());

    std::string resolved = ::mc::engine::service::resolve_object_path(" child/path ", *child);
    EXPECT_EQ(resolved, "/org/openubmc/parent/child/path");

    resolved = ::mc::engine::service::resolve_object_path("/absolute/path", *child);
    EXPECT_EQ(resolved, "/absolute/path");
}

TEST(ServiceUtilsTest, ResolveObjectPathWithCustomResolver)
{
    ResolverGuard guard;
    ::mc::engine::engine::set_path_resolver(
        [](std::string_view pattern, const ::mc::engine::abstract_object&, std::string& out) {
        out = std::string("/custom/") + std::string(pattern);
        return true;
    });

    auto parent = ::mc::make_shared<service_test_object>();
    parent->init("/org/openubmc/parent", "Parent");
    auto child = ::mc::make_shared<service_test_object>();
    child->init("/org/openubmc/child", "Child");
    child->set_parent(parent);
    child->set_owner(parent.get());

    auto resolved = ::mc::engine::service::resolve_object_path("segment", *child);
    EXPECT_EQ(resolved, "/custom/segment");
}

} // namespace
