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
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/engine.h>
#include <mc/exception.h>
#include <mc/format.h>
#include <mc/singleton.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>

#include <map>

namespace test_engine {

struct test_service : public mc::engine::service {
    test_service()
        : mc::engine::service("org.openubmc.test_service")
    {
    }
};

template <typename T>
using property = mc::engine::property<T>;

class test_interface_1 : public mc::engine::interface<test_interface_1> {
public:
    MC_INTERFACE("org.test.test_interface_1")

    std::string rewite_method(const std::string& value)
    {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::not_supported);
    }

    int32_t add_values(int32_t a, int32_t b)
    {
        return a + b;
    }

    property<int32_t>          m_i32;
    property<std::string>      m_str;
    property<std::vector<int>> m_vec;
    int                        m_normal_v;
};

class test_interface_2 : public mc::engine::interface<test_interface_2> {
public:
    MC_INTERFACE("org.test.test_interface_2")

    property<mc::variant> m_variant;
};

class test_object : public mc::engine::object<test_object> {
public:
    MC_OBJECT(test_object, "TestObject", "/org/test/object_${object_id}_${i32 + 100}",
              (test_interface_1)(test_interface_2))

    std::string rewite_method(const std::string& value)
    {
        return sformat("test_object:rewite_method: {}", value);
    }

    test_interface_1 m_iface_1;
    test_interface_2 m_iface_2;
};

class engine_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        service.init();
        service.start();
    }

    void TearDown() override
    {
        service.stop();
        TestWithEngine::TearDown();
    }

    auto create_object(std::string_view path)
    {
        auto obj = test_object::create();

        int         object_id   = m_object_id++;
        std::string object_name = sformat("{}_{}", obj->get_class_name(), object_id);

        obj->set_object_path(path);
        obj->set_object_name(object_name);
        obj->set_position("0101");
        service.register_object(obj);
        return obj;
    };

    auto get_managed_objects(mc::engine::abstract_object& obj)
    {
        mc::variants var;
        for (auto [path, _] : obj.get_managed_objects()) {
            var.push_back(path);
        };
        return var;
    };

    auto objects(std::initializer_list<std::string_view> objs)
    {
        mc::variants vec;
        for (auto obj : objs) {
            vec.push_back(obj);
        }
        std::sort(vec.begin(), vec.end());
        return vec;
    }

    int          m_object_id = 0;
    test_service service;
};

} // namespace test_engine

MC_REFLECT(test_engine::test_interface_1,
           (m_i32, "i32")(m_str, "str")(m_vec, "vec")(m_normal_v, "normal_v"),
           (rewite_method)(rewite_method, "not_rewite_method")(add_values))
MC_REFLECT(test_engine::test_interface_2, (m_variant, "variant"))
MC_REFLECT(test_engine::test_object,
           (m_iface_1, "iface_1")(m_iface_2, "iface_2"),
           (rewite_method))

using namespace test_engine;

TEST_F(engine_test, test_engine_dbus_connection)
{
    mc::dbus::connection conn;
    try {
        conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
    } catch (const mc::exception& e) {
        GTEST_SKIP() << "SessionBus 未就绪，跳过测试: " << e.what();
        return;
    }

    if (!conn.start()) {
        GTEST_SKIP() << "SessionBus 无法启动，跳过测试";
        return;
    }

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    if (!reply.is_valid() || reply.is_error()) {
        GTEST_SKIP() << "SessionBus 响应异常，跳过测试: valid=" << reply.is_valid()
                     << " error=" << (reply.is_error() ? reply.get_error_name() : "none");
        return;
    }

    std::set<std::string> names;
    reply >> names;
    EXPECT_GE(names.count("org.openubmc.test_service"), 1);
}

TEST_F(engine_test, test_object_property_changed_sig)
{
    auto obj = test_object::create();
    obj->m_iface_1.m_i32.set_value(1);
    service.register_object(obj);

    // 检查对象路径模板计算是否正确
    auto path = obj->get_object_path();
    EXPECT_EQ(path, sformat("/org/test/object_{}_{}",
                            obj->get_object_id(), obj->m_iface_1.m_i32.get_value() + 100));

    mc::dict values;
    obj->property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    obj->m_iface_1.m_i32.set_value(10);
    obj->m_iface_1.m_i32.set_value(20);
    obj->m_iface_1.m_str.set_value("123");
    obj->m_iface_1.m_str.set_value(std::string("456"));
    std::string str = "000";
    obj->m_iface_1.m_str.set_value(str);
    obj->m_iface_1.m_vec.modify([&](auto& vec) {
        vec.push_back(1);
        return true;
    });
    obj->m_iface_2.m_variant.set_value(100);

    // 普通变量修改不会触发信号
    obj->m_iface_1.m_normal_v = 100;

    mc::dict expected = {
        {"i32", 10},
        {"i32", 20},
        {"str", "123"},
        {"str", "456"},
        {"str", "000"},
        {"vec", mc::variants{1}},
        {"variant", 100}};
    EXPECT_EQ(values, expected);
    EXPECT_EQ(str, obj->m_iface_1.m_str);
}

TEST_F(engine_test, test_interface_property_changed_sig)
{
    auto obj = test_object::create();
    service.register_object(obj);

    mc::dict values;
    obj->m_iface_1.property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    obj->m_iface_1.m_i32.set_value(10);
    obj->m_iface_1.m_str.set_value("123");

    // 接口2的属性修改不会触发接口1的信号
    obj->m_iface_2.m_variant.set_value(100);

    mc::dict expected = {{"i32", 10}, {"str", "123"}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_property_changed_sig)
{
    auto obj = test_object::create();
    service.register_object(obj);

    // 指定订阅接口1的i32属性
    mc::dict values;
    obj->m_iface_1.m_i32.property_changed().connect(
        [&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    obj->m_iface_1.m_i32.set_value(10);

    // 接口1的其他属性修改不会触发
    obj->m_iface_1.m_str.set_value("123");

    // 接口2的属性修改不会触发接口1的信号
    obj->m_iface_2.m_variant.set_value(100);

    mc::dict expected = {{"i32", 10}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_property_changed_sig_use_abstract_object)
{
    auto obj = test_object::create();
    service.register_object(obj);

    // 1：通过引擎从全局对象表里面找到对象
    auto  engine = mc::engine::get_engine();
    auto& table  = engine.get_object_table();

    // 2：通过路径全局对象表里面找到对象
    auto result = table.find_object(mc::engine::by_path::field == obj->get_object_path());
    EXPECT_EQ(result, obj.get());
    auto* res_obj = static_cast<mc::engine::abstract_object*>(result.get());

    mc::dict values;
    res_obj->property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    res_obj->set_property("i32", 10);
    auto value = res_obj->get_property("i32");
    EXPECT_EQ(value, 10);

    mc::dict expected = {{"i32", 10}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_object_reflect)
{
    auto obj = test_object::create();

    obj->m_iface_1.m_i32.set_value(20);
    obj->m_iface_1.m_str.set_value("123");
    obj->m_iface_1.m_vec      = std::vector<int>{1, 2, 3};
    obj->m_iface_1.m_normal_v = 100;

    obj->m_iface_2.m_variant.set_value(100);

    mc::variant var;
    mc::to_variant(*obj, var);

    mc::dict expected = {
        {"org.test.test_interface_1",
         mc::dict{{"i32", 20}, {"str", "123"}, {"vec", mc::variants{1, 2, 3}}, {"normal_v", 100}}},
        {"org.test.test_interface_2", mc::dict{{"variant", 100}}}};
    EXPECT_EQ(var.get_object(), expected) << var.to_string() << "\n"
                                          << expected.to_string();

    auto obj2 = test_object::create();
    mc::from_variant(var, *obj2);
    EXPECT_EQ(obj2->m_iface_1.m_i32, 20);
    EXPECT_EQ(obj2->m_iface_1.m_str, "123");
    EXPECT_EQ(obj2->m_iface_1.m_vec, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(obj2->m_iface_1.m_normal_v, 100);
    EXPECT_EQ(obj2->m_iface_2.m_variant, 100);
}

TEST_F(engine_test, test_managed_object)
{
    auto test_obj = create_object("/org/test");
    auto obj1     = create_object("/org/test/o1");
    auto obj2     = create_object("/org/test/o2");
    auto obj3     = create_object("/org/test/o3");
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/o3"}));

    // 先添加子对象中间空一节 middle 路径
    auto obj2_mid_a = create_object("/org/test/o2/middle/a");
    auto obj2_mid_b = create_object("/org/test/o2/middle/b");
    EXPECT_EQ(get_managed_objects(*obj2),
              (mc::variants{"/org/test/o2/middle/a", "/org/test/o2/middle/b"}));

    // 然后再添加 middle 对象，这样 obj2_mid_a、obj2_mid_b 都会被移动到 middle 对象下
    auto obj2_mid = create_object("/org/test/o2/middle");
    EXPECT_EQ(get_managed_objects(*obj2_mid),
              (mc::variants{"/org/test/o2/middle/a", "/org/test/o2/middle/b"}));
    EXPECT_EQ(get_managed_objects(*obj2), (mc::variants{"/org/test/o2/middle"}));

    // 虽然与 obj2_mid 有一样的前缀，但是并不是子路径
    auto obj2_mids = create_object("/org/test/o2/middles");
    EXPECT_EQ(get_managed_objects(*obj2_mid),
              (mc::variants{"/org/test/o2/middle/a", "/org/test/o2/middle/b"}));
    EXPECT_EQ(get_managed_objects(*obj2),
              (mc::variants{"/org/test/o2/middle", "/org/test/o2/middles"}));

    auto test_obj_new    = create_object("/bmc/dev");
    auto obj1_new        = create_object("/bmc/dev/Accessor/Accessor_1v8_0101010301");
    auto obj2_new        = create_object("/bmc/dev/Accessor/Accessor_0v8_0101010301");
    auto obj3_new        = create_object("/bmc/dev/Accessor/Accessor_1v2_0101010301");
    auto managed_objects = get_managed_objects(*test_obj_new);
    EXPECT_EQ(get_managed_objects(*test_obj_new),
              objects({"/bmc/dev/Accessor/Accessor_1v8_0101010301", "/bmc/dev/Accessor/Accessor_0v8_0101010301", "/bmc/dev/Accessor/Accessor_1v2_0101010301"}));
}

TEST_F(engine_test, test_managed_object_comprehensive)
{
    // ======= 基本场景测试 =======
    auto root_obj = create_object("/org");
    auto test_obj = create_object("/org/test");

    // 测试1：基本注册，找到合适的owner
    auto obj1 = create_object("/org/test/o1");
    auto obj2 = create_object("/org/test/o2");
    EXPECT_EQ(get_managed_objects(*test_obj), objects({"/org/test/o1", "/org/test/o2"}));
    EXPECT_EQ(get_managed_objects(*root_obj), objects({"/org/test"}));

    // ======= 层次结构调整测试 =======
    // 测试2：先添加深层子对象，再添加中间节点，验证层次结构调整
    auto deep_a = create_object("/org/test/deep/a/b/c");
    auto deep_b = create_object("/org/test/deep/a/b/d");
    EXPECT_EQ(
        get_managed_objects(*test_obj),
        objects({"/org/test/o1", "/org/test/o2", "/org/test/deep/a/b/c", "/org/test/deep/a/b/d"}));

    // 添加中间节点，应该会重新组织层次结构
    auto deep_ab = create_object("/org/test/deep/a/b");
    EXPECT_EQ(get_managed_objects(*deep_ab),
              objects({"/org/test/deep/a/b/c", "/org/test/deep/a/b/d"}));

    // 继续添加中间节点
    auto deep_a_obj = create_object("/org/test/deep/a");
    EXPECT_EQ(get_managed_objects(*deep_a_obj), objects({"/org/test/deep/a/b"}));
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/deep/a"}));

    // 最后添加顶层节点
    auto deep_obj = create_object("/org/test/deep");
    EXPECT_EQ(get_managed_objects(*deep_obj), objects({"/org/test/deep/a"}));
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/deep"}));

    // ======= 边界条件测试 =======
    // 测试3：路径前缀相同但不是父子关系的对象
    auto obj1_similar = create_object("/org/test/o11");
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/deep", "/org/test/o11"}));

    // 测试4：路径完全不相关的对象
    auto unrelated = create_object("/org/unrelated");
    EXPECT_EQ(get_managed_objects(*root_obj), objects({"/org/test", "/org/unrelated"}));

    // 测试5：添加已存在路径的对象（异常，不会注册成功）
    EXPECT_THROW(create_object("/org/test/o1"), mc::invalid_arg_exception);

    // ======= 对象删除及子对象重新分配测试 =======
    // 测试6：删除中间节点，子对象应重新分配
    auto sub1 = create_object("/org/sub/a");
    auto sub2 = create_object("/org/sub/a/b");
    auto sub3 = create_object("/org/sub/a/c");
    EXPECT_EQ(get_managed_objects(*root_obj),
              objects({"/org/test", "/org/unrelated", "/org/sub/a"}));
    EXPECT_EQ(get_managed_objects(*sub1), objects({"/org/sub/a/b", "/org/sub/a/c"}));

    // 删除中间节点
    service.unregister_object(sub1->get_object_path());

    // 验证子对象被重新分配给根节点
    EXPECT_EQ(get_managed_objects(*root_obj),
              objects({"/org/test", "/org/unrelated", "/org/sub/a/b", "/org/sub/a/c"}));

    // ======= 特殊路径测试 =======
    // 测试7：带有特殊字符的路径
    auto special1 = create_object("/org/test/special_dash///");
    auto special2 = create_object("/org/test/special_dot");
    auto special3 = create_object("/org/test/special_underscore");
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/deep", "/org/test/o11",
                       "/org/test/special_dash", "/org/test/special_dot",
                       "/org/test/special_underscore"}));

    // ======= 极端情况测试 =======
    // 测试8：超长路径
    std::string long_path =
        "/org/test/very/very/very/very/very/very/very/very/very/very/very/very/long/path";
    auto long_obj = create_object(long_path);
    EXPECT_TRUE(test_obj->get_managed_objects().count(long_path) != 0);

    // 测试9：重新注册对象到不同路径
    auto reuse_obj = create_object("/org/reuse1");
    EXPECT_TRUE(root_obj->get_managed_objects().count("/org/reuse1") != 0);

    // 更改路径后重新注册
    service.unregister_object("/org/reuse1");
    reuse_obj->set_object_path("/org/reuse2");
    service.register_object(reuse_obj);

    EXPECT_TRUE(root_obj->get_managed_objects().count("/org/reuse1") == 0);
    EXPECT_TRUE(root_obj->get_managed_objects().count("/org/reuse2") != 0);

    // 测试10：对象结构重组 - 从上到下构建
    // 先创建顶层节点
    auto rebuild_top = create_object("/org/rebuild");
    // 添加多个子节点
    auto rebuild_a = create_object("/org/rebuild/a");
    auto rebuild_b = create_object("/org/rebuild/b");
    auto rebuild_c = create_object("/org/rebuild/c");

    EXPECT_EQ(get_managed_objects(*rebuild_top),
              objects({"/org/rebuild/a", "/org/rebuild/b", "/org/rebuild/c"}));

    // 测试11：插入节点到已有结构中间
    auto insert_a1 = create_object("/org/rebuild/a/1");
    auto insert_a2 = create_object("/org/rebuild/a/2");
    EXPECT_EQ(get_managed_objects(*rebuild_a), objects({"/org/rebuild/a/1", "/org/rebuild/a/2"}));

    // 现在在a和1之间插入新节点
    auto insert_mid  = create_object("/org/rebuild/a/mid");
    auto insert_mid1 = create_object("/org/rebuild/a/mid/1");

    EXPECT_EQ(get_managed_objects(*rebuild_a),
              objects({"/org/rebuild/a/1", "/org/rebuild/a/2", "/org/rebuild/a/mid"}));
    EXPECT_EQ(get_managed_objects(*insert_mid), objects({"/org/rebuild/a/mid/1"}));
}

TEST(ServiceApiValidation, InitInvalidBusName)
{
    ::mc::engine::service invalid_service("invalid bus name");
    EXPECT_FALSE(invalid_service.init());
}

TEST_F(engine_test, ServiceLifecycleHooks)
{
    std::map<std::string, std::string> dump_context{{"phase", "collect"}};
    auto                               tmp_dir     = mc::filesystem::temp_directory_path();
    auto                               nonexistent = (tmp_dir / "nonexistent").string();
    service.on_dump(dump_context, nonexistent);
    service.on_detach_debug_console({});
    EXPECT_EQ(service.on_reboot_prepare({}), 0);
    EXPECT_EQ(service.on_reboot_process({}), 0);
    EXPECT_EQ(service.on_reboot_action({}), 0);
    service.on_reboot_cancel({});

    service.cleanup();
    EXPECT_TRUE(service.is_healthy());
}

TEST_F(engine_test, ServiceTimeoutCalls)
{
    auto obj = create_object("/org/openubmc/service_api/object");

    ::mc::variants args;
    args.emplace_back(4);
    args.emplace_back(6);

    auto result = service.timeout_call(
        ::mc::milliseconds(100), service.name(), obj->get_object_path(),
        obj->m_iface_1.get_interface_name(), "add_values", "ii", args);
    EXPECT_EQ(result.as_int32(), 10);

    // shm_timeout_call 在超时或服务不存在时会抛出异常，需要捕获
    try {
        auto opt = service.shm_timeout_call(::mc::milliseconds(10), "org.openubmc.undefined",
                                            obj->get_object_path(), obj->m_iface_1.get_interface_name(),
                                            "add_values", "ii", args);
        EXPECT_FALSE(opt.has_value());
    } catch (const std::exception& e) {
        // 超时异常是预期的，测试通过
        EXPECT_NE(std::string(e.what()).find("timeout"), std::string::npos);
    }
}

TEST_F(engine_test, ServiceMatchManagement)
{
    // 使用 std::string 确保字符串生命周期安全
    std::string member_name    = "PropertiesChanged";
    std::string interface_name = "org.freedesktop.DBus.Properties";
    // 直接传递 std::string，它们会自动转换为 string_view
    ::mc::dbus::match_rule rule = ::mc::dbus::match_rule::new_signal(member_name, interface_name);
    auto                   id   = service.add_match(rule, [](auto&) {
    });
    // get_rule_id() 从 0 开始，所以 id 可能为 0，这是有效的
    EXPECT_GE(id, 0U);
    service.remove_match(id);
}

TEST_F(engine_test, test_object_rewite_method)
{
    auto obj = create_object("/org/test");

    mc::engine::abstract_object* obj_ptr = obj.get();

    // 对象重载了该方法
    EXPECT_EQ(obj_ptr->invoke("rewite_method", {"111"}, "org.test.test_interface_1"),
              "test_object:rewite_method: 111");

    // 调用 not_rewite_method 方法
    EXPECT_THROW(obj_ptr->invoke("not_rewite_method", {"222"}, "org.test.test_interface_1"),
                 mc::method_call_exception);
}