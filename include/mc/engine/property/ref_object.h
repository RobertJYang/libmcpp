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

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <mc/engine/base.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/variant.h>
#include <mc/variant/variant_extension.h>

namespace mc::engine {

// 前向声明
class abstract_object;
class abstract_interface;

// 引用对象类，实现弱引用语义
class ref_object : public variant_extension_base {
public:
    using object_finder_type = std::function<abstract_object*(const std::string&)>;

    ref_object(const std::string& object_name, object_finder_type finder = nullptr)
        : m_object_name(object_name), m_object_finder(finder) {
    }

    // 获取被引用对象的属性
    mc::variant get_property(const std::string_view property_name) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }
        return target_object->get_property(property_name);
    }

    // 获取被引用对象的接口属性
    mc::variant get_property(const std::string_view interface_name, const std::string_view property_name) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->get_property(property_name);
        }

        return target_object->get_property(property_name);
    }

    // 设置被引用对象的属性
    void set_property(const std::string_view property_name, const mc::variant& value) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在，无法设置属性: ${object_name}", ("object_name", m_object_name));
        }
        target_object->set_property(property_name, value);
    }

    // 设置被引用对象的接口属性
    void set_property(const std::string_view interface_name, const std::string_view property_name, const mc::variant& value) const {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在，无法设置属性: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            interface_obj->set_property(property_name, value);
        } else {
            target_object->set_property(property_name, value);
        }
    }

    invoke_result invoke(std::string_view method_name, const mc::variants& args) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        return target_object->invoke(method_name, args);
    }

    invoke_result invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->invoke(method_name, args);
        }

        return target_object->invoke(method_name, args);
    }

    async_result async_invoke(std::string_view method_name, const mc::variants& args = {}) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        return target_object->async_invoke(method_name, args);
    }

    async_result async_invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args = {}) {
        auto* target_object = find_related_object();
        if (target_object == nullptr) {
            MC_THROW(mc::invalid_op_exception, "引用对象不存在: ${object_name}", ("object_name", m_object_name));
        }

        if (!interface_name.empty()) {
            auto interface_obj = target_object->get_interface(interface_name);
            if (interface_obj == nullptr) {
                MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                         ("interface", interface_name)("object_name", m_object_name));
            }
            return interface_obj->async_invoke(method_name, args);
        }

        return target_object->async_invoke(method_name, args);
    }

    // 获取对象名称
    const std::string& get_object_name() const {
        return m_object_name;
    }

    // 检查被引用的对象是否存在
    bool is_valid() const {
        return find_related_object() != nullptr;
    }

    // 获取被引用的对象指针（可能为空）
    abstract_object* get_object() const {
        return find_related_object();
    }

    std::string as_string() const override {
        return m_object_name;
    }

    bool equals(const variant_extension_base& other) const override {
        if (auto* other_ref = dynamic_cast<const ref_object*>(&other)) {
            return m_object_name == other_ref->m_object_name;
        }
        return false;
    }

    // 实现 variant_extension_base 的纯虚函数
    mc::shared_ptr<variant_extension_base> clone() const override {
        return mc::make_shared<ref_object>(m_object_name, m_object_finder);
    }

    std::string_view get_type_name() const override {
        return "ref_object";
    }

private:
    std::string        m_object_name;
    object_finder_type m_object_finder;

    // 查找被引用的对象（弱引用，可能返回 nullptr）
    abstract_object* find_related_object() const {
        if (m_object_finder) {
            return m_object_finder(m_object_name);
        }
        return nullptr;
    }
};

} // namespace mc::engine 