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
template <typename T>
class reflection<T, std::enable_if_t<is_reflectable<T>() && std::is_enum<T>()>> : public reflection_base {
public:
    using reflection_ptr = mc::shared_ptr<reflection<T>>;

    ~reflection();

    /**
     * @brief 通过名称获取枚举值
     * @param name 枚举值名称
     * @return uint64_t 枚举值
     */
    uint64_t get_enum_value(std::string_view name) const override {
        return m_data.get_value(name);
    }

    /**
     * @brief 通过枚举值获取名称
     * @param value 枚举值
     * @return std::string_view 枚举值名称
     */
    std::string_view get_enum_name(uint64_t value) const override {
        return m_data.get_name(value);
    }

    /**
     * @brief 获取所有枚举值名称
     * @return std::vector<std::string_view> 枚举值名称列表
     */
    std::vector<std::string_view> get_enum_names() const override {
        return m_data.get_names();
    }

    /**
     * @brief 检查是否包含指定枚举值
     * @param name 枚举值名称
     * @return bool 是否包含
     */
    bool has_enum_value(std::string_view name) const override {
        return m_data.has_value(name);
    }

    bool has_enum_value(uint64_t value) const override {
        return m_data.has_value(value);
    }

    // 继承自 reflection_base 的接口实现
    std::shared_ptr<reflected_object> create_object() override {
        throw_enum_not_support_create_object(get_type_name());
    }

    std::string_view get_type_name() const override {
        return reflector<T>::get_name();
    }

    type_id_type get_type_id() const override {
        return reflector<T>::get_type_id();
    }

    std::vector<type_id_type> get_base_type_ids() const override {
        return {}; // 枚举类型没有基类
    }

    bool is_derived_from(type_id_type base_type_id) const override {
        return false; // 枚举类型不继承任何类型
    }

    const property_type_info* get_property_info(std::string_view name) const override {
        return nullptr; // 枚举类型没有属性
    }

    const method_type_info* get_method_info(std::string_view name) const override {
        return nullptr; // 枚举类型没有方法
    }

    const base_class_type_info* get_base_class_info(std::string_view name) const override {
        return nullptr; // 枚举类型没有基类
    }

    const member_info_base* get_custom_info(std::string_view name, size_t reflect_type) const override {
        return nullptr; // 枚举类型没有自定义信息
    }

    std::vector<std::string_view> get_property_names() const override {
        return {}; // 枚举类型没有属性
    }

    std::vector<std::string_view> get_method_names() const override {
        return {}; // 枚举类型没有方法
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

    reflection() : m_data(reflector<T>::get_metadata()) {
        m_is_enum = true;
    }

private:
    const enum_metadata& m_data;
};

} // namespace mc::reflect

#endif // MC_REFLECT_ENUM_METADATA_H