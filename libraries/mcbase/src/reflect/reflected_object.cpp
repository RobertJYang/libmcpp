/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/reflect/reflection.h>

namespace mc::reflect::detail {

reflected_object_bridge::reflected_object_bridge(const reflected_object_ops& ops) noexcept : m_ops(&ops)
{}

type_id_type reflected_object_bridge::get_type_id() const
{
    return m_ops->get_reflection().get_type_id();
}

variant reflected_object_bridge::get_property(mc::string_view name) const
{
    const auto* property = m_ops->get_reflection().get_property_info(name);
    if (property) {
        return property->get_value(m_ops->get_const_object(this));
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

void reflected_object_bridge::set_property(mc::string_view name, const variant& value)
{
    const auto* property = m_ops->get_reflection().get_property_info(name);
    if (property) {
        property->set_value(m_ops->get_object(this), value);
        return;
    }
    MC_THROW(mc::bad_property_exception, "属性不存在: ${name}", ("name", name));
}

variant reflected_object_bridge::invoke_method(mc::string_view name, const std::vector<variant>& args)
{
    const auto* method = m_ops->get_reflection().get_method_info(name);
    if (method) {
        return method->invoke(m_ops->get_object(this), args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

async_result reflected_object_bridge::async_invoke_method(mc::string_view name, const std::vector<variant>& args)
{
    const auto* method = m_ops->get_reflection().get_method_info(name);
    if (method) {
        return method->async_invoke(m_ops->get_object(this), args);
    }
    MC_THROW(mc::bad_method_exception, "方法不存在: ${name}", ("name", name));
}

std::vector<mc::string_view> reflected_object_bridge::get_property_names() const
{
    return m_ops->get_reflection().get_property_names();
}

std::vector<mc::string_view> reflected_object_bridge::get_method_names() const
{
    return m_ops->get_reflection().get_method_names();
}

const reflected_object_ops& reflected_object_bridge::get_ops() const noexcept
{
    return *m_ops;
}

} // namespace mc::reflect::detail
