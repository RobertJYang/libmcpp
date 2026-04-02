
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

#include <mc/intrusive/list.h>
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
        mc::string b;
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
    // const std::tuple<mc::reflect::static_property_info<int MyClass::*>,
    //                  mc::reflect::static_property_info<mc::string MyClass::*>> &
*/

/**
 * @brief 获取给定类型的静态反射元数据
 * @return 给定类型的静态反射元数据的引用
 * @note mc::reflect::static_metadata<T>::members 是一个编译期常量，这里只返回引用，如果需要基于这个数据
 * 生成其他编译期常量，可以直接使用 mc::reflect::static_metadata<T>::members 而不是这里返回的引用。
 */
template <typename T>
const auto& get_static_members()
{
    return mc::reflect::static_metadata<T>::members;
}

/**
 * @brief 获取给定类型的静态反射元数据中指定标签的成员
 * @return 给定类型的静态反射元数据中指定标签的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T, typename Tag>
auto get_static_members_by_tag()
{
    return mc::reflect::detail::filter_members_by_tag<T, Tag>(get_static_members<T>());
}

namespace detail {

template <typename T, typename = void>
struct has_member_ptr : std::false_type {};

template <typename T>
struct has_member_ptr<T, std::void_t<decltype(std::declval<T>().member_ptr)>> : std::true_type {};

template <typename T>
inline constexpr bool has_member_ptr_v = has_member_ptr<T>::value;

template <typename T, typename = void>
struct has_runtime_method_factory : std::false_type {};

template <typename T>
struct has_runtime_method_factory<T, std::void_t<decltype(std::declval<const T&>().create_runtime_method())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_runtime_method_factory_v = has_runtime_method_factory<T>::value;

template <typename T, typename = void>
struct has_runtime_property_factory : std::false_type {};

template <typename T>
struct has_runtime_property_factory<T, std::void_t<decltype(std::declval<const T&>().create_runtime_property())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_runtime_property_factory_v = has_runtime_property_factory<T>::value;

struct filter_field_property {
    template <typename ElementType>
    static constexpr bool check = has_tag_v<property_tag, ElementType> && has_member_ptr_v<ElementType>;
};

template <typename T, typename Tuple>
auto filter_field_properties(const Tuple& all_members)
{
    auto properties = filter_members<T, filter_field_property>(all_members);
    return mc::traits::tuple_map(properties, [](auto* property) {
        return std::make_tuple(*property);
    });
}

} // namespace detail

/**
 * @brief 获取给定类型的静态反射元数据中所有属性的成员
 * @return 给定类型的静态反射元数据中所有属性的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_properties()
{
    return get_static_members_by_tag<T, mc::reflect::property_tag>();
}

/**
 * @brief 获取给定类型的静态字段属性成员
 * @return 仅包含成员指针字段属性的静态描述信息
 * @note 这里会排除计算属性等没有 member_ptr 的属性成员。
 */
template <typename T>
auto get_static_field_properties()
{
    return mc::reflect::detail::filter_field_properties<T>(get_static_members<T>());
}

/**
 * @brief 获取给定类型的静态反射元数据中所有方法的成员
 * @return 给定类型的静态反射元数据中所有方法的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_methods()
{
    return get_static_members_by_tag<T, mc::reflect::method_tag>();
}

/**
 * @brief 获取给定类型的静态反射元数据中所有基类的成员
 * @return 给定类型的静态反射元数据中所有基类的成员
 * @note 这里返回的是元数据成员指针构成的 std::tuple<M1*, M2*, ...> 对象，
 * 可以使用 mc::traits::tuple_for_each 遍历这个 tuple。
 */
template <typename T>
auto get_static_base_classes()
{
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

template <typename T>
struct member_node : mc::intrusive::slist_base_hook<> {
    const T* member{nullptr};

    member_node() = default;
    member_node(const T* member) : member(member)
    {}
};

using property_list = mc::intrusive::slist<member_node<property_type_info>, mc::intrusive::constant_time_size<false>>;
using method_list   = mc::intrusive::slist<member_node<method_type_info>, mc::intrusive::constant_time_size<false>>;
using base_class_list =
    mc::intrusive::slist<member_node<base_class_type_info>, mc::intrusive::constant_time_size<false>>;
using member_list = mc::intrusive::slist<member_node<member_info_base>, mc::intrusive::constant_time_size<false>>;
using custom_list = member_list;

class MC_API struct_metadata {
public:
    struct_metadata(mc::string_view name, type_id_type type_id);

    template <typename Members>
    struct_metadata(mc::string_view name, type_id_type type_id, const Members& members) : struct_metadata(name, type_id)
    {
        add_members(members);
    }

    ~struct_metadata();

    mc::string_view get_name() const noexcept;
    type_id_type    get_type_id() const noexcept;

    const property_type_info*   get_property_info(mc::string_view name) const;
    const property_type_info*   get_property_info(size_t offset) const;
    const method_type_info*     get_method_info(mc::string_view name) const;
    const method_type_info*     get_method_info(size_t offset) const;
    const base_class_type_info* get_base_class_info(mc::string_view name) const;
    const member_info_base*     get_custom_info(mc::string_view name, size_t reflect_type) const;

    using property_visitor_t   = std::function<visit_status(const property_type_info*)>;
    using method_visitor_t     = std::function<visit_status(const method_type_info*)>;
    using base_class_visitor_t = std::function<visit_status(const base_class_type_info*)>;
    using custom_visitor_t     = std::function<visit_status(const member_info_base*)>;

    void visit_properties(const property_visitor_t& visitor) const;
    void visit_methods(const method_visitor_t& visitor) const;
    void visit_base_classes(const base_class_visitor_t& visitor) const;
    void visit_customs(const custom_visitor_t& visitor) const;

    void                         append_signature(mc::string& sig) const;
    std::vector<type_id_type>    get_base_type_ids() const;
    bool                         is_derived_from(type_id_type base_type_id) const;
    std::vector<mc::string_view> get_property_names() const;
    std::vector<mc::string_view> get_method_names() const;

    const property_list&   get_properties() const;
    const method_list&     get_methods() const;
    const base_class_list& get_base_classes() const;
    const member_list&     get_custom_members() const;

private:
    template <typename Members>
    void add_members(const Members& members)
    {
        // 先添加子类成员再添加基类成员，子类成员会覆盖基类同名成员
        mc::traits::tuple_for_each(members, [&](auto& member) {
            using member_type = std::decay_t<decltype(member)>;
            if constexpr (std::is_base_of_v<member_info_base, member_type>) {
                if (member.type() >= static_cast<int>(member_info_type::custom_start)) {
                    add_custom_info(&member);
                    return;
                }
            }

            if constexpr (std::is_base_of_v<method_type_info, member_type>) {
                add_method_info(&member);
            } else if constexpr (std::is_base_of_v<property_type_info, member_type>) {
                add_property_info(&member);
            } else if constexpr (has_tag_v<method_tag, member_type> &&
                                 detail::has_runtime_method_factory_v<member_type>) {
                add_owned_method_info(member.create_runtime_method());
            } else if constexpr (has_tag_v<property_tag, member_type> &&
                                 detail::has_runtime_property_factory_v<member_type>) {
                add_owned_property_info(member.create_runtime_property());
            }
        });
        mc::traits::tuple_for_each(members, [&](auto& member) {
            using member_type = std::decay_t<decltype(member)>;
            if constexpr (std::is_base_of_v<base_class_type_info, member_type>) {
                add_base_class_info(&member);
            }
        });

        add_members_finish();
    }

    void add_property_info(const property_type_info* property);
    void add_owned_property_info(const property_type_info* property);
    void add_method_info(const method_type_info* method);
    void add_owned_method_info(const method_type_info* method);
    void add_base_class_info(const base_class_type_info* base_class);
    void add_custom_info(const member_info_base* custom);
    void add_members_finish();

    struct impl;
    std::unique_ptr<impl> m_impl;
};

struct enum_values {
    enum_values() = default;

    template <size_t N>
    enum_values(const std::array<enum_member_info, N>& values) : members(&values[0]), count(N)
    {}

    enum_values(const enum_member_info* values, size_t count) : members(values), count(count)
    {}

    const enum_member_info* members{nullptr};
    size_t                  count{0};
};

class MC_API enum_metadata {
public:
    enum_metadata(mc::string_view name, enum_values values);
    ~enum_metadata();

    enum_value_type get_value(mc::string_view name) const;
    enum_value_type get_value_from_variant(const mc::variant& var) const;
    mc::string_view get_name(enum_value_type value) const;

    std::optional<enum_value_type> try_get_value(mc::string_view name) const;
    std::optional<enum_value_type> try_get_value_from_variant(const mc::variant& var) const;
    std::optional<mc::string_view> try_get_name(enum_value_type value) const;

    bool has_value(mc::string_view name) const;
    bool has_value(enum_value_type value) const;

    std::vector<mc::string_view> get_names() const;
    enum_values                  get_values() const;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_H