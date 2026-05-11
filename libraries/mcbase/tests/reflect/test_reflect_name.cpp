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
#include <mc/dict.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <string_view>
#include <type_traits>

namespace test_reflect_name {
static_assert(std::is_same_v<
              decltype(static_cast<mc::string_view (*)(mc::string_view) noexcept>(
                  &mc::reflect::detail::reflect_name_from_literal)),
              mc::string_view (*)(mc::string_view) noexcept>);

static_assert(std::is_same_v<
              decltype(static_cast<mc::string_view (*)(std::string_view) noexcept>(
                  &mc::reflect::detail::reflect_name_from_literal)),
              mc::string_view (*)(std::string_view) noexcept>);

// 定义一个简单的用户类，用于测试反射
struct user {
    MC_REFLECTABLE("test_reflect_name.user");

    int         m_id;
    mc::string m_name;
    int         m_age;
};

// 定义一个混合使用默认名称和自定义名称的类
struct product {
    MC_REFLECTABLE("test_reflect_name.product");

    int         m_id;
    mc::string m_name;
    double      m_price;
    int         m_stock;
};

constexpr std::string_view service_name = "test_reflect_name.service";

struct service {
    MC_REFLECTABLE(service_name);

    int m_version;
};
} // namespace test_reflect_name

MC_REFLECT(test_reflect_name::user, ((m_id, "ID"))((m_name, "姓名"))((m_age, "年龄")))
MC_REFLECT(test_reflect_name::product, (m_id)((m_name, "产品名称"))(m_price)((m_stock, "库存")))
MC_REFLECT(test_reflect_name::service, (m_version))

namespace test_reflect_name {

// 测试自定义名称的反射
TEST(reflect_custom_name_test, custom_name)
{
    // 创建一个用户对象
    user u;
    u.m_id   = 1001;
    u.m_name = "张三";
    u.m_age  = 30;

    // 转换为variant
    mc::variant var;
    mc::to_variant(u, var);

    // 检查variant
    ASSERT_TRUE(var.is_dict());

    // 获取dict
    auto d = var.as<mc::dict>();

    // 检查自定义键名
    ASSERT_TRUE(d.contains("ID"));
    ASSERT_TRUE(d.contains("姓名"));
    ASSERT_TRUE(d.contains("年龄"));

    // 检查值
    ASSERT_EQ(d["ID"].as<int>(), 1001);
    ASSERT_EQ(d["姓名"].as<mc::string>(), "张三");
    ASSERT_EQ(d["年龄"].as<int>(), 30);

    // 测试从variant转换回用户对象
    user u2;
    mc::from_variant(var, u2);

    // 检查转换结果
    ASSERT_EQ(u2.m_id, 1001);
    ASSERT_EQ(u2.m_name, "张三");
    ASSERT_EQ(u2.m_age, 30);
}

// 测试混合使用默认名称和自定义名称的反射
TEST(reflect_custom_name_test, mixed_names)
{
    // 创建一个产品对象
    product p;
    p.m_id    = 101;
    p.m_name  = "笔记本电脑";
    p.m_price = 5999.99;
    p.m_stock = 50;

    // 转换为variant
    mc::variant var;
    mc::to_variant(p, var);

    // 检查variant
    ASSERT_TRUE(var.is_dict());

    // 获取dict
    auto d = var.as<mc::dict>();

    // 检查键名
    ASSERT_TRUE(d.contains("m_id"));     // 默认名称
    ASSERT_TRUE(d.contains("产品名称")); // 自定义名称
    ASSERT_TRUE(d.contains("m_price"));  // 默认名称
    ASSERT_TRUE(d.contains("库存"));     // 自定义名称

    // 检查值
    ASSERT_EQ(d["m_id"].as<int>(), 101);
    ASSERT_EQ(d["产品名称"].as<mc::string>(), "笔记本电脑");
    ASSERT_EQ(d["m_price"].as<double>(), 5999.99);
    ASSERT_EQ(d["库存"].as<int>(), 50);

    // 测试visit_members功能
    std::vector<mc::string> member_names;
    mc::reflect::visit_properties<product>([&](mc::string_view name, auto, auto) {
        member_names.push_back(mc::string(name));
    });

    // 检查成员名称
    ASSERT_EQ(member_names.size(), 4);
    ASSERT_TRUE(std::find(member_names.begin(), member_names.end(), "m_id") != member_names.end());
    ASSERT_TRUE(std::find(member_names.begin(), member_names.end(), "产品名称") != member_names.end());
    ASSERT_TRUE(std::find(member_names.begin(), member_names.end(), "m_price") != member_names.end());
    ASSERT_TRUE(std::find(member_names.begin(), member_names.end(), "库存") != member_names.end());
}

// 测试反射工具函数
TEST(reflect_custom_name_test, reflection_utils)
{
    // 测试类型名称获取
    ASSERT_EQ(mc::reflect::get_type_name<user>(), "test_reflect_name.user");
    ASSERT_EQ(mc::reflect::get_type_name<product>(), "test_reflect_name.product");

    // 测试类型是否可反射
    ASSERT_TRUE(mc::reflect::is_reflectable<user>());
    ASSERT_TRUE(mc::reflect::is_reflectable<product>());
    ASSERT_FALSE(mc::reflect::is_reflectable<int>());

    // 测试类型是否为枚举
    ASSERT_FALSE(mc::reflect::is_enum<user>());
    ASSERT_FALSE(mc::reflect::is_enum<product>());
}

TEST(reflect_custom_name_test, reflectable_accepts_std_string_view_constant)
{
    ASSERT_EQ(mc::reflect::get_type_name<service>(), "test_reflect_name.service");
}

} // namespace test_reflect_name