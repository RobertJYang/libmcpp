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
 * @file property.h
 * @brief 实现基于反射的属性访问元数据，提高反射性能
 */
#ifndef MC_REFLECT_PROPERTY_H
#define MC_REFLECT_PROPERTY_H

#include <functional>
#include <mc/variant.h>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::reflect {

// 前置声明
template <typename T>
constexpr bool is_reflectable();

template <typename T, typename Visitor>
void visit_members(const Visitor& visitor);

/**
 * 存储类型反射的元数据信息
 * @tparam T 对象类型
 */
template <typename T>
class reflection_metadata {
public:
    using getter_type = std::function<mc::variant(const T&)>;
    using setter_type = std::function<void(T&, const mc::variant&)>;

    /**
     * 获取单例实例
     */
    static reflection_metadata<T>& instance() {
        static reflection_metadata<T> inst;
        return inst;
    }

    /**
     * 获取属性值
     * @param obj 对象
     * @param name 属性名
     * @return 属性值的可选包装
     */
    std::optional<mc::variant> get_property(const T& obj, std::string_view name) const {
        auto it = m_getters.find(std::string(name));
        if (it != m_getters.end()) {
            return it->second(obj);
        }
        return std::nullopt;
    }

    /**
     * 设置属性值
     * @param obj 对象
     * @param name 属性名
     * @param value 属性值
     * @return 是否设置成功
     */
    bool set_property(T& obj, std::string_view name, const mc::variant& value) {
        auto it = m_setters.find(std::string(name));
        if (it != m_setters.end()) {
            it->second(obj, value);
            return true;
        }
        return false;
    }

    /**
     * 获取所有属性名
     * @return 属性名列表
     */
    std::vector<std::string> get_property_names() const {
        std::vector<std::string> names;
        names.reserve(m_getters.size());
        for (const auto& [name, _] : m_getters) {
            names.push_back(name);
        }
        return names;
    }

private:
    reflection_metadata() {
        initialize();
    }

    /**
     * 初始化元数据
     */
    void initialize() {
        if (m_initialized) {
            return;
        }

        if constexpr (is_reflectable<T>()) {
            visit_members<T>([this](const char* name, auto getter, auto setter) {
                m_getters[name] = [getter](const T& obj) -> mc::variant {
                    return getter(obj);
                };
                m_setters[name] = [setter](T& obj, const mc::variant& v) {
                    setter(obj, v);
                };
            });
        }

        m_initialized = true;
    }

    std::unordered_map<std::string, getter_type> m_getters;
    std::unordered_map<std::string, setter_type> m_setters;
    bool                                         m_initialized = false;
};

/**
 * 获取对象属性值的优化版本，自动使用元数据缓存
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @return 属性值的可选包装
 */
template <typename T>
std::optional<mc::variant> get_property(const T& obj, std::string_view key) {
    if constexpr (is_reflectable<T>()) {
        return reflection_metadata<T>::instance().get_property(obj, key);
    } else {
        static_assert(is_reflectable<T>(), "类型必须支持反射才能使用get_property");
        return std::nullopt; // 这行代码永远不会执行，只是为了满足编译器的返回值要求
    }
}

/**
 * 设置对象属性值的优化版本，自动使用元数据缓存
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @param value 属性值
 * @return 是否设置成功
 */
template <typename T>
bool set_property(T& obj, std::string_view key, const mc::variant& value) {
    if constexpr (is_reflectable<T>()) {
        return reflection_metadata<T>::instance().set_property(obj, key, value);
    } else {
        static_assert(is_reflectable<T>(), "类型必须支持反射才能使用set_property");
        return false; // 这行代码永远不会执行，只是为了满足编译器的返回值要求
    }
}

/**
 * 获取对象属性值的优化版本，自动使用元数据缓存 (保留向后兼容)
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @return 属性值的可选包装
 * @deprecated 请使用 get_property 代替
 */
template <typename T>
std::optional<mc::variant> get_cached_property(const T& obj, const std::string& key) {
    return get_property(obj, key);
}

/**
 * 设置对象属性值的优化版本，自动使用元数据缓存 (保留向后兼容)
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @param value 属性值
 * @return 是否设置成功
 * @deprecated 请使用 set_property 代替
 */
template <typename T>
bool set_cached_property(T& obj, const std::string& key, const mc::variant& value) {
    return set_property(obj, key, value);
}

} // namespace mc::reflect

#endif // MC_REFLECT_PROPERTY_H