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

#include <mc/reflect/metadata_info.h>
#include <mc/variant.h>

namespace mc {
namespace reflect {
void throw_method_not_exist(std::string_view method_name);

template <typename T>
constexpr bool is_reflectable();

template <typename T>
struct reflector;

/**
 * @brief 类型反射元数据缓存
 *
 * 缓存一个类型的反射元数据，提供高效的成员查找功能
 *
 * @tparam T 要缓存元数据的类型
 */
template <typename T>
class reflection_metadata {
public:
    using property_map        = std::unordered_map<std::string_view, const property_info_base<T>*>;
    using property_offset_map = std::unordered_map<size_t, const property_info_base<T>*>;
    using method_map          = std::unordered_map<std::string_view, const method_info_base<T>*>;

    /**
     * @brief 获取单例实例
     *
     * @return reflection_metadata<T>& 单例引用
     */
    static reflection_metadata<T>& instance() {
        static reflection_metadata<T> instance;
        return instance;
    }

    /**
     * @brief 获取指定名称的成员信息
     *
     * @param name 成员名称
     * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const property_info_base<T>* get_property_info(std::string_view name) const {
        auto it = m_name_to_properties.find(name);
        return it != m_name_to_properties.end() ? it->second : nullptr;
    }

    /**
     * @brief 获取指定名称的方法信息
     *
     * @param name 方法名称
     * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
     */
    const method_info_base<T>* get_method_info(std::string_view name) const {
        auto it = m_name_to_methods.find(name);
        return it != m_name_to_methods.end() ? it->second : nullptr;
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
        return mc::variant(); // 不会执行到这里
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

private:
    // 私有构造函数，保证单例
    reflection_metadata() {
        initialize();
    }

    // 初始化元数据缓存
    void initialize() {
        // 初始化属性
        {
            auto visitor = [&](auto&... member) {
                (add_property(member), ...);
            };

            const auto& properties = reflector<T>::get_properties();
            std::apply(visitor, properties);
        }

        // 初始化方法
        {
            auto visitor = [&](auto&... member) {
                (add_method(member), ...);
            };

            const auto& methods = reflector<T>::get_methods();
            std::apply(visitor, methods);
        }
    }

    void add_property(const property_info_base<T>& property) {
        m_name_to_properties[property.name]       = &property;
        m_offset_to_properties[property.offset()] = &property;
    }

    void add_method(const method_info_base<T>& method) {
        m_name_to_methods[method.name] = &method;
    }

private:
    property_map        m_name_to_properties;
    property_offset_map m_offset_to_properties;
    method_map          m_name_to_methods;
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
    static_assert(is_reflectable<clean_type>(), "类型必须支持反射才能使用get_metadata");
    return reflection_metadata<clean_type>::instance();
}

/**
 * @brief 获取指定名称的方法信息
 *
 * @tparam T 类型
 * @param method_name 方法名称
 * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
 */
template <typename T>
const method_info_base<T>* get_method_info(std::string_view method_name) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_method_info(method_name);
}

/**
 * @brief 获取指定名称的成员信息
 *
 * @tparam T 类型
 * @param name 成员名称
 * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
 */
template <typename T>
const property_info_base<T>* get_property_info(std::string_view name) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_info(name);
}

/**
 * @brief 根据偏移量获取成员名称
 *
 * @tparam T 类型
 * @param offset 偏移量
 * @return std::string_view 成员名称
 */
template <typename T>
std::string_view get_property_name(size_t offset) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_name(offset);
}

/**
 * @brief 根据成员指针获取成员名称
 *
 * @tparam T 类型
 * @tparam M 成员类型
 * @param member 成员指针
 * @return std::string_view 成员名称
 */
template <typename T, typename M, typename BaseT>
std::string_view get_property_name(M BaseT::* member) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_name(member);
}

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_METADATA_H