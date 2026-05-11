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

#include <mc/engine/base.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/small_function.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/variant.h>
#include <mc/variant/variant_extension.h>
#include <memory>

namespace mc::engine {

// 前向声明
class abstract_object;
class abstract_interface;

// 引用对象类，实现弱引用语义
class MC_API ref_object : public variant_extension_base {
public:
    using object_finder_type = mc::small_function<abstract_object*(mc::string_view), 64>;
    struct adopt_shared_finder_t {
        explicit adopt_shared_finder_t() = default;
    };

    ref_object(mc::string_view object_name, object_finder_type finder = {});

    // 获取被引用对象的属性
    mc::variant get_property(mc::string_view property_name) const;

    // 获取被引用对象的接口属性
    mc::variant get_property(mc::string_view interface_name, mc::string_view property_name) const;

    // 设置被引用对象的属性
    void set_property(mc::string_view property_name, const mc::variant& value) const;

    // 设置被引用对象的接口属性
    void set_property(mc::string_view interface_name, mc::string_view property_name, const mc::variant& value) const;

    invoke_result invoke(mc::string_view method_name, const mc::variants& args);

    invoke_result invoke(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args);

    async_result async_invoke(mc::string_view method_name, const mc::variants& args = {});

    async_result async_invoke(mc::string_view interface_name, mc::string_view method_name, const mc::variants& args);

    // 获取对象名称
    const mc::string& get_object_name() const;

    // 检查被引用的对象是否存在
    bool is_valid() const;

    // 获取被引用的对象指针（可能为空）
    abstract_object* get_object() const;

    mc::string as_string() const override;

    bool equals(const variant_extension_base& other) const override;

    // 实现 variant_extension_base 的纯虚函数
    mc::shared_ptr<variant_extension_base> copy() const override;

    mc::string_view get_type_name() const override;

private:
    using shared_object_finder_type = std::shared_ptr<object_finder_type>;

    ref_object(mc::string_view object_name, shared_object_finder_type finder, adopt_shared_finder_t);

    mc::string                m_object_name;
    shared_object_finder_type m_object_finder;

    // 查找被引用的对象（弱引用，可能返回 nullptr）
    abstract_object* find_related_object() const;
};

} // namespace mc::engine