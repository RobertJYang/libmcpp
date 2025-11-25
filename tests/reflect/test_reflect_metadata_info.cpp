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

/**
 * @file test_reflect_metadata_info.cpp
 * @brief 测试反射元数据信息相关的方法
 */
#include <gtest/gtest.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <typeindex>

namespace test_metadata_info {
// 测试类
class test_class {
public:
    MC_REFLECTABLE("test_metadata_info.test_class");

    int         m_id;
    std::string m_name;
    double      m_score;

    int get_id() const {
        return m_id;
    }

    void set_id(int id) {
        m_id = id;
    }

    int get_readonly_value() const {
        return m_id * 2;
    }

    // 静态方法
    static int static_add(int a, int b) {
        return a + b;
    }

    static std::string static_format(const std::string& prefix, int value) {
        return prefix + std::to_string(value);
    }

    // 非静态方法
    int add(int a, int b) {
        return a + b + m_id;
    }

    std::string format(const std::string& prefix) {
        return prefix + std::to_string(m_id);
    }
};
} // namespace test_metadata_info

MC_REFLECT(test_metadata_info::test_class,
           (m_id)(m_name)(m_score)(MC_COMPUTED_PROPERTY("id", get_id, set_id))(
               MC_COMPUTED_PROPERTY("readonly_value", get_readonly_value))(static_add)(
               static_format)(add)(format))

using namespace test_metadata_info;

// 测试 property_info::typeinfo() 和 type_name()
TEST(ReflectMetadataInfoTest, PropertyInfoTypeInfo) {
    test_class obj;
    obj.m_id    = 10;
    obj.m_name  = "test";
    obj.m_score = 95.5;

    // 获取属性信息 - 通过成员指针获取
    auto* id_prop = mc::reflect::get_reflection<test_class>().get_property_info(&test_class::m_id);
    ASSERT_NE(id_prop, nullptr);

    // 测试 typeinfo()
    std::type_index id_type = id_prop->typeinfo();
    EXPECT_EQ(id_type, typeid(int));

    // 测试 type_name()
    std::string_view id_type_name = id_prop->type_name();
    EXPECT_FALSE(id_type_name.empty());

    // 测试其他属性
    auto* name_prop = mc::reflect::get_reflection<test_class>().get_property_info(&test_class::m_name);
    ASSERT_NE(name_prop, nullptr);
    EXPECT_EQ(name_prop->typeinfo(), typeid(std::string));
    EXPECT_FALSE(name_prop->type_name().empty());

    auto* score_prop = mc::reflect::get_reflection<test_class>().get_property_info(&test_class::m_score);
    ASSERT_NE(score_prop, nullptr);
    EXPECT_EQ(score_prop->typeinfo(), typeid(double));
    EXPECT_FALSE(score_prop->type_name().empty());
}

// 测试 computed_property_info::offset(), typeinfo(), type_name(), get_signature()
TEST(ReflectMetadataInfoTest, ComputedPropertyInfoMethods) {
    test_class obj;
    obj.m_id = 20;

    // 获取计算属性信息
    auto* id_prop = mc::reflect::get_property_info<test_class>("id");
    ASSERT_NE(id_prop, nullptr);

    // 测试 offset() - 计算属性应该返回 0
    EXPECT_EQ(id_prop->offset(), 0U);

    // 测试 typeinfo()
    std::type_index id_type = id_prop->typeinfo();
    EXPECT_EQ(id_type, typeid(int));

    // 测试 type_name()
    std::string_view id_type_name = id_prop->type_name();
    EXPECT_FALSE(id_type_name.empty());

    // 测试 get_signature()
    std::string_view id_signature = id_prop->get_signature();
    EXPECT_FALSE(id_signature.empty());

    // 测试只读计算属性
    auto* readonly_prop = mc::reflect::get_property_info<test_class>("readonly_value");
    ASSERT_NE(readonly_prop, nullptr);
    EXPECT_EQ(readonly_prop->offset(), 0U);
    EXPECT_EQ(readonly_prop->typeinfo(), typeid(int));
    EXPECT_FALSE(readonly_prop->type_name().empty());
    EXPECT_FALSE(readonly_prop->get_signature().empty());
}

// 测试 computed_property_info::clone()
TEST(ReflectMetadataInfoTest, ComputedPropertyInfoClone) {
    test_class obj;
    obj.m_id = 30;

    // 获取计算属性信息
    auto* id_prop = mc::reflect::get_property_info<test_class>("id");
    ASSERT_NE(id_prop, nullptr);

    // 测试 clone()
    auto* cloned_prop = id_prop->clone();
    ASSERT_NE(cloned_prop, nullptr);

    // 验证克隆的属性信息
    EXPECT_EQ(cloned_prop->name, id_prop->name);
    EXPECT_EQ(cloned_prop->offset(), id_prop->offset());
    EXPECT_EQ(cloned_prop->typeinfo(), id_prop->typeinfo());
    EXPECT_EQ(cloned_prop->type_name(), id_prop->type_name());

    // 清理 - 使用 operator delete，因为 member_info_base 没有虚析构函数
    operator delete(const_cast<mc::reflect::member_info_base*>(cloned_prop));
}

// 测试 method_info::is_static(), typeinfo(), type_name(), arg_count()
TEST(ReflectMetadataInfoTest, MethodInfoMethods) {
    // 测试静态方法
    auto* static_add_method = mc::reflect::get_method_info<test_class>("static_add");
    ASSERT_NE(static_add_method, nullptr);

    // 测试 is_static()
    EXPECT_TRUE(static_add_method->is_static());

    // 测试 typeinfo() - 返回类型
    std::type_index return_type = static_add_method->typeinfo();
    EXPECT_EQ(return_type, typeid(int));

    // 测试 type_name() - 返回类型名称
    std::string_view return_type_name = static_add_method->type_name();
    EXPECT_FALSE(return_type_name.empty());

    // 测试 arg_count()
    EXPECT_EQ(static_add_method->arg_count(), 2U);

    // 测试非静态方法
    auto* add_method = mc::reflect::get_method_info<test_class>("add");
    ASSERT_NE(add_method, nullptr);

    // 测试 is_static()
    EXPECT_FALSE(add_method->is_static());

    // 测试 typeinfo()
    EXPECT_EQ(add_method->typeinfo(), typeid(int));

    // 测试 type_name()
    EXPECT_FALSE(add_method->type_name().empty());

    // 测试 arg_count()
    EXPECT_EQ(add_method->arg_count(), 2U);

    // 测试无参数方法
    test_class obj;
    obj.m_id = 40;

    // 测试静态方法调用
    {
        mc::variants args = {mc::variant(5), mc::variant(7)};
        mc::variant  result = static_add_method->invoke(args);
        EXPECT_EQ(result, 12);
    }

    // 测试静态方法异步调用
    {
        mc::variants args = {mc::variant(3), mc::variant(4)};
        auto         async_result = static_add_method->async_invoke(args);
        mc::variant  result       = async_result.get();
        EXPECT_EQ(result, 7);
    }

    // 测试静态方法 format
    auto* static_format_method = mc::reflect::get_method_info<test_class>("static_format");
    ASSERT_NE(static_format_method, nullptr);
    EXPECT_TRUE(static_format_method->is_static());
    EXPECT_EQ(static_format_method->typeinfo(), typeid(std::string));
    EXPECT_EQ(static_format_method->arg_count(), 2U);

    {
        mc::variants args = {mc::variant("prefix_"), mc::variant(100)};
        mc::variant  result = static_format_method->invoke(args);
        EXPECT_EQ(result.as<std::string>(), "prefix_100");
    }
}

// 测试 from_variant 中数组长度不足的情况
TEST(ReflectMetadataInfoTest, FromVariantArrayInsufficientLength) {
    test_class obj;
    obj.m_id    = 50;
    obj.m_name  = "original";
    obj.m_score = 80.0;

    // 创建长度不足的数组
    mc::variants short_array = {mc::variant(60)};

    // 从数组转换 - 只有第一个元素会被设置，其他保持原值
    mc::variant var(short_array);
    mc::reflect::from_variant(var, obj);

    // 验证只有 m_id 被更新
    EXPECT_EQ(obj.m_id, 60);
    // m_name 和 m_score 应该保持原值（因为数组长度不足）
    EXPECT_EQ(obj.m_name, "original");
    EXPECT_DOUBLE_EQ(obj.m_score, 80.0);
}

// 测试 is_valid_type_name 中单独的 `:` 不合法分支
TEST(ReflectMetadataInfoTest, IsValidTypeNameInvalidColon) {
    // 测试单独的 `:` 字符（不是 `::`）
    EXPECT_FALSE(mc::reflect::is_valid_type_name("test:name"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("test:"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name(":test"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("test::name:invalid"));
    EXPECT_FALSE(mc::reflect::is_valid_type_name("test:name:invalid"));

    // 测试合法的 `::` 分隔符
    EXPECT_TRUE(mc::reflect::is_valid_type_name("test::name"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc::reflect::test"));

    // 测试合法的 `.` 分隔符
    EXPECT_TRUE(mc::reflect::is_valid_type_name("test.name"));
    EXPECT_TRUE(mc::reflect::is_valid_type_name("mc.reflect.test"));
}

// 测试 property_info::clone()
TEST(ReflectMetadataInfoTest, PropertyInfoClone) {
    test_class obj;
    obj.m_id = 70;

    // 获取属性信息 - 通过成员指针获取
    auto* id_prop = mc::reflect::get_reflection<test_class>().get_property_info(&test_class::m_id);
    ASSERT_NE(id_prop, nullptr);

    // 测试 clone()
    auto* cloned_prop = id_prop->clone();
    ASSERT_NE(cloned_prop, nullptr);

    // 验证克隆的属性信息
    EXPECT_EQ(cloned_prop->name, id_prop->name);
    EXPECT_EQ(cloned_prop->offset(), id_prop->offset());
    EXPECT_EQ(cloned_prop->typeinfo(), id_prop->typeinfo());
    EXPECT_EQ(cloned_prop->type_name(), id_prop->type_name());

    // 清理 - 使用 operator delete，因为 member_info_base 没有虚析构函数
    operator delete(const_cast<mc::reflect::member_info_base*>(cloned_prop));
}

