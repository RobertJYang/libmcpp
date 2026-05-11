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
#include <map>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>

namespace test_static_info {
class test_user {
public:
    MC_REFLECTABLE("test_static_info.test_user");

    int         m_id;
    mc::string m_name;
    double      m_score;
};

class test_person {
public:
    MC_REFLECTABLE("test_static_info.test_person");

    int         m_id;
    mc::string m_name;
    int         m_age;
};
} // namespace test_static_info

MC_REFLECT(test_static_info::test_user, (m_id)(m_name)(m_score))
MC_REFLECT(test_static_info::test_person, ((m_id, "用户ID"))((m_name, "姓名"))((m_age, "年龄")))

using namespace test_static_info;

// 测试获取成员名称
TEST(ReflectStaticInfo, GetMemberName)
{
    // 使用命名空间限定
    mc::string_view name = mc::reflect::get_property_name<test_person>(&test_person::m_id);
    EXPECT_EQ(name, "用户ID");

    auto person_name_name = mc::reflect::get_property_name<test_person>(&test_person::m_name);
    EXPECT_EQ(person_name_name, "姓名");

    auto person_age_name = mc::reflect::get_property_name<test_person>(&test_person::m_age);
    EXPECT_EQ(person_age_name, "年龄");
}

// 测试将成员指针转换为名称，然后使用visit_members获取值
TEST(ReflectStaticInfo, MemberPtrToValueAccess)
{
    test_user user{1, "张三", 95.5};

    // 获取m_id成员的指针
    auto get_member_value = [&user](auto member_ptr) {
        mc::variant      result;
        mc::string_view member_name = mc::reflect::get_property_name<test_user>(member_ptr);

        // 使用成员名称通过visit_members获取值
        mc::reflect::visit_properties<test_user>([&](mc::string_view name, auto getter, auto) {
            if (name == member_name) {
                result = getter(user);
            }
        });

        return result;
    };

    // 测试各成员的值
    EXPECT_EQ(get_member_value(&test_user::m_id).as<int>(), 1);
    EXPECT_EQ(get_member_value(&test_user::m_name).as<mc::string>(), "张三");
    EXPECT_DOUBLE_EQ(get_member_value(&test_user::m_score).as<double>(), 95.5);
}

// 测试使用成员指针设置值
TEST(ReflectStaticInfo, SetValueByMemberPtr)
{
    test_user user{1, "张三", 95.5};

    // 设置成员值的函数
    auto set_member_value = [&user](auto member_ptr, const mc::variant& value) {
        mc::string_view member_name = mc::reflect::get_property_name<test_user>(member_ptr);

        mc::reflect::visit_properties<test_user>([&](mc::string_view name, auto, auto setter) {
            if (name == member_name) {
                setter(user, value);
            }
        });
    };

    // 测试设置各成员的值
    set_member_value(&test_user::m_id, 2);
    set_member_value(&test_user::m_name, mc::string("李四"));
    set_member_value(&test_user::m_score, 88.0);

    // 验证设置后的值
    EXPECT_EQ(user.m_id, 2);
    EXPECT_EQ(user.m_name, "李四");
    EXPECT_DOUBLE_EQ(user.m_score, 88.0);
}

// 测试通过属性名称获取对象属性值
TEST(ReflectStaticInfo, GetProperty)
{
    test_user user{10, "王五", 78.5};

    // 使用属性原始名称获取值
    EXPECT_EQ(mc::reflect::get_property(user, "m_id").as<int>(), 10);
    EXPECT_EQ(mc::reflect::get_property(user, "m_name").as<mc::string>(), "王五");
    EXPECT_DOUBLE_EQ(mc::reflect::get_property(user, "m_score").as<double>(), 78.5);

    // 测试获取不存在的属性
    EXPECT_TRUE(mc::reflect::get_property(user, "not_exist").is_null());

    // 测试自定义命名的属性
    test_person person{20, "赵六", 30};

    // 使用自定义名称获取值
    EXPECT_EQ(mc::reflect::get_property(person, "用户ID").as<int>(), 20);
    EXPECT_EQ(mc::reflect::get_property(person, "姓名").as<mc::string>(), "赵六");
    EXPECT_EQ(mc::reflect::get_property(person, "年龄").as<int>(), 30);
}

// 测试通过属性名称设置对象属性值
TEST(ReflectStaticInfo, SetProperty)
{
    test_user user{10, "王五", 78.5};

    // 设置属性值
    EXPECT_TRUE(mc::reflect::set_property(user, "m_id", 11));
    EXPECT_TRUE(mc::reflect::set_property(user, "m_name", mc::string("张三丰")));
    EXPECT_TRUE(mc::reflect::set_property(user, "m_score", 99.9));

    // 验证设置结果
    EXPECT_EQ(user.m_id, 11);
    EXPECT_EQ(user.m_name, "张三丰");
    EXPECT_DOUBLE_EQ(user.m_score, 99.9);

    // 测试设置不存在的属性
    EXPECT_FALSE(mc::reflect::set_property(user, "not_exist", 100));

    // 测试类型不匹配的情况
    EXPECT_FALSE(mc::reflect::set_property(user, "m_id", "字符串"));

    // 测试自定义命名的属性
    test_person person{20, "赵六", 30};

    // 使用自定义名称设置
    EXPECT_TRUE(mc::reflect::set_property(person, "用户ID", 22));
    EXPECT_TRUE(mc::reflect::set_property(person, "姓名", mc::string("李世民")));
    EXPECT_TRUE(mc::reflect::set_property(person, "年龄", 40));

    // 验证设置结果
    EXPECT_EQ(person.m_id, 22);
    EXPECT_EQ(person.m_name, "李世民");
    EXPECT_EQ(person.m_age, 40);
}

// 测试组合使用get_property和set_property
TEST(ReflectStaticInfo, CombinedPropertyAccess)
{
    test_user source{100, "源用户", 88.8};
    test_user target{0, "", 0.0};

    // 从源对象获取属性并设置到目标对象
    for (const auto& name : {"m_id", "m_name", "m_score"}) {
        auto value = mc::reflect::get_property(source, name);
        if (!value.is_null()) {
            EXPECT_TRUE(mc::reflect::set_property(target, name, value));
        }
    }

    // 验证复制结果
    EXPECT_EQ(target.m_id, 100);
    EXPECT_EQ(target.m_name, "源用户");
    EXPECT_DOUBLE_EQ(target.m_score, 88.8);
}