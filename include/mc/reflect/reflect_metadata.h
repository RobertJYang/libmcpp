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
 * @file reflect_metadata.h
 * @brief 反射元数据缓存
 */
#ifndef MC_REFLECT_METADATA_H
#define MC_REFLECT_METADATA_H

#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include <mc/dict.h>
#include <mc/memory.h>
#include <mc/reflect/metadata_info.h>
#include <mc/singleton.h>
#include <mc/variant.h>

namespace mc::reflect {

template <typename T>
constexpr bool is_reflectable();

template <typename T>
struct reflector;

/**
 * @brief 反射对象包装器实现
 *
 * 包装实际对象，提供动态访问接口
 *
 * @tparam T 被包装的对象类型
 */
template <typename T>
class reflected_object_impl : public reflected_object {
public:
    explicit reflected_object_impl(std::shared_ptr<T> obj);

    int get_type_id() const override;

    variant get_property(std::string_view name) const override;
    void    set_property(std::string_view name, const variant& value) override;

    variant      invoke_method(std::string_view name, const std::vector<variant>& args) override;
    async_result async_invoke_method(std::string_view name, const std::vector<variant>& args) override;

    std::vector<std::string_view> get_property_names() const override;
    std::vector<std::string_view> get_method_names() const override;

    std::shared_ptr<T> get_object() const;

private:
    std::shared_ptr<T> m_obj;
};

/**
 * @brief 类型反射元数据缓存
 *
 * 缓存一个类型的反射元数据，提供高效的成员查找功能
 *
 * @tparam T 要缓存元数据的类型
 */
template <typename T>
class reflection_metadata : public reflection_metadata_base {
public:
    using property_map          = std::unordered_map<std::string_view, const property_info_base<T>*>;
    using property_offset_map   = std::unordered_map<size_t, const property_info_base<T>*>;
    using method_map            = std::unordered_map<std::string_view, const method_info_base<T>*>;
    using base_class_prop_map   = std::unordered_map<std::string_view, property_map>;
    using base_class_method_map = std::unordered_map<std::string_view, method_map>;
    using base_class_info_map =
        std::unordered_map<std::string_view, const base_class_info_base<T>*>;

    /**
     * @brief 获取单例实例
     *
     * @return reflection_metadata<T>& 单例引用
     */
    static reflection_metadata<T>& instance() {
        return *instance_ptr();
    }

    static std::shared_ptr<reflection_metadata<T>>& instance_ptr() {
        return mc::singleton<std::shared_ptr<reflection_metadata<T>>>::instance_with_creator([]() {
            return new std::shared_ptr<reflection_metadata<T>>(new reflection_metadata<T>());
        });
    }

    /**
     * @brief 获取指定名称的成员信息
     *
     * @param name 成员名称
     * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const property_info_base<T>* get_property_info(std::string_view name) const override {
        auto it = m_name_to_properties.find(name);
        return it != m_name_to_properties.end() ? it->second : nullptr;
    }

    /**
     * @brief 获取指定名称的方法信息
     *
     * @param name 方法名称
     * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
     */
    const method_info_base<T>* get_method_info(std::string_view name) const override {
        auto it = m_name_to_methods.find(name);
        return it != m_name_to_methods.end() ? it->second : nullptr;
    }

    template <typename M>
    const method_info_base<T>* get_method_info(M T::* member_ptr) const {
        auto offset = get_function_offset(member_ptr);
        for (auto& [name, method] : m_name_to_methods) {
            if (method->offset() == offset) {
                return method;
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取指定偏移量的成员信息
     *
     * @param offset 成员偏移量
     * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const property_info_base<T>* get_property_info(size_t offset) const {
        auto it = m_offset_to_properties.find(offset);
        return it != m_offset_to_properties.end() ? it->second : nullptr;
    }

    template <typename M, typename BaseT,
              typename = std::enable_if_t<std::is_same_v<T, BaseT> || std::is_base_of_v<BaseT, T>>>
    const property_info_base<T>* get_property_info(M BaseT::* member) const {
        return get_property_info(MC_MEMBER_OFFSETOF(T, member));
    }

    /**
     * @brief 获取成员属性值
     *
     * @param obj 对象实例
     * @param key 属性名
     * @return mc::variant 属性值，如果不存在返回mc::variant::null_type
     */
    mc::variant get_property(const T& obj, std::string_view key) const {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            return property->get_value(obj);
        }
        return mc::variant();
    }

    /**
     * @brief 获取成员属性值
     *
     * @param obj 对象实例
     * @param key 属性名
     * @param base_class_name 基类名称
     * @return mc::variant 属性值，如果不存在返回mc::variant::null_type
     */
    mc::variant get_property(const T& obj, std::string_view key,
                             std::string_view base_class_name) const {
        auto it = m_base_class_info_map.find(base_class_name);
        if (it != m_base_class_info_map.end()) {
            return it->second->get_value(obj, key);
        }

        return mc::variant();
    }

    /**
     * @brief 获取对象所有属性值
     *
     * @param obj 对象实例
     * @return mc::dict 所有属性值
     */
    mc::dict get_all_properties(const T& obj) const {
        mc::mutable_dict result;
        for (const auto& [name, property] : m_name_to_properties) {
            result[name] = property->get_value(obj);
        }
        return result;
    }

    /**
     * @brief 调用对象的方法
     *
     * @param obj 对象实例
     * @param method_name 方法名称
     * @param args 方法参数
     * @return mc::variant 方法返回值，如果方法不存在则返回mc::variant::null_type
     */
    mc::variant invoke_method(T& obj, std::string_view method_name,
                              const mc::variants& args = {}) const {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method) {
            return method->invoke(obj, args);
        }

        throw_method_not_exist(method_name);
    }

    mc::variant invoke_method(std::string_view method_name, const mc::variants& args = {}) const {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method && method->is_static()) {
            return method->invoke(args);
        }

        throw_method_not_exist(method_name);
    }

    async_result async_invoke_method(T& obj, std::string_view method_name,
                                     const mc::variants& args = {}) const {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method) {
            return method->async_invoke(obj, args);
        }

        throw_method_not_exist(method_name);
    }

    async_result async_invoke_method(std::string_view method_name, const mc::variants& args = {}) const {
        const method_info_base<T>* method = get_method_info(method_name);
        if (method && method->is_static()) {
            return method->async_invoke(args);
        }

        throw_method_not_exist(method_name);
    }

    /**
     * @brief 设置成员属性值
     *
     * @param obj 对象实例
     * @param key 属性名
     * @param value 属性值
     * @return bool 设置是否成功
     */
    bool set_property(T& obj, std::string_view key, const mc::variant& value) const {
        const property_info_base<T>* property = get_property_info(key);
        if (property) {
            property->set_value(obj, value);
            return true;
        }
        return false;
    }

    /**
     * @brief 设置成员属性值
     *
     * @param obj 对象实例
     * @param key 属性名
     * @param base_class_name 基类名称
     * @param value 属性值
     * @return bool 设置是否成功
     */
    bool set_property(T& obj, std::string_view key, std::string_view base_class_name,
                      const mc::variant& value) const {
        auto it = m_base_class_info_map.find(base_class_name);
        if (it != m_base_class_info_map.end()) {
            it->second->set_value(obj, key, value);
            return true;
        }
        return false;
    }

    /**
     * @brief 获取成员名称
     *
     * @param offset 成员偏移量
     * @return std::string_view 成员名称，如果不存在则返回空字符串
     */
    std::string_view get_property_name(size_t offset) const {
        const property_info_base<T>* property = get_property_info(offset);
        if (property) {
            return property->name;
        }
        return {};
    }

    template <typename M, typename BaseT,
              typename = std::enable_if_t<std::is_same_v<T, BaseT> || std::is_base_of_v<BaseT, T>>>
    std::string_view get_property_name(M BaseT::* member) {
        return get_property_name(MC_MEMBER_OFFSETOF(T, member));
    }

    /**
     * @brief 获取所有成员信息
     *
     * @return const property_map& 成员名称到成员信息的映射
     */
    const property_map& get_properties() const {
        return m_name_to_properties;
    }

    /**
     * @brief 获取所有方法信息
     *
     * @return const method_map& 方法名称到方法信息的映射
     */
    const method_map& get_methods() const {
        return m_name_to_methods;
    }

    reflected_object_ptr create_object() override {
        if constexpr (std::is_abstract_v<T>) {
            MC_THROW(mc::bad_type_exception, "抽象类型不能创建对象: ${type}", ("type", get_type_name()));
        } else {
            return reflected_object_ptr{new reflected_object_impl<T>(std::make_shared<T>())};
        }
    }

    std::string_view get_type_name() const override {
        return reflector<T>::name();
    }

    std::vector<int> get_base_type_ids() const override {
        std::vector<int> base_ids;
        auto             base_classes = reflector<T>::get_base_classes();
        mc::traits::tuple_for_each(base_classes, [&](auto& base) {
            using base_type = typename std::decay_t<decltype(base)>::base_type;
            base_ids.push_back(reflector<base_type>::type_id);
        });
        return base_ids;
    }

    bool is_derived_from(int base_type_id) const override {
        auto base_ids = get_base_type_ids();
        return std::find(base_ids.begin(), base_ids.end(), base_type_id) != base_ids.end();
    }

    std::vector<std::string_view> get_property_names() const override {
        std::vector<std::string_view> names;
        for (const auto& [name, _] : m_name_to_properties) {
            names.push_back(name);
        }
        return names;
    }

    std::vector<std::string_view> get_method_names() const override {
        std::vector<std::string_view> names;
        for (const auto& [name, _] : m_name_to_methods) {
            names.push_back(name);
        }
        return names;
    }

private:
    reflection_metadata() {
        initialize();
    }

    void initialize() {
        init_properties();
        init_methods();
        init_base_class_info();
    }

    void init_properties() {
        auto& props = reflector<T>::get_properties();
        mc::traits::tuple_for_each(props, [&](auto& property) {
            // 如果基类有多个同名的属性，默认第一个名字就是当前类的属性
            if (property.is_override == 0 &&
                m_name_to_properties.find(property.name) == m_name_to_properties.end()) {
                m_name_to_properties[property.name]       = &property;
                m_offset_to_properties[property.offset()] = &property;
            }

            using property_type = std::decay_t<decltype(property)>;
            using base_type     = typename property_type::base_type;
            using class_type    = typename property_type::class_type;
            if constexpr (!std::is_same_v<base_type, class_type>) {
                auto base_name                                  = mc::reflect::reflector<base_type>::name();
                m_base_class_prop_map[base_name][property.name] = &property;
            }
        });
    }

    void init_methods() {
        mc::traits::tuple_for_each(reflector<T>::get_methods(), [&](auto& method) {
            // 如果基类有多个同名的方法，默认第一个名字就是当前类的方法
            if (method.is_override == 0 &&
                m_name_to_methods.find(method.name) == m_name_to_methods.end()) {
                m_name_to_methods[method.name] = &method;
            }

            using method_type = std::decay_t<decltype(method)>;
            using base_type   = typename method_type::base_type;
            using class_type  = typename method_type::class_type;
            if constexpr (!std::is_same_v<base_type, class_type> &&
                          mc::reflect::is_reflectable<base_type>()) {
                auto base_name                                  = mc::reflect::reflector<base_type>::name();
                m_base_class_method_map[base_name][method.name] = &method;
            }
        });
    }

    void init_base_class_info() {
        const auto& base_classes = reflector<T>::get_base_classes();
        mc::traits::tuple_for_each(base_classes, [&](auto& base_class_info) {
            m_base_class_info_map[base_class_info.name] = &base_class_info;
        });
    }

private:
    property_map          m_name_to_properties;
    property_offset_map   m_offset_to_properties;
    method_map            m_name_to_methods;
    base_class_prop_map   m_base_class_prop_map;
    base_class_method_map m_base_class_method_map;
    base_class_info_map   m_base_class_info_map;
};

/**
 * @brief 获取类型的元数据
 *
 * @tparam T 类型
 * @return reflection_metadata<T>& 元数据引用
 */
template <typename T>
reflection_metadata<std::remove_cv_t<std::remove_reference_t<T>>>& get_metadata() {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(is_reflectable<clean_type>(), "类型必须支持反射才能使用");
    return reflection_metadata<clean_type>::instance();
}

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::get_value(const C& obj, std::string_view name) const {
    return reflection_metadata<BaseT>::instance().get_property(obj, name);
}

template <typename C, typename BaseT>
void base_class_info<C, BaseT>::set_value(C& obj, std::string_view name,
                                          const mc::variant& value) const {
    reflection_metadata<BaseT>::instance().set_property(obj, name, value);
}

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::invoke(C& obj, std::string_view name,
                                              const mc::variants& args) const {
    return reflection_metadata<BaseT>::instance().invoke_method(obj, name, args);
}

template <typename C, typename BaseT>
async_result base_class_info<C, BaseT>::async_invoke(C& obj, std::string_view name,
                                                     const mc::variants& args) const {
    return reflection_metadata<BaseT>::instance().async_invoke_method(obj, name, args);
}

template <typename T>
reflected_object_impl<T>::reflected_object_impl(std::shared_ptr<T> obj)
    : m_obj(obj) {
}

template <typename T>
int reflected_object_impl<T>::get_type_id() const {
    return reflector<T>::type_id;
}

template <typename T>
variant reflected_object_impl<T>::get_property(std::string_view name) const {
    auto prop_info = reflection_metadata<T>::instance().get_property_info(name);
    if (prop_info) {
        return prop_info->get_value(*m_obj);
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

template <typename T>
void reflected_object_impl<T>::set_property(std::string_view name, const variant& value) {
    auto prop_info = reflection_metadata<T>::instance().get_property_info(name);
    if (prop_info) {
        prop_info->set_value(*m_obj, value);
        return;
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

template <typename T>
variant reflected_object_impl<T>::invoke_method(std::string_view name, const std::vector<variant>& args) {
    auto method_info = reflection_metadata<T>::instance().get_method_info(name);
    if (method_info) {
        return method_info->invoke(*m_obj, args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

template <typename T>
async_result reflected_object_impl<T>::async_invoke_method(std::string_view name, const std::vector<variant>& args) {
    auto method_info = reflection_metadata<T>::instance().get_method_info(name);
    if (method_info) {
        return method_info->async_invoke(*m_obj, args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

template <typename T>
std::vector<std::string_view> reflected_object_impl<T>::get_property_names() const {
    return reflection_metadata<T>::instance().get_property_names();
}

template <typename T>
std::vector<std::string_view> reflected_object_impl<T>::get_method_names() const {
    return reflection_metadata<T>::instance().get_method_names();
}

template <typename T>
std::shared_ptr<T> reflected_object_impl<T>::get_object() const {
    return m_obj;
}

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_H