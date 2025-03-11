/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file test_reflect.cpp
 * @brief 测试反射功能
 */
#include <gtest/gtest.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <string>
#include <vector>
#include <cstring>
#include <functional>

using namespace mc;
using namespace mc::reflect;

// 测试用的颜色枚举
enum class test_color {
    RED,
    GREEN,
    BLUE
};

// 成员访问器
template<typename T>
class member_visitor {
public:
    explicit member_visitor(const T& obj) : m_obj(obj) {}
    
    template<typename Getter, typename Setter>
    void operator()(const char* name, Getter&& getter, Setter&& setter) const {
        names.push_back(name);
    }
    
    mutable std::vector<std::string> names;
    
private:
    const T& m_obj;
};

// 测试类
class test_person {
public:
    std::string m_name;
    int m_age;
    bool m_is_male;
    
    test_person() : m_name(""), m_age(0), m_is_male(false) {}
    test_person(const std::string& name, int age, bool is_male)
        : m_name(name), m_age(age), m_is_male(is_male) {}
    
    bool operator==(const test_person& other) const {
        return m_name == other.m_name && m_age == other.m_age && m_is_male == other.m_is_male;
    }
};

// 使用反射宏注册类型
namespace mc {
namespace reflect {

template<>
struct reflector<test_person> {
    using is_defined = std::true_type;
    using is_enum = std::false_type;
    
    static const char* name() { return "test_person"; }
    
    template<typename Visitor>
    static void visit(Visitor&& visitor) {
        visitor("m_name", 
                [](const test_person& p) -> variant { return p.m_name; },
                [](test_person& p, const variant& v) { p.m_name = v.as<std::string>(); });
        visitor("m_age", 
                [](const test_person& p) -> variant { return p.m_age; },
                [](test_person& p, const variant& v) { p.m_age = v.as<int>(); });
        visitor("m_is_male", 
                [](const test_person& p) -> variant { return p.m_is_male; },
                [](test_person& p, const variant& v) { p.m_is_male = v.as<bool>(); });
    }
    
    static void to_variant(const test_person& p, mutable_dict& dict) {
        visit([&](const char* name, auto getter, auto) {
            dict[name] = getter(p);
        });
    }
    
    static void from_variant(const dict& d, test_person& p) {
        visit([&](const char* name, auto, auto setter) {
            if (d.contains(name)) {
                setter(p, d[name]);
            }
        });
    }
};

template<>
struct reflector<test_color> {
    using is_defined = std::true_type;
    using is_enum = std::true_type;
    
    static const char* name() { return "test_color"; }
    
    static void to_variant(const test_color& e, variant& var) {
        switch (e) {
            case test_color::RED:
                var = "RED";
                break;
            case test_color::GREEN:
                var = "GREEN";
                break;
            case test_color::BLUE:
                var = "BLUE";
                break;
            default:
                throw bad_enum_cast("无效的枚举值");
        }
    }
    
    static void from_variant(const variant& var, test_color& e) {
        const std::string& s = var.as<std::string>();
        if (s == "RED") {
            e = test_color::RED;
            return;
        }
        if (s == "GREEN") {
            e = test_color::GREEN;
            return;
        }
        if (s == "BLUE") {
            e = test_color::BLUE;
            return;
        }
        throw bad_enum_cast("无效的枚举字符串");
    }
};

} // namespace reflect
} // namespace mc

// 辅助函数
namespace test_helpers {

template<typename T>
void to_variant(const T& obj, variant& var) {
    if constexpr (mc::reflect::reflector<T>::is_enum::value) {
        mc::reflect::reflector<T>::to_variant(obj, var);
    } else {
        mc::mutable_dict dict;
        mc::reflect::reflector<T>::to_variant(obj, dict);
        var = variant(dict);
    }
}

template<typename T>
void from_variant(const variant& var, T& obj) {
    if constexpr (mc::reflect::reflector<T>::is_enum::value) {
        mc::reflect::reflector<T>::from_variant(var, obj);
    } else {
        const mc::dict& d = var.as<mc::dict>();
        mc::reflect::reflector<T>::from_variant(d, obj);
    }
}

template<typename T>
variant to_variant(const T& obj) {
    variant var;
    to_variant(obj, var);
    return var;
}

template<typename T>
T from_variant(const variant& var) {
    T obj;
    from_variant(var, obj);
    return obj;
}

} // namespace test_helpers

using namespace test_helpers;

// 测试类反射
TEST(ReflectTest, ClassReflection) {
    test_person p("张三", 30, true);
    
    // 转换为变体
    variant var;
    test_helpers::to_variant(p, var);
    
    // 检查变体类型
    EXPECT_TRUE(var.get_type() == variant::type_id::object_type);
    
    // 检查字典内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d["m_name"].as<std::string>(), "张三");
    EXPECT_EQ(d["m_age"].as<int>(), 30);
    EXPECT_EQ(d["m_is_male"].as<bool>(), true);
    
    // 从变体转回对象
    test_person p2;
    test_helpers::from_variant(var, p2);
    EXPECT_EQ(p2.m_name, "张三");
    EXPECT_EQ(p2.m_age, 30);
    EXPECT_EQ(p2.m_is_male, true);
}

// 测试对象转换为变体
TEST(ReflectTest, ToVariant) {
    test_person person("张三", 30, true);
    
    // 转换为变体
    variant var = test_helpers::to_variant(person);
    
    // 检查变体类型
    EXPECT_TRUE(var.get_type() == variant::type_id::object_type);
    
    // 获取字典并检查内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d.size(), 3);
    EXPECT_TRUE(d.contains("m_name"));
    EXPECT_TRUE(d.contains("m_age"));
    EXPECT_TRUE(d.contains("m_is_male"));
    
    // 检查字典值
    EXPECT_EQ(d["m_name"].as<std::string>(), "张三");
    EXPECT_EQ(d["m_age"].as<int>(), 30);
    EXPECT_EQ(d["m_is_male"].as<bool>(), true);
}

// 测试从变体转换为对象
TEST(ReflectTest, FromVariant) {
    // 创建字典
    mc::mutable_dict dict;
    dict("m_name", "李四")("m_age", 25)("m_is_male", false);
    
    // 从变体转换为对象
    test_person person;
    test_helpers::from_variant(dict, person);
    
    // 检查对象内容
    EXPECT_EQ(person.m_name, "李四");
    EXPECT_EQ(person.m_age, 25);
    EXPECT_EQ(person.m_is_male, false);
}

// 测试枚举反射
TEST(ReflectTest, EnumReflection) {
    // 枚举转变体
    test_color color = test_color::GREEN;
    variant var;
    test_helpers::to_variant(color, var);
    
    // 检查变体内容
    EXPECT_TRUE(var.get_type() == variant::type_id::string_type);
    EXPECT_EQ(var.as<std::string>(), "GREEN");
    
    // 变体转枚举
    test_color new_color;
    test_helpers::from_variant(var, new_color);
    EXPECT_EQ(new_color, test_color::GREEN);
    
    // 测试无效枚举值
    variant invalid_var = "YELLOW";
    EXPECT_THROW(test_helpers::from_variant(invalid_var, new_color), bad_enum_cast);
}

// 测试成员访问
TEST(ReflectTest, MemberVisit) {
    // 访问成员
    test_person p;
    member_visitor<test_person> visitor(p);
    mc::reflect::reflector<test_person>::visit(visitor);
    
    EXPECT_EQ(visitor.names.size(), 3);
    EXPECT_EQ(visitor.names[0], "m_name");
    EXPECT_EQ(visitor.names[1], "m_age");
    EXPECT_EQ(visitor.names[2], "m_is_male");
}
