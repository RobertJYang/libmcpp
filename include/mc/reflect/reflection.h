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
 * @file reflection.h
 * @brief 反射元数据缓存
 */
#ifndef MC_REFLECTION_METADATA_H
#define MC_REFLECTION_METADATA_H

#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include <mc/dict.h>
#include <mc/memory.h>
#include <mc/reflect/metadata.h>
#include <mc/reflect/metadata_info.h>
#include <mc/singleton.h>
#include <mc/variant.h>

namespace mc::reflect {

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

    type_id_type get_type_id() const override;

    variant get_property(std::string_view name) const override;
    void    set_property(std::string_view name, const variant& value) override;

    variant      invoke_method(std::string_view name, const std::vector<variant>& args) override;
    async_result async_invoke_method(std::string_view name, const std::vector<variant>& args) override;

    std::vector<std::string_view> get_property_names() const override;
    std::vector<std::string_view> get_method_names() const override;

    const std::shared_ptr<T>& get_object() const;

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
class reflection<T, std::enable_if_t<is_reflectable<T>() && !std::is_enum<T>()>> : public reflection_base {
public:
    using reflection_ptr = mc::shared_ptr<reflection<T>>;

    ~reflection() override;

    /**
     * @brief 获取指定名称的成员信息
     *
     * @param name 成员名称
     * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const property_info_base<T>* get_property_info(std::string_view name) const override {
        return static_cast<const property_info_base<T>*>(m_struct_data.get_property_info(name));
    }

    /**
     * @brief 获取指定名称的方法信息
     *
     * @param name 方法名称
     * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
     */
    const method_info_base<T>* get_method_info(std::string_view name) const override {
        return static_cast<const method_info_base<T>*>(m_struct_data.get_method_info(name));
    }

    template <typename M>
    const method_info_base<T>* get_method_info(M T::* member_ptr) const {
        auto offset = get_function_offset(member_ptr);
        return static_cast<const method_info_base<T>*>(m_struct_data.get_method_info(offset));
    }

    /**
     * @brief 获取指定偏移量的成员信息
     *
     * @param offset 成员偏移量
     * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const property_info_base<T>* get_property_info(size_t offset) const {
        return static_cast<const property_info_base<T>*>(m_struct_data.get_property_info(offset));
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
        auto it = m_data.base_class_info_map.find(base_class_name);
        if (it != m_data.base_class_info_map.end()) {
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
        for (const auto& [name, property] : m_data.name_to_properties) {
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
        auto it = m_data.base_class_info_map.find(base_class_name);
        if (it != m_data.base_class_info_map.end()) {
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
    const auto& get_properties() const {
        return m_data.name_to_properties;
    }

    /**
     * @brief 获取所有方法信息
     *
     * @return const method_map& 方法名称到方法信息的映射
     */
    const auto& get_methods() const {
        return m_data.name_to_methods;
    }

    reflected_object_ptr create_object() override {
        if constexpr (std::is_abstract_v<T>) {
            MC_THROW(mc::bad_type_exception, "抽象类型不能创建对象: ${type}", ("type", get_type_name()));
        } else {
            return reflected_object_ptr{new reflected_object_impl<T>(std::make_shared<T>())};
        }
    }

    std::string_view get_type_name() const override {
        return reflector<T>::get_name();
    }

    type_id_type get_type_id() const override {
        return reflector<T>::get_type_id();
    }

    std::vector<type_id_type> get_base_type_ids() const override {
        std::vector<type_id_type> base_ids;
        for (const auto& [name, base_class_info] : m_data.base_class_info_map) {
            base_ids.push_back(base_class_info->get_type_id());
        }
        return base_ids;
    }

    bool is_derived_from(type_id_type base_type_id) const override {
        auto base_ids = get_base_type_ids();
        return std::find(base_ids.begin(), base_ids.end(), base_type_id) != base_ids.end();
    }

    std::vector<std::string_view> get_property_names() const override {
        std::vector<std::string_view> names;
        for (const auto& [name, _] : m_data.name_to_properties) {
            names.push_back(name);
        }
        return names;
    }

    std::vector<std::string_view> get_method_names() const override {
        std::vector<std::string_view> names;
        for (const auto& [name, _] : m_data.name_to_methods) {
            names.push_back(name);
        }
        return names;
    }

    reflection_ptr shared_from_this() {
        return reflection_ptr(this);
    }

private:
    template <typename, typename>
    friend struct reflector; // 需要访问 instance

    static reflection<T>& instance() {
        return *instance_ptr();
    }

    static reflection_ptr& instance_ptr() {
        return mc::singleton<reflection_ptr>::instance_with_creator([]() {
            return new reflection_ptr(new reflection<T>());
        });
    }

    reflection()
        : m_data(reflector<T>::get_metadata()),
          m_struct_data(reflector<T>::get_struct_metadata()) {
    }

    detail::metadata<T>&     m_data;
    detail::struct_metadata& m_struct_data;
};

/**
 * @brief 获取类型的元数据
 *
 * @tparam T 类型
 * @return reflection<T>& 元数据引用
 */
template <typename T>
auto& get_reflection() {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(is_reflectable<clean_type>(), "类型必须支持反射才能使用");
    return reflector<clean_type>::get_reflection();
}

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::get_value(const C& obj, std::string_view name) const {
    return get_reflection<BaseT>().get_property(obj, name);
}

template <typename C, typename BaseT>
void base_class_info<C, BaseT>::set_value(C& obj, std::string_view name,
                                          const mc::variant& value) const {
    get_reflection<BaseT>().set_property(obj, name, value);
}

template <typename C, typename BaseT>
mc::variant base_class_info<C, BaseT>::invoke(C& obj, std::string_view name,
                                              const mc::variants& args) const {
    return get_reflection<BaseT>().invoke_method(obj, name, args);
}

template <typename C, typename BaseT>
async_result base_class_info<C, BaseT>::async_invoke(C& obj, std::string_view name,
                                                     const mc::variants& args) const {
    return get_reflection<BaseT>().async_invoke_method(obj, name, args);
}

template <typename T>
reflected_object_impl<T>::reflected_object_impl(std::shared_ptr<T> obj)
    : m_obj(std::move(obj)) {
}

template <typename T>
type_id_type reflected_object_impl<T>::get_type_id() const {
    return reflector<T>::get_type_id();
}

template <typename T>
variant reflected_object_impl<T>::get_property(std::string_view name) const {
    auto prop_info = get_reflection<T>().get_property_info(name);
    if (prop_info) {
        return prop_info->get_value(*m_obj);
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

template <typename T>
void reflected_object_impl<T>::set_property(std::string_view name, const variant& value) {
    auto prop_info = get_reflection<T>().get_property_info(name);
    if (prop_info) {
        prop_info->set_value(*m_obj, value);
        return;
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

template <typename T>
variant reflected_object_impl<T>::invoke_method(std::string_view name, const std::vector<variant>& args) {
    auto method_info = get_reflection<T>().get_method_info(name);
    if (method_info) {
        return method_info->invoke(*m_obj, args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

template <typename T>
async_result reflected_object_impl<T>::async_invoke_method(std::string_view name, const std::vector<variant>& args) {
    auto method_info = get_reflection<T>().get_method_info(name);
    if (method_info) {
        return method_info->async_invoke(*m_obj, args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

template <typename T>
std::vector<std::string_view> reflected_object_impl<T>::get_property_names() const {
    return get_reflection<T>().get_property_names();
}

template <typename T>
std::vector<std::string_view> reflected_object_impl<T>::get_method_names() const {
    return get_reflection<T>().get_method_names();
}

template <typename T>
const std::shared_ptr<T>& reflected_object_impl<T>::get_object() const {
    return m_obj;
}

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_H