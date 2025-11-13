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

#include <mc/engine/context.h>
#include <mc/engine/engine.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/error_engine.h>
#include <mc/exception.h>

namespace mc::test::engine_tests {

class context_test_interface : public ::mc::engine::interface<context_test_interface> {
public:
    MC_INTERFACE("org.openubmc.test.context.interface")

    ::mc::engine::property<int32_t> m_value{0};
};

class context_test_object : public ::mc::engine::object<context_test_object> {
public:
    MC_OBJECT(context_test_object, "ContextObject", "/org/openubmc/test_context",
              (context_test_interface))

    void init() {
        set_object_name("ContextObject");
        set_object_path("/org/openubmc/test_context");
    }

    context_test_interface m_iface;
};

class context_test_service : public ::mc::engine::service {
public:
    context_test_service()
        : ::mc::engine::service("org.openubmc.test.context.service") {
    }

    bool start() override {
        return true;
    }

    bool stop() override {
        return true;
    }
};

} // namespace mc::test::engine_tests

MC_REFLECT(mc::test::engine_tests::context_test_interface, ((m_value, "Value")))
MC_REFLECT(mc::test::engine_tests::context_test_object, ((m_iface, "Interface")))

namespace {

using mc::test::engine_tests::context_test_object;
using mc::test::engine_tests::context_test_service;

class ContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        object.init();
    }

    context_test_service service;
    context_test_object  object;
};

TEST_F(ContextTest, ArgsAndStatusManagement) {
    ::mc::engine::context ctx(service, object);

    EXPECT_TRUE(ctx.get_arg("missing").is_null());

    ctx.set_arg("key", ::mc::variant(42));
    EXPECT_EQ(ctx.get_arg("key").as_int64(), 42);
    EXPECT_EQ(ctx.get_args().at("key").as_int64(), 42);

    ctx.ignore();
    EXPECT_EQ(ctx.get_status(), ::mc::engine::handler_status::ignored);

    ctx.accept();
    EXPECT_EQ(ctx.get_status(), ::mc::engine::handler_status::accepted);
}

TEST_F(ContextTest, CallInfoInspection) {
    ::mc::engine::context ctx(service, object);

    ::mc::variants                     args;
    ::mc::engine::detail::variants_call call(args, "org.test.interface", "DoWork");
    call.path   = "/org/test/path";
    call.sender = "org.test.sender";
    ctx.set_call_info(call);

    EXPECT_EQ(ctx.get_path(), "/org/test/path");
    EXPECT_EQ(ctx.get_method_name(), "DoWork");
    EXPECT_EQ(ctx.get_interface_name(), "org.test.interface");
    EXPECT_EQ(ctx.get_sender(), "org.test.sender");
}

TEST_F(ContextTest, CurrentContextAccessAndErrorHandling) {
    ::mc::engine::context ctx(service, object);

    ::mc::variants                     args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    call.path = "/org/test/path";
    ctx.set_call_info(call);

    if (!::mc::error_engine::get_instance().is_registered("org.openubmc.test.context.error")) {
        ::mc::error_engine::get_instance().register_const_error("org.openubmc.test.context.error");
    }

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto* current_ptr = ::mc::engine::context::get_current_context_ptr();
        EXPECT_EQ(current_ptr, &ctx);
        EXPECT_NO_THROW(::mc::engine::context::get_current_context());
        EXPECT_THROW(::mc::engine::context::throw_error("org.openubmc.test.context.error"),
                     ::mc::method_call_exception);
    }

    ctx.report_error("org.openubmc.test.context.error", {{"detail", "value"}});
    EXPECT_TRUE(ctx.is_error());
    EXPECT_NE(ctx.get_error(), nullptr);
}

TEST_F(ContextTest, AbstractObjectContextAccess) {
    ::mc::engine::context ctx(service, object);
    ::mc::variants                     args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    ctx.set_call_info(call);

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto& current_ctx = object.get_context();
        EXPECT_EQ(&current_ctx, &ctx);
    }

    EXPECT_THROW(static_cast<void>(object.get_context()), ::mc::runtime_exception);
}

TEST_F(ContextTest, AbstractInterfaceContextAccess) {
    ::mc::engine::context ctx(service, object);
    ::mc::variants                     args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    ctx.set_call_info(call);

    auto& interface_ref = object.m_iface;

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto& current_ctx = interface_ref.get_context();
        EXPECT_EQ(&current_ctx, &ctx);
    }

    EXPECT_THROW(static_cast<void>(interface_ref.get_context()), ::mc::runtime_exception);
}

} // namespace

