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
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/error_engine.h>
#include <mc/exception.h>
#include <test_utilities/engine_test_base.h>

namespace mc::test::engine_tests {

class context_test_interface : public ::mc::engine::interface<context_test_interface> {
public:
    MC_INTERFACE("org.openubmc.test.context.interface")

    ::mc::engine::property<int32_t> m_value{0};

    ::mc::string read_privilege_from_context(const std::map<::mc::string, ::mc::string>& context)
    {
        auto it = context.find("Privilege");
        if (it == context.end()) {
            return "missing_param";
        }
        auto* current_ctx = ::mc::engine::context::get_current_context_ptr();
        if (current_ctx == nullptr) {
            return "missing_tls";
        }
        auto privilege = current_ctx->get_arg("Privilege");
        if (!privilege.is_string()) {
            return "missing_tls_value";
        }
        auto privilege_str = privilege.as<::mc::string_view>();
        if (privilege_str != it->second) {
            return "mismatch";
        }
        return ::mc::string(privilege_str);
    }
};

class context_test_object : public ::mc::engine::object<context_test_object> {
public:
    MC_OBJECT(context_test_object, "ContextObject", "/org/openubmc/test_context", (context_test_interface))

    void init()
    {
        set_object_name("ContextObject");
        set_object_path("/org/openubmc/test_context");
    }

    context_test_interface m_iface;
};

class context_test_service : public ::mc::engine::service {
public:
    context_test_service() : ::mc::engine::service("org.openubmc.test.context.service")
    {}
};

} // namespace mc::test::engine_tests

MC_REFLECT(mc::test::engine_tests::context_test_interface,
           ((m_value, "Value"))((read_privilege_from_context, "ReadPrivilegeFromContext")))
MC_REFLECT(mc::test::engine_tests::context_test_object, ((m_iface, "Interface")))

namespace {

using mc::test::engine_tests::context_test_object;
using mc::test::engine_tests::context_test_service;

class ContextTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        object.init();
    }

    void TearDown() override
    {
        // 确保在测试结束时清理 error_engine，避免全局清理时访问已销毁的对象
        ::mc::error_engine::reset_for_test();
    }

    context_test_service service;
    context_test_object  object;
};

TEST_F(ContextTest, ArgsAndStatusManagement)
{
    ::mc::engine::context ctx(service, object);

    EXPECT_TRUE(ctx.get_arg("missing").is_null());

    ctx.set_arg("key", ::mc::variant(42));
    EXPECT_EQ(ctx.get_arg("key").as_int64(), 42);
    EXPECT_EQ(ctx.get_args().at("key").as_int64(), 42);

    // 测试 const 版本的 get_args()
    const auto& const_args = ctx.get_args();
    EXPECT_EQ(const_args.at("key").as_int64(), 42);

    // 测试 const 版本的 get_service()
    const auto& const_service_ref = ctx.get_service();
    EXPECT_EQ(&const_service_ref, &service);

    ctx.ignore();
    EXPECT_EQ(ctx.get_status(), ::mc::engine::handler_status::ignored);

    ctx.accept();
    EXPECT_EQ(ctx.get_status(), ::mc::engine::handler_status::accepted);
}

TEST_F(ContextTest, CallInfoInspection)
{
    ::mc::engine::context ctx(service, object);

    ::mc::variants                      args;
    ::mc::engine::detail::variants_call call(args, "org.test.interface", "DoWork");
    call.path   = "/org/test/path";
    call.sender = "org.test.sender";
    ctx.set_call_info(call);

    EXPECT_EQ(ctx.get_path(), "/org/test/path");
    EXPECT_EQ(ctx.get_method_name(), "DoWork");
    EXPECT_EQ(ctx.get_interface_name(), "org.test.interface");
    EXPECT_EQ(ctx.get_sender(), "org.test.sender");
}

TEST_F(ContextTest, CurrentContextAccessAndErrorHandling)
{
    ::mc::engine::context ctx(service, object);

    ::mc::variants                      args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    call.path = "/org/test/path";
    ctx.set_call_info(call);

    if (!::mc::error_engine::get_instance().is_registered("org.openubmc.test.context.error")) {
        ::mc::error_engine::get_instance().register_const_error("org.openubmc.test.context.error");
    }

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto*                                current_ptr = ::mc::engine::context::get_current_context_ptr();
        EXPECT_EQ(current_ptr, &ctx);
        EXPECT_NO_THROW(::mc::engine::context::get_current_context());
        EXPECT_THROW(::mc::engine::context::throw_error("org.openubmc.test.context.error"),
                     ::mc::method_call_exception);
    }

    ctx.report_error("org.openubmc.test.context.error", {{"detail", "value"}});
    EXPECT_TRUE(ctx.is_error());
    EXPECT_NE(ctx.get_error(), nullptr);
}

TEST_F(ContextTest, AbstractObjectContextAccess)
{
    ::mc::engine::context               ctx(service, object);
    ::mc::variants                      args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    ctx.set_call_info(call);

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto&                                current_ctx = object.get_context();
        EXPECT_EQ(&current_ctx, &ctx);
    }

    EXPECT_THROW(static_cast<void>(object.get_context()), ::mc::runtime_exception);
}

TEST_F(ContextTest, AbstractInterfaceContextAccess)
{
    ::mc::engine::context               ctx(service, object);
    ::mc::variants                      args;
    ::mc::engine::detail::variants_call call(args, "org.test.iface", "Method");
    ctx.set_call_info(call);

    auto& interface_ref = object.m_iface;

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto&                                current_ctx = interface_ref.get_context();
        EXPECT_EQ(&current_ctx, &ctx);
    }

    EXPECT_THROW(static_cast<void>(interface_ref.get_context()), ::mc::runtime_exception);
}

TEST_F(ContextTest, ContextSetArgs)
{
    ::mc::engine::context ctx(service, object);

    // 使用 set_args 批量设置参数
    ::mc::dict args_dict;
    args_dict["key1"] = 42;
    args_dict["key2"] = "value";
    args_dict["key3"] = true;

    ctx.set_args(args_dict);

    // 验证参数已设置
    EXPECT_EQ(ctx.get_arg("key1").as_int64(), 42);
    EXPECT_EQ(ctx.get_arg("key2").as_string(), "value");
    EXPECT_EQ(ctx.get_arg("key3").as_bool(), true);
}

} // namespace

namespace {

class ContextDispatchTest : public ::mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        ASSERT_TRUE(service.init());
        ASSERT_TRUE(service.start());

        object = context_test_object::create();
        object->init();
        service.register_object(object);
    }

    void TearDown() override
    {
        service.stop();
        ::mc::error_engine::reset_for_test();
        TestWithEngine::TearDown();
    }

    context_test_service                  service;
    ::mc::shared_ptr<context_test_object> object;
};

TEST_F(ContextDispatchTest, dispatcher_injects_first_a_ss_into_thread_local_context)
{
    ::mc::dict context_args;
    context_args["Privilege"] = "255";

    ::mc::engine::message request;
    request.header.type           = ::mc::engine::message_type::method_call;
    request.header.destination    = ::mc::string(service.name());
    request.header.sender         = "org.openubmc.test.client";
    request.header.path           = object->get_object_path();
    request.header.interface_name = "org.openubmc.test.context.interface";
    request.header.member_name    = "ReadPrivilegeFromContext";
    request.header.serial         = 1;
    request.body = ::mc::engine::make_payload<::mc::engine::method_call_payload>("a{ss}", ::mc::variants{context_args});

    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "255");
}

} // namespace
