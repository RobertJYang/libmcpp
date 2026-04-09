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

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect.h>
#include <mc/variant.h>

#include <mc/log/log_manager.h>

#include <functional>
#include <string>
#include <vector>

namespace test_reflect {
// 测试用的颜色枚举
enum class test_color { RED, GREEN, BLUE };

enum class test_normal_color { NORMAL_RED, NORMAL_GREEN, NORMAL_BLUE };

// 测试类
class test_person {
public:
    MC_REFLECTABLE("test_person");

    std::string m_name;
    int         m_age;
    bool        m_is_male;

    test_person() : m_name(""), m_age(0), m_is_male(false)
    {}
    test_person(const std::string& name, int age, bool is_male) : m_name(name), m_age(age), m_is_male(is_male)
    {}

    bool operator==(const test_person& other) const
    {
        return m_name == other.m_name && m_age == other.m_age && m_is_male == other.m_is_male;
    }

    int id{1};
    int get_id() const
    {
        return id;
    }

    void set_id(int id)
    {
        this->id = id;
    }

    int get_readonly_id() const
    {
        return id;
    }
};
} // namespace test_reflect

MC_REFLECTABLE("test_reflect.test_color", test_reflect::test_color);

// 重命名类名和枚举名
MC_REFLECT_ENUM(test_reflect::test_color, (BLUE)(RED)(GREEN))
MC_REFLECT(test_reflect::test_person,
           (m_name)(m_age)(m_is_male)(MC_COMPUTED_PROPERTY("id", get_id, set_id))(MC_COMPUTED_PROPERTY("readonly_id",
                                                                                                       get_id)))

template <typename C>
struct property_info_base_test {
    std::string_view name;
    constexpr property_info_base_test(std::string_view n) : name(n)
    {}

    virtual std::type_index typeinfo() const = 0;
};

template <typename C, typename M, typename BaseT = C>
struct property_info_test : public property_info_base_test<C> {
    using class_type  = C;
    using member_type = M;
    using base_type   = BaseT;

    M BaseT::*member_ptr;

    constexpr property_info_test(std::string_view n, M BaseT::*ptr) : property_info_base_test<C>(n), member_ptr(ptr)
    {}

    virtual std::type_index typeinfo() const override
    {
        return typeid(member_type);
    }
};

namespace test_reflect {

template <typename T>
class member_visitor {
public:
    explicit member_visitor(const T& obj) : m_obj(obj)
    {}

    template <typename Getter, typename Setter>
    void operator()(std::string_view name, Getter&& getter, Setter&& setter) const
    {
        names.push_back(std::string(name));
        values.push_back(getter(m_obj));
    }

    mutable std::vector<std::string> names;
    mutable std::vector<mc::variant> values;

private:
    const T& m_obj;
};

// 测试类反射
TEST(ReflectTest, ClassReflection)
{
    test_person p("张三", 30, true);

    // 检查类型是否可反射
    EXPECT_TRUE(mc::reflect::is_reflectable<test_person>());
    EXPECT_FALSE(mc::reflect::is_enum<test_person>());

    // 获取类型名称，test_person 重命名，不需要命名空间
    EXPECT_EQ(mc::reflect::reflector<test_person>::get_name(), "test_person");

    // 转换为变体
    mc::variant var(p);

    // 检查变体类型
    EXPECT_TRUE(var.is_object());

    // 检查字典内容
    const mc::dict& d = var.as<mc::dict>();
    ASSERT_EQ(d.size(), 5);
    EXPECT_EQ(d["m_name"], "张三");
    EXPECT_EQ(d["m_age"], 30);
    EXPECT_EQ(d["m_is_male"], true);
    EXPECT_EQ(d["id"], 1);
    EXPECT_EQ(d["readonly_id"], 1);

    // 从变体转回对象
    test_person p2 = var.as<test_person>();
    EXPECT_EQ(p2.m_name, "张三");
    EXPECT_EQ(p2.m_age, 30);
    EXPECT_EQ(p2.m_is_male, true);
    EXPECT_EQ(p2.id, 1);
    EXPECT_EQ(p, p2);

    auto id_val = mc::reflect::get_property<test_person>(p2, "id");
    EXPECT_EQ(id_val, 1);
    mc::reflect::set_property<test_person>(p2, "id", 2);
    EXPECT_EQ(p2.id, 2);

    auto readonly_id_val = mc::reflect::get_property<test_person>(p2, "readonly_id");
    EXPECT_EQ(readonly_id_val, 2);

    // 没有 setter 的属性不能设置值
    mc::reflect::set_property<test_person>(p2, "readonly_id", 3);
    EXPECT_EQ(p2.id, 2);
}

// 测试枚举反射
TEST(ReflectTest, EnumReflection)
{
    // 检查类型是否可反射
    EXPECT_TRUE(mc::reflect::is_reflectable<test_color>());
    EXPECT_TRUE(mc::reflect::is_enum<test_color>());
    EXPECT_FALSE(mc::reflect::is_normal_enum<test_color>());

    // 获取类型名称
    EXPECT_EQ(mc::reflect::reflector<test_color>::get_name(), "test_reflect.test_color");

    // 枚举转变体
    test_color  color = test_color::GREEN;
    mc::variant var(color);

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

    mc::variant var_red(red);
    mc::variant var_green(green);
    mc::variant var_blue(blue);

    EXPECT_EQ(var_red, "RED");
    EXPECT_EQ(var_green, "GREEN");
    EXPECT_EQ(var_blue, "BLUE");

    // 测试无效枚举值
    mc::variant invalid_var = "YELLOW";
    EXPECT_THROW(invalid_var.as<test_color>(), mc::bad_cast_exception);
}

// 测试普通枚举反射
TEST(ReflectTest, NormalEnumReflection)
{
    // 检查类型是否可反射
    EXPECT_FALSE(mc::reflect::is_reflectable<test_normal_color>());
    EXPECT_TRUE(mc::reflect::is_normal_enum<test_normal_color>());

    // 枚举转变体
    test_normal_color color = test_normal_color::NORMAL_GREEN;
    mc::variant       var(color);

    // 检查变体内容
    EXPECT_TRUE(var.is_integer());
    EXPECT_EQ(var, 1);

    // 变体转枚举
    test_normal_color new_color = var.as<test_normal_color>();
    EXPECT_EQ(new_color, test_normal_color::NORMAL_GREEN);

    // 测试所有枚举值
    test_normal_color red   = test_normal_color::NORMAL_RED;
    test_normal_color green = test_normal_color::NORMAL_GREEN;
    test_normal_color blue  = test_normal_color::NORMAL_BLUE;

    mc::variant var_red(red);
    mc::variant var_green(green);
    mc::variant var_blue(blue);

    EXPECT_EQ(var_red, 0);
    EXPECT_EQ(var_green, 1);
    EXPECT_EQ(var_blue, 2);

    EXPECT_EQ(mc::reflect::get_signature<test_normal_color>(), "i");
}

// 测试成员访问
TEST(ReflectTest, MemberVisit)
{
    // 创建测试对象
    test_person p("李四", 25, false);

    // 访问成员
    member_visitor<test_person> visitor(p);
    mc::reflect::visit_properties<test_person>(visitor);

    // 检查成员名称
    ASSERT_EQ(visitor.names.size(), 5);
    EXPECT_EQ(visitor.names[0], "m_name");
    EXPECT_EQ(visitor.names[1], "m_age");
    EXPECT_EQ(visitor.names[2], "m_is_male");
    EXPECT_EQ(visitor.names[3], "id");
    EXPECT_EQ(visitor.names[4], "readonly_id");
    // 检查成员值
    ASSERT_EQ(visitor.values.size(), 5);
    EXPECT_EQ(visitor.values[0], "李四");
    EXPECT_EQ(visitor.values[1], 25);
    EXPECT_EQ(visitor.values[2], false);
    EXPECT_EQ(visitor.values[3], 1);
    EXPECT_EQ(visitor.values[4], 1);
}

// 测试部分更新对象
TEST(ReflectTest, PartialUpdate)
{
    // 创建原始对象
    test_person p("张三", 30, true);

    // 使用初始化列表构造字典并更新对象
    from_variant(mc::dict{{"m_age", 35}}, p);

    // 检查更新后的对象
    EXPECT_EQ(p.m_name, "张三");  // 未更新
    EXPECT_EQ(p.m_age, 35);       // 已更新
    EXPECT_EQ(p.m_is_male, true); // 未更新
}

// 测试嵌套对象
TEST(ReflectTest, NestedObjects)
{
    // 创建测试对象
    test_person p1("张三", 30, true);
    test_person p2("李四", 25, false);

    // 使用初始化列表构造嵌套字典
    mc::dict nested_dict{{"person1", p1}, {"person2", p2}};

    // 转换为变体
    mc::variant var = nested_dict;

    // 检查变体内容
    const mc::dict& d = var.as<mc::dict>();
    EXPECT_EQ(d.size(), 2);

    // 从变体中提取对象
    test_person p1_extracted = d["person1"].as<test_person>();
    test_person p2_extracted = d["person2"].as<test_person>();

    // 检查提取的对象
    EXPECT_EQ(p1_extracted, p1);
    EXPECT_EQ(p2_extracted, p2);
}

// 测试反射与变体的互操作性
TEST(ReflectTest, VariantInteroperability)
{
    // 创建测试对象
    test_person p("王五", 40, true);

    // 对象转变体
    mc::variant var(p);

    // 变体转字典
    const mc::dict& d = var.as<mc::dict>();

    // 使用初始化列表构造修改后的字典
    mc::dict modified_dict = d;
    mc::dict md(modified_dict);
    md["m_name"] = "赵六";
    md["m_age"]  = 45;

    // 字典转回对象
    test_person p2 = mc::variant(md).as<test_person>();

    // 检查修改后的对象
    EXPECT_EQ(p2.m_name, "赵六");
    EXPECT_EQ(p2.m_age, 45);
    EXPECT_EQ(p2.m_is_male, true);
}

// 测试反射与序列化
TEST(ReflectTest, Serialization)
{
    // 创建测试对象
    test_person p("张三", 30, true);

    // 对象转变体
    mc::variant var(p);

    // 变体转字典
    const mc::dict& d = var.as<mc::dict>();

    // 模拟序列化：将字典转换为字符串表示
    std::string serialized = "{";
    bool        first      = true;
    for (const auto& key : d.keys()) {
        if (!first) {
            serialized += ", ";
        }
        first = false;

        const mc::variant& value = d[key];
        serialized += "\"" + key.get_string() + "\": ";

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
TEST(ReflectTest, ComplexNestedStructure)
{
    // 使用初始化列表构造复杂嵌套结构
    mc::dict root{{"name", "复杂结构"},
                  {"value", 42},
                  {"level1", mc::dict{{"key1", "value1"},
                                      {"level2", mc::dict{{"nested", true},
                                                          {"color", test_color::BLUE},
                                                          {"person", test_person("张三", 30, true)}}}}}};

    // 转换为变体
    mc::variant var = root;

    // 检查结构
    const mc::dict& d = var.as<mc::dict>();
    EXPECT_EQ(d["name"], "复杂结构");
    EXPECT_EQ(d["value"], 42);

    const mc::dict& l1 = d["level1"].as<mc::dict>();
    EXPECT_EQ(l1["key1"], "value1");

    const mc::dict& l2 = l1["level2"].as<mc::dict>();
    EXPECT_EQ(l2["nested"], true);
    EXPECT_EQ(l2["color"], "BLUE");

    const mc::dict& person_dict = l2["person"].as<mc::dict>();
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

// 测试类型名称验证函数
TEST(ReflectTest, TypeNameValidation)
{
    // 测试有效的类型名称
    // 单一类型名
    EXPECT_TRUE(mc::reflect::is_valid_type_name("Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("_Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("sensor_1"));

    // 点号分隔符
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc.devices.Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc.devices.sensors.TemperatureSensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("a.b.c"));

    // 双冒号分隔符
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc::devices::Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc::devices::sensors::TemperatureSensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("a::b::c"));

    // 混合分隔符
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc::devices.Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc.devices::Sensor"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc.core::services.manager.TaskManager"));

    // 测试无效的类型名称
    // 空字符串
    EXPECT_FALSE(mc::reflect::is_valid_type_name(""));

    // 以分隔符开头
    EXPECT_FALSE(mc::reflect::is_valid_type_name(".abc"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("::abc"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name(":abc"));

    // 以分隔符结尾
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc."));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc::"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc:"));

    // 连续分隔符
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc..def"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc::::def"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc.::def"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc::.def"));

    // 单独的冒号
    EXPECT_FALSE(mc::reflect::is_valid_type_name("abc:def"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("a:b:c"));

    // 段名以数字开头
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc.123abc"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("123abc"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc::1abc"));

    // 非法字符
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc.ab-c"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc.ab@c"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc::ab#c"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("mc.ab c"));

    // 超长名称
    std::string long_name(256, 'a');
    EXPECT_FALSE(mc::reflect::is_valid_type_name(long_name));
}

// 测试 to_variant 直接转换为 variant（会创建 dict）
TEST(ReflectTest, ToVariantCreatesDict)
{
    test_person p("test", 42, true);

    // 测试 to_variant 转换为 variant（会创建 dict）
    mc::variant var;
    mc::reflect::to_variant(p, var);

    EXPECT_TRUE(var.is_object());
    const mc::dict& d = var.get_object();
    EXPECT_EQ(d["m_name"], "test");
    EXPECT_EQ(d["m_age"], 42);
    EXPECT_EQ(d["m_is_male"], true);
}

// 测试 to_variant 直接转换为 dict
TEST(ReflectTest, ToVariantDirectToDict)
{
    test_person p("direct_dict", 100, false);

    // 测试 to_variant 直接转换为 dict
    mc::dict dict;
    mc::reflect::to_variant(p, dict);

    EXPECT_EQ(dict["m_name"], "direct_dict");
    EXPECT_EQ(dict["m_age"], 100);
    EXPECT_EQ(dict["m_is_male"], false);
}

// 测试 from_variant 从数组转换为对象
TEST(ReflectTest, FromVariantFromArray)
{
    // 创建数组 variant，按照反射顺序：[m_name, m_age, m_is_male, id, readonly_id]
    // 注意：test_person 有 5 个属性，但 id 和 readonly_id 是计算属性，可能不需要
    // 实际上只有 m_name, m_age, m_is_male 是成员变量
    mc::variants arr = {"array_name", 200, true};
    mc::variant  var(arr);

    // 从数组转换为对象
    test_person p;
    mc::reflect::from_variant(var, p);

    EXPECT_EQ(p.m_name, "array_name");
    EXPECT_EQ(p.m_age, 200);
    EXPECT_EQ(p.m_is_male, true);
}

// 测试 from_variant 从数组转换为对象（数组长度不足）
TEST(ReflectTest, FromVariantFromArrayIncomplete)
{
    // 创建不完整的数组 variant
    mc::variants arr = {"partial_name", 300};
    mc::variant  var(arr);

    // 从数组转换为对象（缺少 m_is_male）
    test_person p("original", 0, true);
    mc::reflect::from_variant(var, p);

    EXPECT_EQ(p.m_name, "partial_name");
    EXPECT_EQ(p.m_age, 300);
    // m_is_male 应该保持原值，因为数组中没有提供
    EXPECT_EQ(p.m_is_male, true);
}

// 测试 from_variant 从数组转换为对象（数组长度超出）
TEST(ReflectTest, FromVariantFromArrayExcess)
{
    // 创建超出长度的数组 variant
    mc::variants arr = {"excess_name", 400, true, "extra1", 999};
    mc::variant  var(arr);

    // 从数组转换为对象，第 4 个元素 "extra1" 无法转换为 int (id 的类型)，应该抛出异常
    test_person p;
    EXPECT_THROW(mc::reflect::from_variant(var, p), mc::invalid_arg_exception);
}

// 测试 from_variant 从无效类型转换
TEST(ReflectTest, FromVariantInvalidType)
{
    test_person p;

    // 尝试从字符串 variant 转换为对象（应该抛出异常）
    mc::variant var("invalid_string");
    EXPECT_THROW(mc::reflect::from_variant(var, p), mc::bad_cast_exception);

    // 尝试从整数 variant 转换为对象（应该抛出异常）
    mc::variant var_int(123);
    EXPECT_THROW(mc::reflect::from_variant(var_int, p), mc::bad_cast_exception);
}

// 测试枚举类型的 to_variant 转换为 dict
TEST(ReflectTest, EnumToVariantToDict)
{
    test_color c = test_color::GREEN;
    mc::dict   dict;
    mc::reflect::to_variant(c, dict);
    EXPECT_TRUE(dict.contains("value"));
    EXPECT_EQ(dict["value"], "GREEN");
}

} // namespace test_reflect