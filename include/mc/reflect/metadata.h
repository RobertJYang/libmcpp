
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
 * @file metadata.h
 * @brief 反射元数据缓存
 */
#ifndef MC_REFLECT_METADATA_H
#define MC_REFLECT_METADATA_H

#include <functional>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include <mc/reflect/metadata_info.h>
#include <mc/singleton.h>

namespace mc::reflect {
template <typename T, typename = void>
class reflection;

// 几个辅助函数用于在编译期获取给定类型的静态反射元数据
//
// 我们使用 MC_REFLECTABLE 和 MC_REFLECT 宏分别用于声明和定义类型反射
// 应该避免将 MC_REFLECT 宏用在头文件中，因为这会导致反射信息被重复编译造成编译时间过长，
// 建议在头文件中用 MC_REFLECTABLE 声明类型反射，在源文件中使用 MC_REFLECT 定义反射。
//
// 使用示例：
/*
    // 在头文件中声明类型支持反射
    struct MyClass {
        MC_REFLECTABLE("test.MyClass"); // 声明反射

        int a;
        std::string b;
    };

    // 定义反射，以下建议放在源文件中
    MC_REFLECT(MyClass, (a)(b));

    // 在同一个源文件中静态反射信息可见，可以获取静态反射元数据
    auto properties = mc::reflect::get_static_properties<MyClass>(); // 获取该类所有属性的静态元数据信息
    mc::traits::tuple_for_each(properties, [](auto* member) {
        using member_info_type = std::decay_t<decltype(*member)>;
        using property_type    = typename member_info_type::member_type;
        using class_type       = typename member_info_type::class_type;
        if constexpr (std::is_same_v<property_type, int>) {
            std::cout << mc::pretty_name<class_type>() << "::"
                        << member->name << " is int type" << std::endl;
        }
    });

    // 我们还可以打印观察构建的静态反射元数据包含哪些类型
    std::cout << mc::pretty_name<decltype(mc::reflect::get_static_members<MyClass>())>()
                  << std::endl;
    // 这会输出：
    // const std::tuple<mc::reflect::property_info<MyClass, int>, mc::reflect::property_info<MyClass, std::string>> &
*/

/**
 * @brief 获取给定类型的静态反射元数据
 * @return 给定类型的静态反射元数据的引用
 * @note mc::reflect::static_metadata<T>::members 是一个编译期常量，这里只返回引用，如果需要基于这个数据
 * 生成其他编译期常量，可以直接使用 mc::reflect::static_metadata<T>::members 而不是这里返回的引用。
 */
template <typename T>
const auto& get_static_members() {
    return mc::reflect::static_metadata<T>::members;
}

/**
 * @brief 获取给定类型的静态反射元数据中指定标签的成员
 * @return 给定类型的静态反射元数据中指定标签的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T, typename Tag>
auto get_static_members_by_tag() {
    return mc::reflect::detail::filter_members_by_tag<T, Tag>(get_static_members<T>());
}

/**
 * @brief 获取给定类型的静态反射元数据中所有属性的成员
 * @return 给定类型的静态反射元数据中所有属性的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_properties() {
    return get_static_members_by_tag<T, mc::reflect::property_tag>();
}

/**
 * @brief 获取给定类型的静态反射元数据中所有方法的成员
 * @return 给定类型的静态反射元数据中所有方法的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_methods() {
    return get_static_members_by_tag<T, mc::reflect::method_tag>();
}

/**
 * @brief 获取给定类型的静态反射元数据中所有基类的成员
 * @return 给定类型的静态反射元数据中所有基类的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_base_classes() {
    return get_static_members_by_tag<T, mc::reflect::base_class_tag>();
}

} // namespace mc::reflect

namespace mc::reflect {

// 以下定义了 struct_metadata 和 enum_metadata 用于保存动态反射元数据
// 动态反射元数据采用延迟初始化策略，在第一次调用 mc::reflect::reflector<T>::get_metadata() 时通过静态元数据自动生成。
// 因为静态反射元数据会在编译期展开，这会造成代码膨胀和编译速度变慢，除非性能特别敏感的地方，否则应该优先使用动态反射元数据。

enum class visit_status {
    VS_CONTINUE,
    VS_BREAK,
};

class MC_API struct_metadata {
public:
    struct_metadata(std::string_view name);

    template <typename T, typename Members>
    struct_metadata(std::string_view name, const Members& members, const T* o)
        : struct_metadata(name) {
        add_members(members, o);
    }

    ~struct_metadata();

    std::string_view get_name() const;

    void add_property_info(const property_type_info* property);
    void add_method_info(const method_type_info* method);
    void add_base_class_info(const base_class_type_info* base_class);
    void add_custom_info(const member_info_base* custom);

    const property_type_info*   get_property_info(std::string_view name) const;
    const property_type_info*   get_property_info(size_t offset) const;
    const method_type_info*     get_method_info(std::string_view name) const;
    const method_type_info*     get_method_info(size_t offset) const;
    const base_class_type_info* get_base_class_info(std::string_view name) const;

    using property_visitor_t   = std::function<visit_status(const property_type_info*)>;
    using method_visitor_t     = std::function<visit_status(const method_type_info*)>;
    using base_class_visitor_t = std::function<visit_status(const base_class_type_info*)>;
    using custom_visitor_t     = std::function<visit_status(const member_info_base*)>;

    void visit_property(const property_visitor_t& visitor) const;
    void visit_method(const method_visitor_t& visitor) const;
    void visit_base_class(const base_class_visitor_t& visitor) const;
    void visit_custom(const custom_visitor_t& visitor) const;

    std::vector<const property_type_info*>   get_properties() const;
    std::vector<const method_type_info*>     get_methods() const;
    std::vector<const base_class_type_info*> get_base_classes() const;
    std::vector<const member_info_base*>     get_custom_members() const;

private:
    template <typename T, typename Members>
    void add_members(const Members& members, const T*) {
        // 先添加子类成员再添加基类成员，子类成员会覆盖基类同名成员
        mc::traits::tuple_for_each(members, [&](auto& member) {
            using member_type = std::decay_t<decltype(member)>;
            if constexpr (std::is_base_of_v<method_type_info, member_type>) {
                add_method_info(&member);
            } else if constexpr (std::is_base_of_v<property_type_info, member_type>) {
                add_property_info(&member);
            } else {
                if (member.type() >= static_cast<int>(member_info_type::custom_start)) {
                    add_custom_info(&member);
                }
            }
        });
        mc::traits::tuple_for_each(members, [&](auto& member) {
            using member_type = std::decay_t<decltype(member)>;
            if constexpr (std::is_base_of_v<base_class_type_info, member_type>) {
                add_base_class_info(&member);
            }
        });
    }

    struct impl;
    std::unique_ptr<impl> m_impl;
};

struct enum_values {
    enum_values() = default;

    template <size_t N>
    enum_values(const std::array<enum_member_info, N>& values)
        : members(&values[0]), count(N) {
    }

    enum_values(const enum_member_info* values, size_t count)
        : members(values), count(count) {
    }

    const enum_member_info* members{nullptr};
    size_t                  count{0};
};

class MC_API enum_metadata {
public:
    enum_metadata(std::string_view name, enum_values values);
    ~enum_metadata();

    enum_value_type  get_value(std::string_view name) const;
    enum_value_type  get_value_from_variant(const mc::variant& var) const;
    std::string_view get_name(enum_value_type value) const;

    std::optional<enum_value_type>  try_get_value(std::string_view name) const;
    std::optional<enum_value_type>  try_get_value_from_variant(const mc::variant& var) const;
    std::optional<std::string_view> try_get_name(enum_value_type value) const;

    bool has_value(std::string_view name) const;
    bool has_value(enum_value_type value) const;

    std::vector<std::string_view> get_names() const;
    enum_values                   get_values() const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_H