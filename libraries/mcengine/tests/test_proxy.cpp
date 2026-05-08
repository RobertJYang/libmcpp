/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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

#include <mc/engine.h>
#include <mc/engine/proxy.h>
#include <mc/engine/service_proto.h>
#include <mc/engine/std_interface.h>
#include <mc/runtime.h>
#include <test_utilities/engine_test_base.h>

#include <chrono>
#include <thread>

// ============================================================================
// 综合测试类型：一个 interface 覆盖简单属性、复杂属性、方法、虚方法 override
// ============================================================================

namespace {

// 综合测试 interface：简单属性 + 复杂属性(vector/dict) + 方法
class test_interface : public mc::engine::interface<test_interface> {
public:
    MC_INTERFACE("org.test.Proxy")

    mc::engine::property<int32_t>              IntValue;
    mc::engine::property<mc::string>           StrValue;
    mc::engine::property<std::vector<int32_t>> VecValue;
    mc::engine::property<mc::dict>             DictValue;

    virtual int32_t Add(int32_t a, int32_t b)
    {
        return a + b;
    }

    virtual std::vector<uint8_t> RepeatByte(uint8_t value, uint8_t count)
    {
        return std::vector<uint8_t>(count, value);
    }
};

// 虚方法 override 版本
class test_interface_override : public test_interface {
public:
    int32_t Add(int32_t a, int32_t b) override
    {
        return a * 100 + b;
    }
};

// 综合测试 object
class test_object : public mc::engine::object<test_object> {
public:
    MC_OBJECT(test_object, "TestObject", "/org/test/proxy/obj0", (test_interface))
    test_interface iface;
};

// 虚方法 override 版本的 object
class test_object_override : public mc::engine::object<test_object_override> {
public:
    MC_OBJECT(test_object_override, "TestObjectOverride", "/org/test/proxy/obj1", (test_interface_override))
    test_interface_override iface;
};

struct test_service : public mc::engine::service {
    test_service() : mc::engine::service("org.test.proxy.Svc")
    {}
};

struct other_service : public mc::engine::service {
    other_service() : mc::engine::service("org.test.proxy.Other")
    {}
};

class sink_transport : public mc::proto::protocol {
protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        return complete(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        return complete(req);
    }
};

#if MCENGINE_USE_SHM
class fixed_shm_resolver : public mc::engine::proxy_shm_resolver {
public:
    explicit fixed_shm_resolver(mc::engine::shm_object* object) : m_object(object)
    {}

    mc::engine::resolved_shm_object resolve(const mc::engine::object_ref&) override
    {
        return {m_object, std::nullopt};
    }

private:
    mc::engine::shm_object* m_object{nullptr};
};
#endif // MCENGINE_USE_SHM

// 客户端 proxy 类型
class test_iface_proxy : public mc::engine::interface_proxy<test_iface_proxy> {
public:
    MC_INTERFACE_PROXY("org.test.Proxy");

    MC_PROXY_PROP(int32_t, IntValue);
    MC_PROXY_PROP(mc::string, StrValue);
    MC_PROXY_PROP(std::vector<int32_t>, VecValue);
    MC_PROXY_PROP(mc::dict, DictValue);

    MC_PROXY_METHOD(int32_t, Add, ((int32_t, a))((int32_t, b)));
    MC_PROXY_METHOD((std::vector<uint8_t>, "ay"), RepeatByte, ((uint8_t, value, "y"))((uint8_t, count)));
};

class test_obj_proxy {
public:
    test_iface_proxy iface;

    void bind_proxy(const mc::engine::object_proxy_seed& seed)
    {
        iface.bind_proxy(seed);
    }
};

} // namespace

// 反射注册
MC_REFLECT(test_interface,
           ((IntValue, "IntValue"))((StrValue, "StrValue"))((VecValue, "VecValue"))((DictValue, "DictValue"))((Add,
                                                                                                               "Add"))(
               (RepeatByte, "RepeatByte")))
MC_REFLECT(test_object, ((iface, "Iface")))
MC_REFLECT(test_interface_override, ((Add, "Add")))
MC_REFLECT(test_object_override, ((iface, "Iface")))

// ============================================================================
// 端到端测试 fixture：TestWithEngine 在 use_shm 时自动管理 SHM runtime，
// 否则退化为纯消息模式。一个 service + 两个 object 覆盖所有场景。
// ============================================================================

class proxy_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        mc::runtime::reset_runtime_context();
        // SHM region + engine 单例由基类 TestWithEngine::SetUp 统一管理。
        mc::test::TestWithEngine::SetUp();

        svc.init();
        svc.start();

        obj = test_object::create();
        obj->set_object_name("test_obj");
        obj->iface.IntValue  = int32_t{42};
        obj->iface.StrValue  = mc::string("hello");
        obj->iface.VecValue  = std::vector<int32_t>{1, 2, 3};
        obj->iface.DictValue = mc::dict{{"k1", mc::variant(mc::string("v1"))}, {"k2", mc::variant(int32_t{99})}};
        svc.register_object(obj);

        obj_override = test_object_override::create();
        obj_override->set_object_name("test_obj_override");
        obj_override->iface.IntValue = int32_t{100};
        svc.register_object(obj_override);

        other_svc.init();
        other_svc.start();
    }

    void TearDown() override
    {
        other_svc.stop();
        svc.stop();
        // engine 单例 reset 与 SHM region unlink 由基类 TearDown 完成。
        mc::test::TestWithEngine::TearDown();
        mc::runtime::reset_runtime_context();
    }

    test_service                         svc;
    other_service                        other_svc;
    mc::shared_ptr<test_object>          obj;
    mc::shared_ptr<test_object_override> obj_override;
};

// ---- 属性读写（简单 + 复杂）----
// SHM ON 时：IntValue/StrValue 通过 SHM 直读，VecValue/DictValue 走消息 fallback
// SHM OFF 时：全部走消息 dispatch

TEST_F(proxy_test, read_write_simple_and_complex_properties)
{
    auto proxy = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);
    ASSERT_TRUE(proxy->iface.is_bound());

    // 简单属性读取
    EXPECT_EQ(proxy->iface.IntValue.get_value(), 42);
    EXPECT_EQ(proxy->iface.StrValue.get_value(), mc::string("hello"));

    // 简单属性写入（通过消息 dispatch）
    proxy->iface.IntValue.set_value(int32_t{888});
    EXPECT_EQ(static_cast<int32_t>(obj->iface.IntValue), 888);

    // 复杂属性读取（vector/dict 走 Properties.Get fallback）
    auto vec = proxy->iface.VecValue.get_value();
    EXPECT_EQ(vec.size(), 3U);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[2], 3);

    auto dict = proxy->iface.DictValue.get_value();
    EXPECT_EQ(dict.size(), 2U);
    EXPECT_EQ(dict["k1"], mc::string("v1"));
    EXPECT_EQ(dict["k2"], int32_t{99});

    // 复杂属性写入
    proxy->iface.VecValue.set_value(std::vector<int32_t>{10, 20, 30, 40});
    auto updated = static_cast<std::vector<int32_t>>(obj->iface.VecValue);
    EXPECT_EQ(updated.size(), 4U);
    EXPECT_EQ(updated[3], 40);
}

// ---- 方法调用 + 虚方法 override ----

TEST_F(proxy_test, method_invoke_and_virtual_override)
{
    auto proxy = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->iface.Add(3, 4), 7);

    auto proxy_override = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj1");
    ASSERT_NE(proxy_override, nullptr);
    EXPECT_EQ(proxy_override->iface.Add(3, 4), 304);
}

TEST_F(proxy_test, method_invoke_with_explicit_result_and_param_signature)
{
    auto proxy = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);

    auto bytes = proxy->iface.RepeatByte(uint8_t{0x5a}, uint8_t{3});
    EXPECT_EQ(bytes, (std::vector<uint8_t>{0x5a, 0x5a, 0x5a}));
}

TEST_F(proxy_test, explicit_result_signature_mismatch_throws)
{
    auto proxy = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);

    EXPECT_THROW((void)proxy->iface.context().invoke("RepeatByte", mc::variants{uint8_t{1}, uint8_t{2}}, "yy", "s"),
                 mc::method_call_exception);
}

// ---- 跨 service 访问 ----

TEST_F(proxy_test, cross_service_access)
{
    auto proxy = other_svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);

    EXPECT_EQ(proxy->iface.IntValue.get_value(), 42);
    proxy->iface.IntValue.set_value(int32_t{999});
    EXPECT_EQ(static_cast<int32_t>(obj->iface.IntValue), 999);
    EXPECT_EQ(proxy->iface.Add(10, 5), 15);
}

// ---- service::call 高级接口 ----

TEST_F(proxy_test, service_call_high_level_api)
{
    auto result = svc.call("/org/test/proxy/obj0", "org.test.proxy.Svc", "org.test.Proxy", "Add",
                           mc::variants{int32_t{10}, int32_t{20}}, "ii");
    EXPECT_EQ(result, int32_t{30});
}

// ---- dynamic API：object_proxy 直接使用 ----

TEST_F(proxy_test, dynamic_api_via_service_dispatch)
{
    mc::engine::object_ref ref;
    ref.service = "org.test.proxy.Svc";
    ref.path    = "/org/test/proxy/obj0";

    mc::engine::proxy_policy policy;
    policy.route = mc::engine::proxy_route::no_cache;

    mc::engine::object_proxy proxy(&svc, ref, policy);

    EXPECT_EQ(proxy.get_property("org.test.Proxy", "IntValue"), int32_t{42});
    proxy.set_property("org.test.Proxy", "IntValue", int32_t{77});
    EXPECT_EQ(static_cast<int32_t>(obj->iface.IntValue), 77);

    auto add_result = proxy.invoke("org.test.Proxy", "Add", mc::variants{int32_t{2}, int32_t{5}}, "ii");
    EXPECT_EQ(add_result, int32_t{7});
}

TEST_F(proxy_test, dynamic_proxy_as_binds_typed_wrapper)
{
    mc::engine::object_ref ref;
    ref.service = "org.test.proxy.Svc";
    ref.path    = "/org/test/proxy/obj0";

    mc::engine::object_proxy dyn(&svc, ref);
    auto                     typed = dyn.as<test_obj_proxy>();

    ASSERT_NE(typed, nullptr);
    ASSERT_TRUE(typed->iface.is_bound());
    EXPECT_EQ(typed->iface.IntValue.get_value(), 42);
    typed->iface.IntValue.set_value(int32_t{1234});
    EXPECT_EQ(static_cast<int32_t>(obj->iface.IntValue), 1234);
    EXPECT_EQ(typed->iface.Add(6, 7), 13);
}

#if MCENGINE_USE_SHM
TEST_F(proxy_test, fast_available_only_returns_shm_subset_and_skips_nocache)
{
    obj->iface.IntValue = int32_t{43};
    obj->iface.StrValue = mc::string("fast");

    auto proxy = svc.get_proxy<test_obj_proxy>("/org/test/proxy/obj0");
    ASSERT_NE(proxy, nullptr);

    auto fast = proxy->iface.m_ctx.get_all_properties(mc::engine::proxy_get_all_mode::fast_available_only);
    EXPECT_EQ(fast["IntValue"], int32_t{43});
    EXPECT_EQ(fast["StrValue"], mc::string("fast"));
    EXPECT_EQ(fast.find("VecValue"), fast.end());
    EXPECT_EQ(fast.find("DictValue"), fast.end());
}
#endif // MCENGINE_USE_SHM

TEST_F(proxy_test, stale_service_epoch_falls_back_to_message_get)
{
    mc::engine::object_ref ref;
    ref.service       = "org.test.proxy.Svc";
    ref.path          = "/org/test/proxy/obj0";
    ref.service_epoch = std::uint64_t{0};

    mc::engine::proxy_policy policy;
    policy.route = mc::engine::proxy_route::auto_;

    mc::engine::object_proxy proxy(&svc, ref, policy);
    EXPECT_EQ(proxy.get_property("org.test.Proxy", "IntValue"), int32_t{42});
}

#if MCENGINE_USE_SHM
TEST_F(proxy_test, no_cache_route_uses_message_even_when_shm_has_stale_value)
{
    obj_override->iface.IntValue = int32_t{11};

    mc::engine::object_ref ref;
    ref.service = "org.test.proxy.Svc";
    ref.path    = "/org/test/proxy/obj0";
    auto resolver = mc::make_shared<fixed_shm_resolver>(obj_override->get_shm_handle());

    mc::engine::proxy_policy auto_policy;
    auto_policy.route = mc::engine::proxy_route::auto_;
    mc::engine::object_proxy auto_proxy(&svc, ref, auto_policy, resolver);
    EXPECT_EQ(auto_proxy.get_property("org.test.Proxy", "IntValue"), int32_t{11});

    mc::engine::proxy_policy no_cache_policy;
    no_cache_policy.route = mc::engine::proxy_route::no_cache;
    mc::engine::object_proxy no_cache_proxy(&svc, ref, no_cache_policy, resolver);
    EXPECT_EQ(no_cache_proxy.get_property("org.test.Proxy", "IntValue"), int32_t{42});
}
#endif // MCENGINE_USE_SHM

TEST_F(proxy_test, proxy_timeout_surfaces_as_exception)
{
    mc::engine::service_proto proto;
    sink_transport            transport;
    proto.add_child(transport);
    other_svc.set_proto(&proto);

    mc::engine::object_ref ref;
    ref.service = "org.test.proxy.Unreachable";
    ref.path    = "/org/test/proxy/obj0";

    mc::engine::proxy_policy policy;
    policy.route   = mc::engine::proxy_route::no_cache;
    policy.timeout = mc::milliseconds(5);

    mc::engine::object_proxy proxy(&other_svc, ref, policy);
    const auto               begin = std::chrono::steady_clock::now();
    EXPECT_THROW((void)proxy.get_property("org.test.Proxy", "IntValue"), mc::exception);
    EXPECT_LT(std::chrono::steady_clock::now() - begin, std::chrono::seconds(1));

    other_svc.set_proto(nullptr);
}

// ---- 错误路径 ----

TEST_F(proxy_test, error_on_unknown_object)
{
    mc::engine::object_ref ref;
    ref.service = "org.test.proxy.Svc";
    ref.path    = "/org/test/nonexistent";

    mc::engine::proxy_policy policy;
    policy.route = mc::engine::proxy_route::no_cache;

    mc::engine::object_proxy proxy(&svc, ref, policy);
    EXPECT_THROW((void)proxy.get_property("org.test.Proxy", "IntValue"), mc::exception);
}

TEST_F(proxy_test, path_not_found_returns_nullptr)
{
    auto proxy = svc.get_proxy<test_obj_proxy>("/no/such/path");
    EXPECT_EQ(proxy, nullptr);
}

TEST_F(proxy_test, unbound_proxy_throws)
{
    test_iface_proxy iface;
    EXPECT_FALSE(iface.m_ctx.is_bound());
    EXPECT_THROW((void)iface.IntValue.get_value(), mc::exception);
    EXPECT_THROW(iface.IntValue.set_value(int32_t{1}), mc::exception);
    EXPECT_THROW(iface.Add(1, 2), mc::exception);
}
