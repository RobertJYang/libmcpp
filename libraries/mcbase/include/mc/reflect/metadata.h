
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

// 编译期静态反射辅助函数

/** @brief 获取给定类型的静态反射元数据 */
template <typename T>
const auto& get_static_members()
{
    return mc::reflect::static_metadata<T>::members;
}

/** @brief 获取给定类型的指定标签成员 */
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

/** @brief 获取给定类型的所有静态属性 */
template <typename T>
auto get_static_properties()
{
    return get_static_members_by_tag<T, mc::reflect::property_tag>();
}

/** @brief 获取给定类型的静态字段属性 */
template <typename T>
auto get_static_field_properties()
{
    return mc::reflect::detail::filter_field_properties<T>(get_static_members<T>());
}

/** @brief 获取给定类型的所有静态方法 */
template <typename T>
auto get_static_methods()
{
    return get_static_members_by_tag<T, mc::reflect::method_tag>();
}

/** @brief 获取给定类型的所有静态基类 */
template <typename T>
auto get_static_base_classes()
{
    return get_static_members_by_tag<T, mc::reflect::base_class_tag>();
}

} // namespace mc::reflect

namespace mc::reflect {

// 动态反射元数据定义

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

    // quark 重载
    const property_type_info*   get_property_info(mc::quark name) const;
    const method_type_info*     get_method_info(mc::quark name) const;
    const base_class_type_info* get_base_class_info(mc::quark name) const;
    const member_info_base*     get_custom_info(mc::quark name, size_t reflect_type) const;

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