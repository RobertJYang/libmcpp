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
#include <mc/reflect/reflect.h>
#include <mc/reflect/reflection_factory.h>
#include <test_utilities/test_base.h>

namespace test_reflection_factory {

// 测试用的简单类
class test_person {
public:
    test_person() = default;
    test_person(std::string name, int age) : m_name(std::move(name)), m_age(age) {
    }

    int get_age() const {
        return m_age;
    }
    void set_age(int age) {
        m_age = age;
    }

    std::string greet() const {
        return "Hello, I'm " + m_name + " and I'm " + std::to_string(m_age) + " years old.";
    }

    std::string greet_with(const std::string& prefix) const {
        return prefix + ": " + greet();
    }

private:
    friend struct mc::reflect::reflector<test_person>;

    std::string m_name;
    int         m_age = 0;
};

} // namespace test_reflection_factory

// 注册反射信息
MC_REFLECT((test_reflection_factory::test_person, "FactoryPerson"),
           ((m_name, "name"))(MC_COMPUTED_PROPERTY("age", get_age, set_age))((greet, "greet"))((greet_with, "greetWith")))

namespace test_reflection_factory {

class reflect_factory_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 为了测试销毁反射元数据，这里显示获取一次单例确保被销毁后还能重新创建，否则其他用例会失败
        mc::reflect::reflection_metadata<test_person>::instance();
    }

    void TearDown() override {
    }
};

TEST_F(reflect_factory_test, TypeRegistration) {
    // 测试类型ID分配
    EXPECT_NE(mc::reflect::reflector<test_person>::type_id, -1);

    // 测试类型名获取
    auto type_name = mc::reflect::reflector<test_person>::name();
    EXPECT_EQ(type_name, "FactoryPerson");
}

TEST_F(reflect_factory_test, FactoryBasicOperations) {
    auto& factory = mc::reflect::reflection_factory::instance();

    // 测试类型ID查询
    auto type_id = factory.get_type_id("FactoryPerson");
    EXPECT_NE(type_id, -1);
    EXPECT_EQ(type_id, mc::reflect::reflector<test_person>::type_id);

    // 测试获取已注册类型
    auto types = factory.get_registered_types();
    bool found = false;
    for (const auto& name : types) {
        if (name == "FactoryPerson") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(reflect_factory_test, ObjectCreation) {
    // 通过类型名创建对象
    auto obj1 = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(obj1->get_type_id(), mc::reflect::reflector<test_person>::type_id);

    // 通过类型ID创建对象
    auto type_id = mc::reflect::get_type_id("FactoryPerson");
    auto obj2    = mc::reflect::create_object(type_id);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->get_type_id(), type_id);
}

TEST_F(reflect_factory_test, PropertyAccess) {
    auto obj = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj, nullptr);

    // 设置属性
    obj->set_property("name", mc::variant("Alice"));
    obj->set_property("age", mc::variant(25));

    // 获取属性
    auto name = obj->get_property("name");
    auto age  = obj->get_property("age");

    EXPECT_EQ(name, mc::variant("Alice"));
    EXPECT_EQ(age, mc::variant(25));
}

TEST_F(reflect_factory_test, MethodInvocation) {
    auto obj = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj, nullptr);

    // 设置基础数据
    obj->set_property("name", mc::variant("Bob"));
    obj->set_property("age", mc::variant(30));

    // 调用无参数方法
    auto greeting = obj->invoke_method("greet", {});
    EXPECT_EQ(greeting.as<std::string>(), "Hello, I'm Bob and I'm 30 years old.");

    // 调用有参数的方法
    std::vector<mc::variant> args                 = {mc::variant("Hi")};
    auto                     greeting_with_prefix = obj->invoke_method("greetWith", args);
    EXPECT_EQ(greeting_with_prefix.as<std::string>(), "Hi: Hello, I'm Bob and I'm 30 years old.");
}

TEST_F(reflect_factory_test, ObjectWrapping) {
    // 创建普通对象
    auto person = std::make_shared<test_person>("Charlie", 35);

    // 包装为反射对象
    auto reflected = mc::reflect::wrap_object(person);
    ASSERT_NE(reflected, nullptr);

    // 测试属性访问
    auto name = reflected->get_property("name");
    auto age  = reflected->get_property("age");

    EXPECT_EQ(name.as<std::string>(), "Charlie");
    EXPECT_EQ(age.as<int>(), 35);

    // 测试方法调用
    auto greeting = reflected->invoke_method("greet", {});
    EXPECT_EQ(greeting.as<std::string>(), "Hello, I'm Charlie and I'm 35 years old.");

    greeting = reflected->async_invoke_method("greet", {}).get();
    EXPECT_EQ(greeting.as<std::string>(), "Hello, I'm Charlie and I'm 35 years old.");
}

TEST_F(reflect_factory_test, PropertyAndMethodListing) {
    auto obj = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj, nullptr);

    // 获取属性列表
    auto prop_names = obj->get_property_names();
    EXPECT_GE(prop_names.size(), 2); // 至少有 name 和 age

    bool has_name = false, has_age = false;
    for (const auto& name : prop_names) {
        if (name == "name") {
            has_name = true;
        }
        if (name == "age") {
            has_age = true;
        }
    }
    EXPECT_TRUE(has_name);
    EXPECT_TRUE(has_age);

    // 获取方法列表
    auto method_names = obj->get_method_names();
    EXPECT_GE(method_names.size(), 2);

    bool has_greet = false;
    for (const auto& name : method_names) {
        if (name == "greet") {
            has_greet = true;
        }
    }
    EXPECT_TRUE(has_greet);
}

TEST_F(reflect_factory_test, ErrorHandling) {
    auto obj = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj, nullptr);

    // 测试不存在的属性访问
    EXPECT_THROW(obj->get_property("nonexistent"), mc::bad_property_exception);
    EXPECT_THROW(obj->set_property("nonexistent", mc::variant(42)), mc::bad_property_exception);

    // 测试不存在的方法调用
    EXPECT_THROW(obj->invoke_method("nonexistent", {}), mc::bad_method_exception);

    // 测试不存在的类型创建
    EXPECT_THROW(mc::reflect::create_object("nonexistent_type"), mc::bad_type_exception);
}

TEST_F(reflect_factory_test, TestDestoryReflectedMetaData) {
    // 创建对象成功
    auto obj = mc::reflect::create_object("FactoryPerson");
    ASSERT_NE(obj, nullptr);

    // 测试反射元数据被销毁后，创建对象会失败
    mc::singleton<std::shared_ptr<mc::reflect::reflection_metadata<test_person>>>::reset_for_test();
    EXPECT_THROW(mc::reflect::create_object("FactoryPerson"), mc::bad_type_exception);
}

} // namespace test_reflection_factory