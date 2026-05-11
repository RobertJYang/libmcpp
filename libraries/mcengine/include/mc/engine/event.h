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

#ifndef MC_ENGINE_EVENT_H
#define MC_ENGINE_EVENT_H

#include <mc/event.h>
#include <mc/result.h>
#include <mc/string_view.h>
#include <mc/variant.h>

#include <optional>

namespace mc::engine {

class context;
class property_base;

constexpr mc::event_type_id property_changed_event_id = 0x00010001u;

class MC_API property_changed_event : public mc::event {
public:
    property_changed_event(const mc::variant& value, const property_base& property);
    ~property_changed_event() override;

    const mc::variant& value() const noexcept
    {
        return m_value;
    }

    const property_base& property() const noexcept
    {
        return m_property;
    }

private:
    mc::variant          m_value;
    const property_base& m_property;
};

constexpr mc::event_type_id method_invoke_event_id = 0x00010002u;

class MC_API method_invoke_event : public mc::event {
public:
    method_invoke_event(context& ctx, mc::string_view method_name, const mc::variants& args,
                        mc::string_view interface_name, bool is_async) noexcept;
    ~method_invoke_event() override;

    context& invoke_context() const noexcept
    {
        return m_context;
    }

    mc::string_view method_name() const noexcept
    {
        return m_method_name;
    }

    const mc::variants& args() const noexcept
    {
        return m_args;
    }

    mc::string_view interface_name() const noexcept
    {
        return m_interface_name;
    }

    bool is_async() const noexcept
    {
        return m_is_async;
    }

    void set_result(const mc::variant& value)
    {
        m_result = value;
        accept();
    }

    void set_result(mc::variant&& value)
    {
        m_result = std::move(value);
        accept();
    }

    bool has_result() const noexcept
    {
        return m_result.has_value();
    }

    const mc::variant& result() const noexcept
    {
        return *m_result;
    }

    void set_async_result(mc::result<mc::variant>&& r);

    bool has_async_result() const noexcept
    {
        return m_async_result.has_value();
    }

    mc::result<mc::variant> take_async_result();

private:
    context&                               m_context;
    mc::string_view                        m_method_name;
    const mc::variants&                    m_args;
    mc::string_view                        m_interface_name;
    bool                                   m_is_async{false};
    std::optional<mc::variant>             m_result;
    std::optional<mc::result<mc::variant>> m_async_result;
};

} // namespace mc::engine

#endif // MC_ENGINE_EVENT_H
