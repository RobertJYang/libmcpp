
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
} // namespace mc::reflect

namespace mc::reflect::detail {

template <typename T, typename Tag, typename Members>
auto get_members_by_tag(const Members& members) {
    return mc::reflect::detail::filter_members_by_tag<T, Tag>(members);
}

template <typename T, typename Members>
auto get_properties(const Members& members) {
    return get_members_by_tag<T, mc::reflect::property_tag>(members);
}

template <typename T, typename Members>
auto get_methods(const Members& members) {
    return get_members_by_tag<T, mc::reflect::method_tag>(members);
}

template <typename T, typename Members>
auto get_base_classes(const Members& members) {
    return get_members_by_tag<T, mc::reflect::base_class_tag>(members);
}

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

    const property_type_info*   get_property_info(std::string_view name) const;
    const property_type_info*   get_property_info(size_t offset) const;
    const method_type_info*     get_method_info(std::string_view name) const;
    const method_type_info*     get_method_info(size_t offset) const;
    const base_class_type_info* get_base_class_info(std::string_view name) const;

    using property_visitor_t   = std::function<visit_status(const property_type_info*)>;
    using method_visitor_t     = std::function<visit_status(const method_type_info*)>;
    using base_class_visitor_t = std::function<visit_status(const base_class_type_info*)>;

    void visit_property(const property_visitor_t& visitor) const;
    void visit_method(const method_visitor_t& visitor) const;
    void visit_base_class(const base_class_visitor_t& visitor) const;

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

} // namespace mc::reflect::detail

#endif // MC_REFLECT_METADATA_H