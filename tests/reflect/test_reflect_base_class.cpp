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
#include <mc/reflect.h>
#include <test_utilities/test_base.h>

namespace test {
class test_person_base {
public:
    int         m_id;
    std::string m_name;

    void set_id(int id) {
        m_id = id;
    }

    int get_id() const {
        return m_id;
    }
};
} // namespace test

namespace test_reflect_base_class {

class test_person : public ::test::test_person_base {
public:
    int m_age;

    void set_id(int id) {
        m_id = id;
    }

    int get_id() const {
        return m_id;
    }
};

class test_company {
public:
    std::string m_name;
    std::string m_address;
    int         m_employee_count;
};

class test_user : public test_person, public test_company {
public:
    void set_score(double score) {
        m_score = score;
    }

    double get_score() const {
        return m_score;
    }
    double m_score;
};
} // namespace test_reflect_base_class

MC_REFLECT(::test::test_person_base,
           ((m_id, "Id"))((m_name, "Name"))((set_id, "SetId"))((get_id, "GetId")))

// 在不同命名空间的基类，基类名称会保留完整名称，为了使用方便定义别名
MC_REFLECT(test_reflect_base_class::test_person,
           ((::test::test_person_base, "test_person_base")), ((m_age, "Age")))

MC_REFLECT(test_reflect_base_class::test_company,
           ((m_name, "Name"))((m_address, "Address"))((m_employee_count, "EmployeeCount")))

// 在同一个命名空间的基类，基类名称会去掉公共命名空间
MC_REFLECT(test_reflect_base_class::test_user,
           (test_reflect_base_class::test_company)(test_reflect_base_class::test_person),
           ((m_score, "Score"))((set_score, "SetScore"))((get_score, "GetScore")))

namespace test_reflect_base_class {

class reflect_base_class_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        user.m_id                 = 1;
        user.test_person::m_name  = "John";
        user.test_company::m_name = "Company";
        user.m_address            = "Beijing";
        user.m_employee_count     = 10;
        user.m_age                = 20;
        user.m_score              = 95.5;
    }

    void TearDown() override {
    }

    test_user user;
};

TEST_F(reflect_base_class_test, TestGetProperties) {
    // 获取属性，对于同名属性，会返回反射类型的第一个该名称的属性
    auto name = mc::reflect::get_property(user, "Name");
    EXPECT_EQ(name, "Company");

    // 指定基类名称访问属性（实际是访问 test_person_base 的属性）
    auto person_name = mc::reflect::get_property(user, "Name", "test_person");
    EXPECT_EQ(person_name, "John");

    // 精确指定该属性属于哪个基类
    auto person_base_name =
        mc::reflect::get_property(user, "Name", "test_person::test_person_base");
    EXPECT_EQ(person_base_name, "John");

    // 指定另一个具有同名属性的基类
    auto company_name = mc::reflect::get_property(user, "Name", "test_company");
    EXPECT_EQ(company_name, "Company");

    // 获取所有属性，对于同名属性，会返回反射类型的第一个该名称的属性
    mc::dict expected = {
        {"Id", 1},
        {"Name", "Company"},
        {"Age", 20},
        {"Score", 95.5},
        {"EmployeeCount", 10},
        {"Address", "Beijing"},
    };
    auto properties = mc::reflect::get_all_properties(user);
    EXPECT_EQ(properties, expected);
}

TEST_F(reflect_base_class_test, TestSetProperties) {
    // 获取属性，对于同名属性，会返回反射类型的第一个该名称的属性
    EXPECT_EQ(mc::reflect::get_property(user, "Name"), "Company");
    mc::reflect::set_property(user, "Name", "Company1");
    EXPECT_EQ(mc::reflect::get_property(user, "Name"), "Company1");

    // 指定基类名称访问属性（实际是访问 test_person_base 的属性）
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_person"), "John");
    mc::reflect::set_property(user, "Name", "test_person", "John1");
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_person"), "John1");

    // 精确指定该属性属于哪个基类
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_person::test_person_base"), "John1");
    mc::reflect::set_property(user, "Name", "test_person::test_person_base", "John2");
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_person::test_person_base"), "John2");

    // 指定另一个具有同名属性的基类
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_company"), "Company1");
    mc::reflect::set_property(user, "Name", "test_company", "Company2");
    EXPECT_EQ(mc::reflect::get_property(user, "Name", "test_company"), "Company2");

    mc::dict expected = {
        {"Id", 1},
        {"Name", "Company2"},
        {"Age", 20},
        {"Score", 95.5},
        {"EmployeeCount", 10},
        {"Address", "Beijing"},
    };
    auto properties = mc::reflect::get_all_properties(user);
    EXPECT_EQ(properties, expected);
}

TEST_F(reflect_base_class_test, TestVariant) {
    mc::variant var;
    mc::to_variant(user, var);
    mc::dict expected = {
        {"Id", 1},
        {"Name", "Company"},
        {"Age", 20},
        {"Score", 95.5},
        {"EmployeeCount", 10},
        {"Address", "Beijing"},
    };
    EXPECT_EQ(var, expected);

    test_user user2;
    mc::from_variant(var, user2);
    EXPECT_EQ(mc::variant(user2), mc::variant(user));

    mc::variant base_var1;
    mc::to_variant(static_cast<test_person&>(user), base_var1);
    EXPECT_EQ(base_var1, (mc::dict{{"Id", 1}, {"Name", "John"}, {"Age", 20}}));
}

} // namespace test_reflect_base_class
