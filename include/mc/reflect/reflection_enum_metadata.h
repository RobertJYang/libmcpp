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

#ifndef MC_REFLECT_REFLECT_ENUM_METADATA_H
#define MC_REFLECT_REFLECT_ENUM_METADATA_H

#include <mc/reflect/base.h>
#include <mc/singleton.h>
#include <mc/traits.h>
#include <mc/variant.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mc::reflect {

/**
 * @brief 枚举反射元数据类
 *
 * 专门用于处理枚举类型的反射，提供枚举值的访问和转换功能
 */
template <typename EnumType>
class reflection_enum_metadata : public reflection_metadata_base {
    static_assert(std::is_enum_v<EnumType>, "EnumType must be an enum type");

public:
    using metadata_ptr = std::shared_ptr<reflection_enum_metadata<EnumType>>;

    /**
     * @brief 获取单例实例
     *
     * @return reflection_metadata<T>& 单例引用
     */
    static reflection_enum_metadata<EnumType>& instance() {
        return *instance_ptr();
    }

    static metadata_ptr& instance_ptr() {
        return mc::singleton<metadata_ptr>::instance_with_creator([]() {
            return new metadata_ptr(new reflection_enum_metadata<EnumType>());
        });
    }

    /**
     * @brief 通过名称获取枚举值
     * @param name 枚举值名称
     * @return uint64_t 枚举值
     */
    uint64_t get_enum_value(std::string_view name) const override {
        auto it = m_name_to_value.find(name);
        if (it != m_name_to_value.end()) {
            return static_cast<uint64_t>(it->second);
        }
        throw_enum_value_not_found(get_type_name(), name);
    }

    /**
     * @brief 通过枚举值获取名称
     * @param value 枚举值
     * @return std::string_view 枚举值名称
     */
    std::string_view get_enum_name(uint64_t value) const override {
        auto it = m_value_to_name.find(value);
        if (it != m_value_to_name.end()) {
            return it->second;
        }
        throw_enum_value_not_found(get_type_name(), value);
    }

    /**
     * @brief 获取所有枚举值名称
     * @return std::vector<std::string_view> 枚举值名称列表
     */
    std::vector<std::string_view> get_enum_names() const override {
        std::vector<std::string_view> names;
        names.reserve(m_value_to_name.size());
        for (const auto& [_, name] : m_value_to_name) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief 检查是否包含指定枚举值
     * @param name 枚举值名称
     * @return bool 是否包含
     */
    bool has_enum_value(std::string_view name) const override {
        return m_name_to_value.find(name) != m_name_to_value.end();
    }

    // 继承自 reflection_metadata_base 的接口实现
    std::shared_ptr<reflected_object> create_object() override {
        throw_enum_not_support_create_object(get_type_name());
    }

    std::string_view get_type_name() const override {
        return reflector<EnumType>::name();
    }

    std::vector<int> get_base_type_ids() const override {
        return {}; // 枚举类型没有基类
    }

    bool is_derived_from(int base_type_id) const override {
        return false; // 枚举类型不继承任何类型
    }

    const property_type_info* get_property_info(std::string_view name) const override {
        return nullptr; // 枚举类型没有属性
    }

    const method_type_info* get_method_info(std::string_view name) const override {
        return nullptr; // 枚举类型没有方法
    }

    std::vector<std::string_view> get_property_names() const override {
        return {}; // 枚举类型没有属性
    }

    std::vector<std::string_view> get_method_names() const override {
        return {}; // 枚举类型没有方法
    }

private:
    reflection_enum_metadata() {
        m_is_enum = true;
        initialize();
    }

    void initialize() {
        auto& members = reflector<EnumType>::get_members();
        mc::traits::tuple_for_each(members, [this](const auto& member) {
            add_enum_value(member.name, static_cast<uint64_t>(member.value));
        });
    }

    void add_enum_value(std::string_view name, uint64_t value) {
        m_name_to_value[name]  = value;
        m_value_to_name[value] = name;
    }

private:
    std::unordered_map<std::string_view, uint64_t> m_name_to_value;
    std::unordered_map<uint64_t, std::string_view> m_value_to_name;
};

} // namespace mc::reflect

#endif // MC_REFLECT_ENUM_METADATA_H