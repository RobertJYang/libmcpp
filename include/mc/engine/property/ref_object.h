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
#include <mc/engine/base.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/variant.h>
#include <mc/variant/variant_extension.h>
#include <string>
#include <string_view>

namespace mc::engine {

// 前向声明
class abstract_object;
class abstract_interface;

// 引用对象类，实现弱引用语义
class MC_API ref_object : public variant_extension_base {
public:
    using object_finder_type = std::function<abstract_object*(const std::string&)>;

    ref_object(const std::string& object_name, object_finder_type finder = nullptr);

    // 获取被引用对象的属性
    mc::variant get_property(const std::string_view property_name) const;

    // 获取被引用对象的接口属性
    mc::variant get_property(const std::string_view interface_name, const std::string_view property_name) const;

    // 设置被引用对象的属性
    void set_property(const std::string_view property_name, const mc::variant& value) const;

    // 设置被引用对象的接口属性
    void set_property(const std::string_view interface_name, const std::string_view property_name,
                      const mc::variant& value) const;

    invoke_result invoke(std::string_view method_name, const mc::variants& args);

    invoke_result invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args);

    async_result async_invoke(std::string_view method_name, const mc::variants& args = {});

    async_result async_invoke(const std::string& interface_name, std::string_view method_name,
                              const mc::variants& args = {});

    // 获取对象名称
    const std::string& get_object_name() const;

    // 检查被引用的对象是否存在
    bool is_valid() const;

    // 获取被引用的对象指针（可能为空）
    abstract_object* get_object() const;

    std::string as_string() const override;

    bool equals(const variant_extension_base& other) const override;

    // 实现 variant_extension_base 的纯虚函数
    mc::shared_ptr<variant_extension_base> copy() const override;

    std::string_view get_type_name() const override;

private:
    std::string        m_object_name;
    object_finder_type m_object_finder;

    // 查找被引用的对象（弱引用，可能返回 nullptr）
    abstract_object* find_related_object() const;
};

} // namespace mc::engine