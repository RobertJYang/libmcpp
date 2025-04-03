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

#include <mc/variant.h>

namespace mc {
namespace reflect {

template <typename T>
constexpr bool is_reflectable();

// 前置声明
template <typename T>
struct member_info;

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
     * @return const member_info<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const member_info<T>* get_member(std::string_view name) const {
        auto it = m_name_to_member.find(name);
        if (it != m_name_to_member.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief 获取指定偏移量的成员信息
     *
     * @param offset 成员偏移量
     * @return const member_info<T>* 成员信息指针，如果不存在则返回nullptr
     */
    const member_info<T>* get_member_by_offset(size_t offset) const {
        for (const auto& [name, member] : m_name_to_member) {
            if (member->offset == offset) {
                return member;
            }
        }
        return nullptr;
    }

    /**
     * @brief 获取成员属性值
     *
     * @param obj 对象实例
     * @param key 属性名
     * @return mc::variant 属性值，如果不存在返回mc::variant::null_type
     */
    mc::variant get_property(const T& obj, std::string_view key) const {
        const member_info<T>* member = get_member(key);
        if (member) {
            return member->getter(obj);
        }
        return mc::variant();
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
        const member_info<T>* member = get_member(key);
        if (member) {
            member->setter(obj, value);
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
    std::string_view get_member_name(size_t offset) const {
        const member_info<T>* member = get_member_by_offset(offset);
        if (member) {
            return member->name;
        }
        return "";
    }

    /**
     * @brief 获取所有成员信息
     *
     * @return const std::unordered_map<std::string_view, const member_info<T>*>&
     * 成员名称到成员信息的映射
     */
    const std::unordered_map<std::string_view, const member_info<T>*>& get_members() const {
        return m_name_to_member;
    }

private:
    // 私有构造函数，保证单例
    reflection_metadata() {
        initialize();
    }

    // 初始化元数据缓存
    void initialize() {
        const auto& members = reflector<T>::get_members();
        for (const auto& member : members) {
            m_name_to_member[member.name] = &member;
        }
    }

private:
    // 成员名称到成员信息的映射
    std::unordered_map<std::string_view, const member_info<T>*> m_name_to_member;
};

/**
 * @brief 获取类型的元数据
 *
 * @tparam T 类型
 * @return reflection_metadata<T>& 元数据引用
 */
template <typename T>
reflection_metadata<T>& get_metadata() {
    static_assert(is_reflectable<T>(), "类型必须支持反射才能使用get_metadata");
    return reflection_metadata<T>::instance();
}

/**
 * @brief 根据偏移量获取成员名称
 *
 * @tparam T 类型
 * @param offset 偏移量
 * @return std::string_view 成员名称
 */
template <typename T>
std::string_view get_member_name_by_offset(size_t offset) {
    return get_metadata<T>().get_member_name(offset);
}

/**
 * @brief 根据成员指针获取成员名称
 *
 * @tparam T 类型
 * @tparam M 成员类型
 * @param member 成员指针
 * @return std::string_view 成员名称
 */
template <typename T, typename M>
std::string_view get_member_name(M T::* member) {
    size_t offset = MC_MEMBER_OFFSETOF(member);
    return get_member_name_by_offset<T>(offset);
}

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_METADATA_H