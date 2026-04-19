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

#include <mc/engine/event.h>
#include <mc/exception.h>

namespace mc::engine {

property_changed_event::property_changed_event(const mc::variant& value, const property_base& property)
    : mc::event(property_changed_event_id), m_value(value), m_property(property)
{}

property_changed_event::~property_changed_event() = default;

method_invoke_event::method_invoke_event(context& ctx, mc::string_view method_name, const mc::variants& args,
                                         mc::string_view interface_name, bool is_async) noexcept
    : mc::event(method_invoke_event_id), m_context(ctx), m_method_name(method_name), m_args(args),
      m_interface_name(interface_name), m_is_async(is_async)
{}

method_invoke_event::~method_invoke_event() = default;

void method_invoke_event::set_async_result(mc::result<mc::variant>&& r)
{
    MC_ASSERT(m_is_async, "set_async_result 仅用于 async_invoke 路径");
    m_async_result.emplace(std::move(r));
    accept();
}

mc::result<mc::variant> method_invoke_event::take_async_result()
{
    MC_ASSERT(has_async_result(), "take_async_result 需要已设置异步覆盖结果");
    mc::result<mc::variant> out = std::move(*m_async_result);
    m_async_result.reset();
    return out;
}

} // namespace mc::engine
