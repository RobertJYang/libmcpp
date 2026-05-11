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
#include <mc/engine/errors/std_errors.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/error_engine.h>
#include <mc/exception.h>
#include <mc/reflect/type_code.h>
#include <mc/runtime/steady_timer.h>
#include <test_utilities/base.h>
#include <test_utilities/engine_test_base.h>

#include <chrono>
#include <future>

namespace mc::test::engine_tests {

class context_test_interface : public ::mc::engine::interface<context_test_interface> {
public:
    MC_INTERFACE("org.openubmc.test.context.interface")

    ::mc::engine::property<int32_t> m_value{0};

    uint32_t read_privilege_from_current_context()
    {
        auto* current_ctx = ::mc::engine::context::get_current_context_ptr();
        if (current_ctx == nullptr) {
            return 0;
        }
        auto privilege = current_ctx->privilege();
        if (!privilege.has_value()) {
            return 0;
        }
        return *privilege;
    }

    ::mc::string read_label_from_current_context(::mc::string label)
    {
        auto* current_ctx = ::mc::engine::context::get_current_context_ptr();
        if (current_ctx == nullptr) {
            return {};
        }
        auto result   = std::move(label);
        auto username = current_ctx->username();
        result += ":";
        if (username.has_value()) {
            result += *username;
        }
        return result;
    }

    uint32_t read_privilege_from_explicit_context(::mc::engine::context ctx)
    {
        auto privilege = ctx.privilege();
        if (!privilege.has_value()) {
            return 0;
        }
        return *privilege;
    }

    ::mc::string read_label_from_explicit_context(::mc::engine::context ctx, ::mc::string label)
    {
        auto result   = std::move(label);
        auto username = ctx.username();
        result += ":";
        if (username.has_value()) {
            result += *username;
        }
        return result;
    }

    // 从隐式上下文（含 dispatcher 合并后的 wire dict）按键读取，用于模拟入站 RPC 携带的扩展字段。
    ::mc::string read_wire_extension(::mc::string key)
    {
        auto* current_ctx = ::mc::engine::context::get_current_context_ptr();
        if (current_ctx == nullptr) {
            return {};
        }
        auto value = current_ctx->get_arg(key);
        if (value.is_null()) {
            return {};
        }
        if (value.is_string()) {
            return value.as_string();
        }
        return value.to_string();
    }

    // 隐式上下文、两个业务 string 参数，用于「参数个数不足」等负数用例。
    ::mc::string concat_implicit_two(::mc::string a, ::mc::string b)
    {
        return a + b;
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
           ((m_value, "Value"))((read_privilege_from_current_context, "ReadPrivilegeFromCurrentContext"))(
               (read_label_from_current_context, "ReadLabelFromCurrentContext"))(
               (read_wire_extension, "ReadWireExtension"))((concat_implicit_two, "ConcatImplicitTwo"))((
               read_privilege_from_explicit_context,
               "ReadPrivilegeFromExplicitContext"))((read_label_from_explicit_context, "ReadLabelFromExplicitContext")))
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

    ctx.set_auth(::mc::engine::auth_state::auth_required);
    ctx.set_privilege(255);
    ctx.set_username("tester");
    ctx.set_client_addr("127.0.0.1");
    ctx.set_interface_name("org.test.interface");
    ctx.local()["OverrideMode"]   = "set";
    ctx.extensions()["CustomKey"] = 42;

    EXPECT_EQ(ctx.auth(), ::mc::engine::auth_state::auth_required);
    EXPECT_EQ(ctx.privilege(), 255U);
    ASSERT_TRUE(ctx.username().has_value());
    EXPECT_EQ(*ctx.username(), "tester");
    ASSERT_TRUE(ctx.client_addr().has_value());
    EXPECT_EQ(*ctx.client_addr(), "127.0.0.1");
    ASSERT_TRUE(ctx.interface_name().has_value());
    EXPECT_EQ(*ctx.interface_name(), "org.test.interface");

    EXPECT_EQ(ctx.get_arg("Auth").as_int64(), static_cast<int64_t>(::mc::engine::auth_state::auth_required));
    EXPECT_EQ(ctx.get_arg("Privilege").as_uint32(), 255U);
    EXPECT_EQ(ctx.get_arg("username").as_string(), "tester");
    EXPECT_EQ(ctx.get_arg("client_addr").as_string(), "127.0.0.1");
    EXPECT_EQ(ctx.get_arg("interface_name").as_string(), "org.test.interface");
    EXPECT_EQ(ctx.get_arg("OverrideMode").as_string(), "set");

    auto args = ctx.get_args();
    EXPECT_EQ(args["Auth"].as_int64(), static_cast<int64_t>(::mc::engine::auth_state::auth_required));
    EXPECT_EQ(args["Privilege"].as_uint32(), 255U);
    EXPECT_EQ(args["CustomKey"].as_int64(), 42);
    EXPECT_EQ(args["OverrideMode"].as_string(), "set");

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
    ::mc::dict args_dict = {
        {"Auth", "1"},
        {"Privilege", 255},
        {"username", "alice"},
        {"client_addr", "10.0.0.8"},
        {"interface_name", "org.test.batch"},
        {"OverrideMode", "get"},
        {"key3", true},
    };

    ctx.set_args(args_dict);

    ASSERT_TRUE(ctx.auth().has_value());
    EXPECT_EQ(*ctx.auth(), ::mc::engine::auth_state::auth_required);
    ASSERT_TRUE(ctx.privilege().has_value());
    EXPECT_EQ(*ctx.privilege(), 255U);
    ASSERT_TRUE(ctx.username().has_value());
    EXPECT_EQ(*ctx.username(), "alice");
    ASSERT_TRUE(ctx.client_addr().has_value());
    EXPECT_EQ(*ctx.client_addr(), "10.0.0.8");
    ASSERT_TRUE(ctx.interface_name().has_value());
    EXPECT_EQ(*ctx.interface_name(), "org.test.batch");

    EXPECT_EQ(ctx.local()["OverrideMode"].as_string(), "get");
    EXPECT_EQ(ctx.extensions()["key3"].as_bool(), true);
    EXPECT_EQ(ctx.get_arg("Auth").as_int64(), 1);
    EXPECT_EQ(ctx.get_arg("Privilege").as_uint32(), 255U);
    EXPECT_EQ(ctx.get_arg("key3").as_bool(), true);
}

TEST_F(ContextTest, CurrentSharesStateCloneCreatesIndependentCopy)
{
    ::mc::engine::context ctx(service, object);
    ctx.set_username("origin");
    ctx.local()["OverrideMode"]   = "set";
    ctx.extensions()["CustomKey"] = "value";

    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto                                 shared_ctx = ::mc::engine::context::current();
        shared_ctx.set_username("shared");
        shared_ctx.local()["OverrideMode"] = "shared_mode";

        auto cloned_ctx = shared_ctx.clone();
        cloned_ctx.set_username("cloned");
        cloned_ctx.local()["OverrideMode"]   = "cloned_mode";
        cloned_ctx.extensions()["CustomKey"] = "cloned_value";

        ASSERT_TRUE(ctx.username().has_value());
        EXPECT_EQ(*ctx.username(), "shared");
        EXPECT_EQ(ctx.local()["OverrideMode"].as_string(), "shared_mode");
        EXPECT_EQ(ctx.extensions()["CustomKey"].as_string(), "value");

        ASSERT_TRUE(shared_ctx.username().has_value());
        EXPECT_EQ(*shared_ctx.username(), "shared");
        EXPECT_EQ(shared_ctx.extensions()["CustomKey"].as_string(), "value");

        ASSERT_TRUE(cloned_ctx.username().has_value());
        EXPECT_EQ(*cloned_ctx.username(), "cloned");
        EXPECT_EQ(cloned_ctx.local()["OverrideMode"].as_string(), "cloned_mode");
        EXPECT_EQ(cloned_ctx.extensions()["CustomKey"].as_string(), "cloned_value");
    }
}

TEST_F(ContextTest, WithContextRestoresCapturedFrameAndState)
{
    ::mc::engine::context ctx(service, object);
    ctx.set_username("captured");
    ctx.set_call_info(::mc::engine::detail::variants_call{{}, "org.test.iface", "Method"});

    ::mc::engine::context held_ctx;
    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        held_ctx = ::mc::engine::context::current();
    }

    EXPECT_NO_THROW({
        auto result = ::mc::engine::with_context(held_ctx, [&]() -> ::mc::string_view {
            auto* current_ptr = ::mc::engine::context::get_current_context_ptr();
            EXPECT_NE(current_ptr, nullptr);
            EXPECT_EQ(&current_ptr->get_service(), &service);
            EXPECT_EQ(&current_ptr->get_object(), &object);
            EXPECT_TRUE(current_ptr->username().has_value());
            if (current_ptr->username().has_value()) {
                EXPECT_EQ(*current_ptr->username(), "captured");
            }
            current_ptr->set_username("updated");
            return current_ptr->get_method_name();
        });
        EXPECT_EQ(result, "Method");
    });

    ASSERT_TRUE(ctx.username().has_value());
    EXPECT_EQ(*ctx.username(), "updated");
}

TEST_F(ContextTest, BindContextCapturesCurrentContextForLaterInvocation)
{
    ::mc::engine::context ctx(service, object);
    ctx.set_username("bound");

    std::function<::mc::string()> bound_fn;
    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        bound_fn = ::mc::engine::bind_context([&]() -> ::mc::string {
            auto& current_ctx = ::mc::engine::context::get_current_context();
            EXPECT_EQ(&current_ctx.get_service(), &service);
            EXPECT_EQ(&current_ctx.get_object(), &object);
            current_ctx.extensions()["from_bound"] = true;
            return current_ctx.username().value_or("");
        });
    }

    ASSERT_TRUE(static_cast<bool>(bound_fn));
    EXPECT_EQ(bound_fn(), "bound");
    EXPECT_EQ(ctx.extensions()["from_bound"].as_bool(), true);
}

TEST_F(ContextTest, BindContextWithCloneKeepsIsolatedState)
{
    ::mc::engine::context ctx(service, object);
    ctx.set_username("origin");

    std::function<void()> bound_fn;
    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        bound_fn = ::mc::engine::bind_context(::mc::engine::context::current().clone(), [&]() {
            auto& current_ctx = ::mc::engine::context::get_current_context();
            current_ctx.set_username("isolated");
        });
    }

    bound_fn();
    ASSERT_TRUE(ctx.username().has_value());
    EXPECT_EQ(*ctx.username(), "origin");
}

TEST_F(ContextTest, internal_rpc_context_snapshot_matches_extensions_and_fixed_fields)
{
    ::mc::engine::context ctx(service, object);
    ctx.set_username("alice");
    ctx.set_privilege(3U);
    ctx.extensions()["Biz"]     = 7;
    ctx.local()["OverrideMode"] = "set";

    ::mc::dict wire = ctx.snapshot_for_internal_rpc();

    EXPECT_FALSE(wire.contains("OverrideMode"));
    EXPECT_EQ(ctx.local()["OverrideMode"].as_string(), "set");

    ASSERT_TRUE(wire.contains("username"));
    EXPECT_EQ(wire["username"].as_string(), "alice");

    ASSERT_TRUE(wire.contains("Biz"));
    EXPECT_EQ(wire["Biz"].as_int64(), 7);

    ASSERT_TRUE(wire.contains("Privilege"));
    EXPECT_EQ(wire["Privilege"].as_uint32(), 3U);
}

using namespace std::chrono_literals;

class ContextTimerPropagationTest : public mc::test::TestWithRuntime {
protected:
    void SetUp() override
    {
        object = context_test_object::create();
        object->init();
    }

    context_test_service                  service;
    ::mc::shared_ptr<context_test_object> object;
};

TEST_F(ContextTimerPropagationTest, steady_timer_without_bind_context_has_null_tls_engine_context)
{
    std::promise<bool> tls_empty;

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&](const std::error_code& ec) {
        EXPECT_FALSE(ec);
        tls_empty.set_value(!ec && (::mc::engine::context::get_current_context_ptr() == nullptr));
    });

    auto fut = tls_empty.get_future();
    ASSERT_EQ(fut.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(fut.get());
}

TEST_F(ContextTimerPropagationTest, bind_context_restores_tls_in_steady_timer_callback)
{
    ::mc::engine::context ctx(service, *object);
    ctx.set_username("timer_ctx");

    std::promise<void>        done;
    mc::runtime::steady_timer timer(mc::get_io_executor());
    {
        ::mc::engine::context_stack::context guard(&service, ctx);
        auto                                 wrapped = ::mc::engine::bind_context([&]() -> void {
            auto* ptr = ::mc::engine::context::get_current_context_ptr();
            ASSERT_NE(ptr, nullptr);
            ASSERT_TRUE(ptr->username().has_value());
            EXPECT_EQ(*ptr->username(), "timer_ctx");
            done.set_value();
        });

        timer.expires_after(20ms);
        timer.async_wait([wrapped](const std::error_code& ec) mutable {
            EXPECT_FALSE(ec);
            if (ec) {
                return;
            }
            wrapped();
        });
    }

    auto done_fut = done.get_future();
    ASSERT_EQ(done_fut.wait_for(2s), std::future_status::ready);
}

class ContextDispatchTest : public ::mc::test::TestWithEngine {
protected:
    ::mc::engine::message make_method_call(mc::string_view member_name, mc::string_view signature,
                                           const ::mc::variants& args) const
    {
        ::mc::engine::message request;
        request.header.type           = ::mc::engine::message_type::method_call;
        request.header.destination    = ::mc::string(service.name());
        request.header.sender         = "org.openubmc.test.client";
        request.header.path           = object->get_object_path();
        request.header.interface_name = "org.openubmc.test.context.interface";
        request.header.member_name    = member_name;
        request.header.serial         = 1;
        request.body                  = ::mc::engine::make_payload<::mc::engine::method_call_payload>(signature, args);
        return request;
    }

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

TEST_F(ContextDispatchTest, dispatcher_rejects_wire_dict_with_non_string_keys_for_implicit_optional_context)
{
    ::mc::dict context_args;
    context_args[mc::variant(static_cast<int64_t>(1))] = mc::variant("bad_key_shape");
    context_args["username"]                           = mc::variant("alice");

    auto request  = make_method_call("ReadLabelFromCurrentContext", "a{ss}s", ::mc::variants{context_args, "demo"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::error);
    EXPECT_EQ(response.header.error_name, ::mc::engine::errors::invalid_args.name);
}

TEST_F(ContextDispatchTest, dispatcher_rejects_too_few_business_args_for_implicit_method)
{
    auto request  = make_method_call("ConcatImplicitTwo", "", ::mc::variants{"only_one"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::error);
    EXPECT_EQ(response.header.error_name, ::mc::engine::errors::invalid_args.name);
}

TEST_F(ContextDispatchTest, dispatcher_rejects_too_many_args_for_explicit_zero_business_method)
{
    ::mc::dict context_args = {{"Privilege", "1"}};
    auto       request      = make_method_call("ReadPrivilegeFromExplicitContext", "a{ss}s",
                                               ::mc::variants{context_args, mc::variant("junk_arg")});
    auto       response     = ::mc::engine::dispatch(service, request);

    ASSERT_EQ(response.header.type, ::mc::engine::message_type::error);
    EXPECT_EQ(response.header.error_name, ::mc::engine::errors::invalid_args.name);
}

TEST_F(ContextDispatchTest, dispatcher_implicit_two_args_without_wire_concatenates_normally)
{
    auto request =
        make_method_call("ConcatImplicitTwo", "", ::mc::variants{mc::variant("hello"), mc::variant("world")});
    auto response = ::mc::engine::dispatch(service, request);

    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "helloworld");
}

TEST_F(ContextDispatchTest, dispatcher_implicit_method_accepts_optional_wire_and_two_business_args)
{
    ::mc::dict context_args = {{"username", "u"}};
    auto       request      = make_method_call("ConcatImplicitTwo", "a{ss}ss",
                                               ::mc::variants{context_args, mc::variant("hi"), mc::variant("bye")});
    auto       response     = ::mc::engine::dispatch(service, request);

    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "hibye");
}

TEST_F(ContextDispatchTest, inbound_implicit_wire_behaves_like_rpc_with_extensions_visible_in_implicit_context)
{
    ::mc::dict context_args = {{"username", "rpc_user"}, {"TenantKey", "tenant-xyz"}};
    auto       request  = make_method_call("ReadWireExtension", "a{ss}s", ::mc::variants{context_args, "TenantKey"});
    auto       response = ::mc::engine::dispatch(service, request);

    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "tenant-xyz");
}

TEST_F(ContextDispatchTest, dispatcher_allows_implicit_context_with_optional_wire_context)
{
    ::mc::dict context_args = {{"Privilege", "255"}, {"username", "alice"}};

    auto request  = make_method_call("ReadLabelFromCurrentContext", "a{ss}s", ::mc::variants{context_args, "demo"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "demo:alice");
}

TEST_F(ContextDispatchTest, dispatcher_injects_explicit_context_when_wire_context_is_omitted)
{
    auto request  = make_method_call("ReadPrivilegeFromExplicitContext", "", {});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as_uint32(), 0U);
}

TEST_F(ContextDispatchTest, dispatcher_accepts_a_ss_for_explicit_context)
{
    ::mc::dict context_args = {{"Privilege", "255"}};

    auto request  = make_method_call("ReadPrivilegeFromExplicitContext", "a{ss}", ::mc::variants{context_args});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as_uint32(), 255U);
}

TEST_F(ContextDispatchTest, dispatcher_accepts_a_sv_for_explicit_context)
{
    ::mc::dict context_args = {{"Privilege", 255U}, {"username", "alice"}};

    auto request  = make_method_call("ReadLabelFromExplicitContext", "a{sv}s", ::mc::variants{context_args, "demo"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::method_return);
    auto* payload = response.try_as<::mc::engine::method_return_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->value.as<::mc::string>(), "demo:alice");
}

TEST_F(ContextDispatchTest, dispatcher_rejects_wrong_first_arg_type_for_explicit_context)
{
    auto request  = make_method_call("ReadPrivilegeFromExplicitContext", "s", ::mc::variants{"bad"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::error);
    EXPECT_EQ(response.header.error_name, ::mc::engine::errors::invalid_args.name);
}

TEST_F(ContextDispatchTest, dispatcher_rejects_extra_args_beyond_business_signature)
{
    ::mc::dict context_args = {{"Privilege", "255"}};

    auto request =
        make_method_call("ReadPrivilegeFromExplicitContext", "a{ss}s", ::mc::variants{context_args, "extra"});
    auto response = ::mc::engine::dispatch(service, request);
    ASSERT_EQ(response.header.type, ::mc::engine::message_type::error);
    EXPECT_EQ(response.header.error_name, ::mc::engine::errors::invalid_args.name);
}

TEST_F(ContextDispatchTest, metadata_exposes_explicit_context_and_business_signature)
{
    auto explicit_info =
        object->get_metadata().get_method_info("ReadLabelFromExplicitContext", "org.openubmc.test.context.interface");
    ASSERT_NE(explicit_info.item, nullptr);
    EXPECT_TRUE(explicit_info.item->has_explicit_context_arg());
    EXPECT_EQ(explicit_info.item->arg_count(), 2U);
    EXPECT_EQ(explicit_info.item->business_arg_count(), 1U);
    EXPECT_EQ(explicit_info.item->get_args_signature(), "(s)");
    EXPECT_EQ(explicit_info.item->get_full_args_signature(), "(a{ss}s)");

    auto implicit_info =
        object->get_metadata().get_method_info("ReadLabelFromCurrentContext", "org.openubmc.test.context.interface");
    ASSERT_NE(implicit_info.item, nullptr);
    EXPECT_FALSE(implicit_info.item->has_explicit_context_arg());
    EXPECT_EQ(implicit_info.item->arg_count(), 1U);
    EXPECT_EQ(implicit_info.item->business_arg_count(), 1U);
    EXPECT_EQ(implicit_info.item->get_args_signature(), "(s)");
    EXPECT_EQ(implicit_info.item->get_full_args_signature(), "(s)");
}

TEST(ContextSignatureModeTest, default_mode_is_a_ss)
{
    // 反射/默认仍为 a_ss；内部转发线型独立于环境变量。
    EXPECT_EQ(mc::engine::internal_context_wire_signature_string(), mc::reflect::container::dict_string_var);
}

} // namespace
