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
#include <mc/engine.h>
#include <mc/singleton.h>
#include <mc/string.h>
#include <test_utilities/test_base.h>

namespace {

struct test_service : public mc::engine::service {
    test_service() : mc::engine::service("org.openubmc.test_service") {
    }
};

template <typename T>
using property = mc::engine::property<T>;

class test_interface_1 : public mc::engine::interface<test_interface_1> {
public:
    MC_INTERFACE("org.test.test_interface_1")

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

    test_interface_1 m_iface_1;
    test_interface_2 m_iface_2;
};

class engine_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override {
        service.init();
        service.start();
    }

    void TearDown() override {
        service.stop();
    }

    auto create_object(std::string_view path) {
        auto obj = test_object::create();

        int         object_id = m_object_id++;
        std::string object_name =
            mc::string::format_v("%s_%d", obj->get_class_name().data(), object_id);

        obj->set_object_path(path);
        obj->set_object_name(object_name);
        obj->set_position("0101");
        service.register_object(obj);
        return obj;
    };

    auto get_managed_objects(mc::engine::abstract_object& obj) {
        mc::variants var;
        for (auto [path, _] : obj.get_managed_objects()) {
            var.push_back(path);
        };
        return var;
    };

    auto objects(std::initializer_list<std::string_view> objs) {
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

} // namespace

MC_REFLECT(test_interface_1,
           ((m_i32, "i32"))((m_str, "str"))((m_vec, "vec"))((m_normal_v, "normal_v")))
MC_REFLECT(test_interface_2, ((m_variant, "variant")))
MC_REFLECT(test_object, ((m_iface_1, "iface_1"))((m_iface_2, "iface_2")))

TEST_F(engine_test, test_engine_dbus_connection) {
    auto strand = mc::engine::make_strand();
    auto conn   = mc::dbus::connection::open_session_bus(strand);
    conn->start();

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn->send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());

    std::set<std::string> names;
    reply >> names;
    EXPECT_GE(names.count("org.openubmc.test_service"), 1);
}

TEST_F(engine_test, test_object_property_changed_sig) {
    auto obj = test_object::create();
    obj->m_iface_1.m_i32.set_value(1);
    obj->set_object_id(1);
    service.register_object(obj);

    // 检查对象路径模板计算是否正确
    auto path = obj->get_object_path();
    EXPECT_EQ(path, "/org/test/object_1_101");

    mc::mutable_dict values;
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

    mc::dict expected = {{"i32", 10},     {"i32", 20},    {"str", "123"},
                         {"str", "456"},  {"str", "000"}, {"vec", mc::variants{1}},
                         {"variant", 100}};
    EXPECT_EQ(values, expected);
    EXPECT_EQ(str, obj->m_iface_1.m_str);
}

TEST_F(engine_test, test_interface_property_changed_sig) {
    auto obj = test_object::create();
    service.register_object(obj);

    mc::mutable_dict values;
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

TEST_F(engine_test, test_property_changed_sig) {
    auto obj = test_object::create();
    service.register_object(obj);

    // 指定订阅接口1的i32属性
    mc::mutable_dict values;
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

TEST_F(engine_test, test_property_changed_sig_use_abstract_object) {
    auto obj = test_object::create();
    service.register_object(obj);

    // 1：通过引擎从全局对象表里面找到对象
    auto  engine = mc::engine::get_engine();
    auto& table  = engine.get_object_table();

    // 2：通过路径全局对象表里面找到对象
    auto result = table.find_object(mc::engine::by_path::field == obj->get_object_path());
    EXPECT_EQ(result, obj.get());
    auto* res_obj = static_cast<mc::engine::abstract_object*>(result.get());

    mc::mutable_dict values;
    res_obj->property_changed().connect([&](const mc::variant& value, const auto& prop) {
        values[prop.get_name()] = value;
    });

    res_obj->set_property("i32", 10);
    auto value = res_obj->get_property("i32");
    EXPECT_EQ(value, 10);

    mc::dict expected = {{"i32", 10}};
    EXPECT_EQ(values, expected);
}

TEST_F(engine_test, test_object_reflect) {
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
    EXPECT_EQ(var.get_object(), expected) << var.to_string() << "\n" << expected.to_string();

    auto obj2 = test_object::create();
    mc::from_variant(var.get_object(), *obj2);
    EXPECT_EQ(obj2->m_iface_1.m_i32, 20);
    EXPECT_EQ(obj2->m_iface_1.m_str, "123");
    EXPECT_EQ(obj2->m_iface_1.m_vec, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(obj2->m_iface_1.m_normal_v, 100);
    EXPECT_EQ(obj2->m_iface_2.m_variant, 100);
}

TEST_F(engine_test, test_managed_object) {
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
}

TEST_F(engine_test, test_managed_object_comprehensive) {
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
    auto special1 = create_object("/org/test/special-dash");
    auto special2 = create_object("/org/test/special.dot");
    auto special3 = create_object("/org/test/special_underscore");
    EXPECT_EQ(get_managed_objects(*test_obj),
              objects({"/org/test/o1", "/org/test/o2", "/org/test/deep", "/org/test/o11",
                       "/org/test/special-dash", "/org/test/special.dot",
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