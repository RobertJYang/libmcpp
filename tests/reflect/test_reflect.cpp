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
#include <functional>
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;

// 测试用的颜色枚举
enum class test_color { RED, GREEN, BLUE };
MC_REFLECT_ENUM(test_color, (RED)(GREEN)(BLUE))

// 测试类
class test_person {
public:
    std::string m_name;
    int         m_age;
    bool        m_is_male;

    test_person() : m_name(""), m_age(0), m_is_male(false) {
    }
    test_person(const std::string& name, int age, bool is_male)
        : m_name(name), m_age(age), m_is_male(is_male) {
    }

    bool operator==(const test_person& other) const {
        return m_name == other.m_name && m_age == other.m_age && m_is_male == other.m_is_male;
    }
};
MC_REFLECT(test_person, (m_name)(m_age)(m_is_male))

template <typename T>
class member_visitor {
public:
    explicit member_visitor(const T& obj) : m_obj(obj) {
    }

    template <typename Getter, typename Setter>
    void operator()(std::string_view name, Getter&& getter, Setter&& setter) const {
        names.push_back(std::string(name));
        values.push_back(getter(m_obj));
    }

    mutable std::vector<std::string> names;
    mutable std::vector<variant>     values;

private:
    const T& m_obj;
};

// 测试类反射
TEST(ReflectTest, ClassReflection) {
    test_person p("张三", 30, true);

    // 检查类型是否可反射
    EXPECT_TRUE(mc::reflect::is_reflectable<test_person>());
    EXPECT_FALSE(mc::reflect::is_enum<test_person>());

    // 获取类型名称
    EXPECT_EQ(mc::reflect::reflector<test_person>::name(), "test_person");

    // 转换为变体
    variant var(p);

    // 检查变体类型
    EXPECT_TRUE(var.is_object());

    // 检查字典内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["m_name"], "张三");
    EXPECT_EQ(d["m_age"], 30);
    EXPECT_EQ(d["m_is_male"], true);

    // 从变体转回对象
    test_person p2 = var.as<test_person>();
    EXPECT_EQ(p2.m_name, "张三");
    EXPECT_EQ(p2.m_age, 30);
    EXPECT_EQ(p2.m_is_male, true);
    EXPECT_EQ(p, p2);
}

// 测试枚举反射
TEST(ReflectTest, EnumReflection) {
    // 检查类型是否可反射
    EXPECT_TRUE(mc::reflect::is_reflectable<test_color>());
    EXPECT_TRUE(mc::reflect::is_enum<test_color>());

    // 获取类型名称
    EXPECT_STREQ(mc::reflect::reflector<test_color>::name(), "test_color");

    // 枚举转变体
    test_color color = test_color::GREEN;
    variant    var(color);

    // 检查变体内容
    EXPECT_TRUE(var.is_string());
    EXPECT_EQ(var, "GREEN");

    // 变体转枚举
    test_color new_color = var.as<test_color>();
    EXPECT_EQ(new_color, test_color::GREEN);

    // 测试所有枚举值
    test_color red   = test_color::RED;
    test_color green = test_color::GREEN;
    test_color blue  = test_color::BLUE;

    variant var_red(red);
    variant var_green(green);
    variant var_blue(blue);

    EXPECT_EQ(var_red, "RED");
    EXPECT_EQ(var_green, "GREEN");
    EXPECT_EQ(var_blue, "BLUE");

    // 测试无效枚举值
    variant invalid_var = "YELLOW";
    EXPECT_THROW(invalid_var.as<test_color>(), bad_cast_exception);
}

// 测试成员访问
TEST(ReflectTest, MemberVisit) {
    // 创建测试对象
    test_person p("李四", 25, false);

    // 访问成员
    member_visitor<test_person> visitor(p);
    mc::reflect::visit_members<test_person>(visitor);

    // 检查成员名称
    EXPECT_EQ(visitor.names.size(), 3);
    EXPECT_EQ(visitor.names[0], "m_name");
    EXPECT_EQ(visitor.names[1], "m_age");
    EXPECT_EQ(visitor.names[2], "m_is_male");

    // 检查成员值
    EXPECT_EQ(visitor.values.size(), 3);
    EXPECT_EQ(visitor.values[0], "李四");
    EXPECT_EQ(visitor.values[1], 25);
    EXPECT_EQ(visitor.values[2], false);
}

// 测试部分更新对象
TEST(ReflectTest, PartialUpdate) {
    // 创建原始对象
    test_person p("张三", 30, true);

    // 使用初始化列表构造字典并更新对象
    from_variant(dict{{"m_age", 35}}, p);

    // 检查更新后的对象
    EXPECT_EQ(p.m_name, "张三");  // 未更新
    EXPECT_EQ(p.m_age, 35);       // 已更新
    EXPECT_EQ(p.m_is_male, true); // 未更新
}

// 测试嵌套对象
TEST(ReflectTest, NestedObjects) {
    // 创建测试对象
    test_person p1("张三", 30, true);
    test_person p2("李四", 25, false);

    // 使用初始化列表构造嵌套字典
    dict nested_dict{{"person1", p1}, {"person2", p2}};

    // 转换为变体
    variant var = nested_dict;

    // 检查变体内容
    const dict& d = var.as<dict>();
    EXPECT_EQ(d.size(), 2);

    // 从变体中提取对象
    test_person p1_extracted = d["person1"].as<test_person>();
    test_person p2_extracted = d["person2"].as<test_person>();

    // 检查提取的对象
    EXPECT_EQ(p1_extracted, p1);
    EXPECT_EQ(p2_extracted, p2);
}

// 测试反射与变体的互操作性
TEST(ReflectTest, VariantInteroperability) {
    // 创建测试对象
    test_person p("王五", 40, true);

    // 对象转变体
    variant var(p);

    // 变体转字典
    const dict& d = var.as<dict>();

    // 使用初始化列表构造修改后的字典
    dict         modified_dict = d;
    mutable_dict md(modified_dict);
    md["m_name"] = "赵六";
    md["m_age"]  = 45;

    // 字典转回对象
    test_person p2 = variant(md).as<test_person>();

    // 检查修改后的对象
    EXPECT_EQ(p2.m_name, "赵六");
    EXPECT_EQ(p2.m_age, 45);
    EXPECT_EQ(p2.m_is_male, true);
}

// 测试反射与序列化
TEST(ReflectTest, Serialization) {
    // 创建测试对象
    test_person p("张三", 30, true);

    // 对象转变体
    variant var(p);

    // 变体转字典
    const dict& d = var.as<dict>();

    // 模拟序列化：将字典转换为字符串表示
    std::string serialized = "{";
    bool        first      = true;
    for (const auto& key : d.keys()) {
        if (!first) {
            serialized += ", ";
        }
        first = false;

        const variant& value = d[key];
        serialized += "\"" + key + "\": ";

        if (value.is_string()) {
            serialized += "\"" + value.as<std::string>() + "\"";
        } else if (value.is_bool()) {
            serialized += value.as<bool>() ? "true" : "false";
        } else if (value.is_integer()) {
            serialized += std::to_string(value.as<int>());
        } else if (value.is_object()) {
            serialized += "{...}"; // 简化嵌套对象表示
        }
    }
    serialized += "}";

    // 检查序列化结果
    EXPECT_TRUE(serialized.find("\"m_name\": \"张三\"") != std::string::npos);
    EXPECT_TRUE(serialized.find("\"m_age\": 30") != std::string::npos);
    EXPECT_TRUE(serialized.find("\"m_is_male\": true") != std::string::npos);
}

// 测试复杂嵌套结构
TEST(ReflectTest, ComplexNestedStructure) {
    // 使用初始化列表构造复杂嵌套结构
    dict root{{"name", "复杂结构"},
              {"value", 42},
              {"level1", dict{{"key1", "value1"},
                              {"level2", dict{{"nested", true},
                                              {"color", test_color::BLUE},
                                              {"person", test_person("张三", 30, true)}}}}}};

    // 转换为变体
    variant var = root;

    // 检查结构
    const dict& d = var.as<dict>();
    EXPECT_EQ(d["name"], "复杂结构");
    EXPECT_EQ(d["value"], 42);

    const dict& l1 = d["level1"].as<dict>();
    EXPECT_EQ(l1["key1"], "value1");

    const dict& l2 = l1["level2"].as<dict>();
    EXPECT_EQ(l2["nested"], true);
    EXPECT_EQ(l2["color"], "BLUE");

    const dict& person_dict = l2["person"].as<dict>();
    EXPECT_EQ(person_dict["m_name"], "张三");
    EXPECT_EQ(person_dict["m_age"], 30);
    EXPECT_EQ(person_dict["m_is_male"], true);

    // 从嵌套结构中提取对象
    test_person extracted_person = l2["person"].as<test_person>();
    EXPECT_EQ(extracted_person.m_name, "张三");
    EXPECT_EQ(extracted_person.m_age, 30);
    EXPECT_EQ(extracted_person.m_is_male, true);

    // 从嵌套结构中提取枚举
    test_color extracted_color = l2["color"].as<test_color>();
    EXPECT_EQ(extracted_color, test_color::BLUE);
}
