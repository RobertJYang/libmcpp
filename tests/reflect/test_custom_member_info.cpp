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

#define TEST_SIGNAL_TYPE (static_cast<int>(mc::reflect::member_info_type::custom_start) + 1)

namespace test_custom_member_info {
// 2. 然后定义信号信息类
template <typename C, typename Signature>
struct signal_info : public mc::reflect::member_info_base {
    using tag_type = mc::reflect::signal_tag;

    mc::signal<Signature> C::* signal_ptr;

    constexpr signal_info(std::string_view n, mc::signal<Signature> C::* ptr)
        : mc::reflect::member_info_base(n), signal_ptr(ptr) {
    }

    std::type_index typeinfo() const override {
        return typeid(mc::signal<Signature>);
    }
    std::string_view type_name() const override {
        return "signal";
    }

    int type() const override {
        return static_cast<int>(mc::reflect::member_info_type::custom_start) + 1;
    }

    std::uintptr_t offset() const override {
        return MC_MEMBER_OFFSETOF(C, signal_ptr);
    }

    member_info_base* clone() const override {
        return new signal_info<C, Signature>(this->name, this->signal_ptr);
    }
};

class TestValue {
public:
    MC_REFLECTABLE("test_custom_member_info.TestValue");

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
} // namespace test_custom_member_info

using TestValue = test_custom_member_info::TestValue;
template <typename C, typename Signature>
using signal_info = test_custom_member_info::signal_info<C, Signature>;

// 3. 现在我们可以为具体类型特化 member_info_creator
namespace mc::reflect {

template <typename Signature>
struct member_info_creator<TestValue, mc::signal<Signature>> {
    static constexpr auto create(mc::signal<Signature> TestValue::* member_ptr, std::string_view name) {
        return std::make_tuple(signal_info<TestValue, Signature>{name, member_ptr});
    }
};
} // namespace mc::reflect

// 反射TestValue类
MC_REFLECT(test_custom_member_info::TestValue,
           // 普通的属性反射
           ((m_name, "name"))((m_value, "value"))
           // 信号的反射，通过特化 member_info_creator<TestValue, mc::signal<Signature>> 支持
           (value_changed)(name_changed)
           // 普通方法的反射
           (set_value)(set_name))

struct MyClass {
    MC_REFLECTABLE("test.MyClass");
    int         a;
    std::string b;
};

MC_REFLECT(MyClass, (a)(b));

// 测试自定义成员信息提取
TEST(CustomMemberInfoTest, SignalMemberInfo) {
    // 检查可反射性
    EXPECT_TRUE(mc::reflect::is_reflectable<TestValue>());

    // 因为 mc::signal 没有实现 to_variant 函数，不能被转换为 mc::variant 类型
    bool is_property_value_changed =
        mc::reflect::is_property_v<decltype(&TestValue::value_changed)>;
    bool is_property_name_changed = mc::reflect::is_property_v<decltype(&TestValue::name_changed)>;
    bool is_property_value        = mc::reflect::is_property_v<decltype(&TestValue::m_value)>;

    EXPECT_FALSE(is_property_value_changed);
    EXPECT_FALSE(is_property_name_changed);
    EXPECT_TRUE(is_property_value);

    // 获取运行时属性、方法、信号元信息
    auto& metadata   = mc::reflect::reflector<TestValue>::get_metadata();
    auto  properties = metadata.get_properties();
    auto  methods    = metadata.get_methods();
    auto  signals    = metadata.get_custom_members();

    EXPECT_EQ(properties.size(), 2);
    EXPECT_EQ(methods.size(), 2);
    EXPECT_EQ(signals.size(), 2);
    for (auto& signal : signals) {
        EXPECT_EQ(signal->type(), TEST_SIGNAL_TYPE);
    }

    // 获取运行静态属性、方法、信号元信息
    auto static_members    = mc::reflect::get_static_members<TestValue>();
    auto static_properties = mc::reflect::get_static_properties<TestValue>();
    auto static_methods    = mc::reflect::get_static_methods<TestValue>();
    auto static_signals    = mc::reflect::get_static_members_by_tag<TestValue, mc::reflect::signal_tag>();

    EXPECT_EQ(std::tuple_size_v<decltype(static_members)>, 6);
    EXPECT_EQ(std::tuple_size_v<decltype(static_properties)>, 2);
    EXPECT_EQ(std::tuple_size_v<decltype(static_methods)>, 2);
    EXPECT_EQ(std::tuple_size_v<decltype(static_signals)>, 2);

    // 使用 tuple_for_each 遍历静态反射元数据
    mc::traits::tuple_for_each(static_signals, [](auto* member) {
        EXPECT_EQ(member->type(), TEST_SIGNAL_TYPE);
    });

    {
        std::cout << mc::pretty_name<decltype(mc::reflect::get_static_members<MyClass>())>()
                  << std::endl;
        auto properties = mc::reflect::get_static_properties<MyClass>();
        mc::traits::tuple_for_each(properties, [](auto* member) {
            using member_info_type = std::decay_t<decltype(*member)>;
            using property_type    = typename member_info_type::member_type;
            using class_type       = typename member_info_type::class_type;
            if constexpr (std::is_same_v<property_type, int>) {
                std::cout << mc::pretty_name<class_type>() << "::"
                          << member->name << " is int type" << std::endl;
            }
        });
    }
}
