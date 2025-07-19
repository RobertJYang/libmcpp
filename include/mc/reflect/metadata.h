
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

template <typename T, typename = void>
class metadata;

template <typename T>
class metadata<T, std::enable_if_t<is_reflectable<T>() && !std::is_enum<T>()>> {
public:
    using property_map_t          = std::unordered_map<std::string_view, const property_info_base<T>*>;
    using property_offset_map_t   = std::unordered_map<size_t, const property_info_base<T>*>;
    using method_map_t            = std::unordered_map<std::string_view, const method_info_base<T>*>;
    using base_class_prop_map_t   = std::unordered_map<std::string_view, property_map_t>;
    using base_class_method_map_t = std::unordered_map<std::string_view, method_map_t>;
    using base_class_info_map_t   = std::unordered_map<std::string_view, const base_class_info_base<T>*>;

    template <typename Members>
    static metadata& instance(const Members& members) {
        return mc::singleton<metadata>::instance_with_creator([&]() {
            return new metadata(members);
        });
    }

    static metadata& get() {
        if (auto* instance = mc::singleton<metadata>::try_get()) {
            return *instance;
        }

        MC_THROW(mc::not_implemented_exception, "metadata not initialized");
    }

    static metadata* try_get() {
        return mc::singleton<metadata>::try_get();
    }

    ~metadata() {
    }

private:
    template <typename, typename>
    friend class mc::reflect::reflection;

    template <typename Members>
    metadata(const Members& members) {
        init_properties(members);
        init_methods(members);
        init_base_class_info(members);
    }

    template <typename Members>
    void init_properties(const Members& members) {
        auto props = get_properties<T>(members);
        mc::traits::tuple_for_each(props, [&](auto* property) {
            // 如果基类有多个同名的属性，默认第一个名字就是当前类的属性
            if (property->is_override == 0 &&
                name_to_properties.find(property->name) == name_to_properties.end()) {
                name_to_properties[property->name]       = property;
                offset_to_properties[property->offset()] = property;
            }

            using property_type = std::decay_t<std::remove_pointer_t<decltype(property)>>;
            using base_type     = typename property_type::base_type;
            using class_type    = typename property_type::class_type;
            if constexpr (!std::is_same_v<base_type, class_type> &&
                          is_reflectable<base_type>()) {
                auto base_name = reflector<base_type>::get_name();

                base_class_prop_map[base_name][property->name] = property;
            }
        });
    }

    template <typename Members>
    void init_methods(const Members& members) {
        auto methods = get_methods<T>(members);
        mc::traits::tuple_for_each(methods, [&](auto* method) {
            // 如果基类有多个同名的方法，默认第一个名字就是当前类的方法
            if (method->is_override == 0 &&
                name_to_methods.find(method->name) == name_to_methods.end()) {
                name_to_methods[method->name] = method;
            }

            using method_type = std::decay_t<std::remove_pointer_t<decltype(method)>>;
            using base_type   = typename method_type::base_type;
            using class_type  = typename method_type::class_type;
            if constexpr (!std::is_same_v<base_type, class_type> &&
                          is_reflectable<base_type>()) {
                auto base_name = reflector<base_type>::get_name();

                base_class_method_map[base_name][method->name] = method;
            }
        });
    }

    template <typename Members>
    void init_base_class_info(const Members& members) {
        auto base_classes = get_base_classes<T>(members);
        mc::traits::tuple_for_each(base_classes, [&](auto* base_class_info) {
            base_class_info_map[base_class_info->name] = base_class_info;
        });
    }

    property_map_t          name_to_properties;
    property_offset_map_t   offset_to_properties;
    method_map_t            name_to_methods;
    base_class_prop_map_t   base_class_prop_map;
    base_class_method_map_t base_class_method_map;
    base_class_info_map_t   base_class_info_map;
};

class MC_API struct_metadata {
public:
    struct_metadata(std::string_view name, size_t property_count = 0);

    template <typename T, typename Members>
    struct_metadata(std::string_view name, const Members& members, const T*)
        : struct_metadata(name, std::tuple_size_v<decltype(get_properties<T>(members))>) {
        mc::traits::tuple_for_each(members, [&](auto& member) {
            using member_type = std::decay_t<decltype(member)>;
            if constexpr (std::is_base_of_v<method_type_info, member_type>) {
                add_method_info(&member);
            } else if constexpr (std::is_base_of_v<property_type_info, member_type>) {
                add_property_info(&member);
            } else if constexpr (std::is_base_of_v<base_class_type_info, member_type>) {
                add_base_class_info(&member);
            }
        });
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

    using property_visitor_t   = std::function<void(const property_type_info*)>;
    using method_visitor_t     = std::function<void(const method_type_info*)>;
    using base_class_visitor_t = std::function<void(const base_class_type_info*)>;

    void visit_property(const property_visitor_t& visitor) const;
    void visit_method(const method_visitor_t& visitor) const;
    void visit_base_class(const base_class_visitor_t& visitor) const;

private:
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