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

/**
 * @file test_custom_member_info.cpp
 * @brief 测试自定义 member_info 提取功能
 */
#include <functional>
#include <gtest/gtest.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/variant.h>
#include <string>

using namespace mc;

// 1. 首先定义信号标签类型
namespace mc::reflect {
struct signal_tag {};
} // namespace mc::reflect

// 2. 然后定义信号信息类
template <typename C, typename Signature>
struct signal_info : public mc::reflect::member_info_base<C> {
    using tag_type = mc::reflect::signal_tag;

    mc::signal<Signature> C::* signal_ptr;

    constexpr signal_info(std::string_view n, mc::signal<Signature> C::* ptr)
        : mc::reflect::member_info_base<C>(n), signal_ptr(ptr) {
    }

    std::type_index typeinfo() const override {
        return typeid(mc::signal<Signature>);
    }
    std::string_view type_name() const override {
        return "signal";
    }
};

class TestValue {
public:
    std::string m_name;
    int         m_value;

    // 信号成员（无法被转换成 mc::variant）
    mc::signal<void(int)>                value_changed;
    mc::signal<void(const std::string&)> name_changed;

    // 普通方法
    void set_value(int v) {
        if (m_value != v) {
            m_value = v;
            value_changed(v);
        }
    }

    void set_name(const std::string& n) {
        if (m_name != n) {
            m_name = n;
            name_changed(n);
        }
    }
};

// 3. 现在我们可以为具体类型特化 member_info_creator
namespace mc::reflect::detail {
template <typename Signature>
struct member_info_creator<TestValue, mc::signal<Signature>> {
    static auto create(mc::signal<Signature> TestValue::* member_ptr, std::string_view name) {
        return std::tuple<signal_info<TestValue, Signature>>{
            signal_info<TestValue, Signature>{name, member_ptr}};
    }
};
} // namespace mc::reflect::detail

// 反射TestValue类
MC_REFLECT(TestValue,
           // 普通的属性反射
           ((m_name, "name"))((m_value, "value"))
           // 信号的反射，通过特化 member_info_creator<TestValue, mc::signal<Signature>> 支持
           (value_changed)(name_changed)
           // 普通方法的反射
           (set_value)(set_name))

// 测试自定义成员信息提取
TEST(CustomMemberInfoTest, SignalMemberInfo) {
    // 检查可反射性
    EXPECT_TRUE(mc::reflect::is_reflectable<TestValue>());

    // 因为 mc::signal 没有实现 to_variant 函数，不能被转换为 mc::variant 类型
    bool is_property_value_changed =
        mc::reflect::detail::is_property_v<decltype(&TestValue::value_changed)>;
    bool is_property_name_changed =
        mc::reflect::detail::is_property_v<decltype(&TestValue::name_changed)>;
    bool is_property_value = mc::reflect::detail::is_property_v<decltype(&TestValue::m_value)>;

    EXPECT_FALSE(is_property_value_changed);
    EXPECT_FALSE(is_property_name_changed);
    EXPECT_TRUE(is_property_value);

    // 获取所有成员
    auto& all_members = mc::reflect::reflector<TestValue>::get_members();

    // 获取属性
    auto& properties = mc::reflect::reflector<TestValue>::get_properties();

    // 获取方法
    auto& methods = mc::reflect::reflector<TestValue>::get_methods();

    // 获取信号（使用自定义tag）
    auto& signals =
        mc::reflect::reflector<TestValue>::get_members_by_tag<mc::reflect::signal_tag>();

    // 检查成员数量
    EXPECT_EQ(std::tuple_size_v<std::remove_reference_t<decltype(all_members)>>, 6);
    EXPECT_EQ(std::tuple_size_v<std::remove_reference_t<decltype(properties)>>, 2);
    EXPECT_EQ(std::tuple_size_v<std::remove_reference_t<decltype(methods)>>, 2);
    EXPECT_EQ(std::tuple_size_v<std::remove_reference_t<decltype(signals)>>,
              2); // 应该有2个信号

    // 测试信号成员信息
    auto signal_visitor = [](const auto& member) {
        EXPECT_TRUE((std::is_same_v<typename std::remove_reference_t<decltype(member)>::tag_type,
                                    mc::reflect::signal_tag>));
    };

    std::apply(
        [&signal_visitor](const auto&... members) {
            (signal_visitor(members), ...);
        },
        signals);
}
