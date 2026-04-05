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

#ifndef MC_PROTOCOL_REQUEST_H
#define MC_PROTOCOL_REQUEST_H

#include <mc/protocol/common.h>
#include <mc/protocol/detail/runtime_core.h>
#include <mc/protocol/packet.h>
#include <mc/protocol/stack_spec.h>

#include <tuple>
#include <type_traits>

namespace mc::protocol {

template <typename Spec>
class runtime;

template <typename Spec, typename CurrentLayer>
class context;

template <typename Spec>
class request;

template <typename... Layers>
class request<stack_spec<Layers...>> {
public:
    using spec_type    = stack_spec<Layers...>;
    using states_type  = std::tuple<detail::layer_state_t<Layers>...>;
    using layers_type  = std::tuple<Layers...>;
    using runtime_core = detail::request_core;

    static constexpr std::size_t layer_count = sizeof...(Layers);

    request() = default;

    request(const request&)            = delete;
    request& operator=(const request&) = delete;
    request(request&&)                 = delete;
    request& operator=(request&&)      = delete;

    mc::protocol::packet& packet() noexcept
    {
        return m_core.packet();
    }

    const mc::protocol::packet& packet() const noexcept
    {
        return m_core.packet();
    }

    request& prepare_packet() noexcept
    {
        detail::packet_access::arm(packet(), spec_type::push_headroom, spec_type::push_tailroom,
                                   spec_type::push_headroom);
        return *this;
    }

    request& prepare_inbound() noexcept
    {
        detail::packet_access::arm(packet(), spec_type::pop_headroom, spec_type::pop_tailroom, spec_type::pop_headroom);
        return *this;
    }

    request& append_payload(const void* data, std::size_t len)
    {
        packet().append_payload(data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    request& append_payload(const T (&arr)[N])
    {
        packet().append_payload(arr);
        return *this;
    }

    template <typename Layer>
    detail::layer_state_t<Layer>& get()
    {
        constexpr std::size_t index = detail::layer_index<Layer, Layers...>::value;
        return std::get<index>(m_states);
    }

    template <typename Layer>
    const detail::layer_state_t<Layer>& get() const
    {
        constexpr std::size_t index = detail::layer_index<Layer, Layers...>::value;
        return std::get<index>(m_states);
    }

    void* unsafe_layer_ptr(std::size_t index) noexcept
    {
        return detail::tuple_ptr_at(m_states, index);
    }

    const void* unsafe_layer_ptr(std::size_t index) const noexcept
    {
        return detail::tuple_ptr_at_const(m_states, index);
    }

    execution_state state() const noexcept
    {
        return m_core.state();
    }

    bool is_suspended() const noexcept
    {
        return m_core.is_suspended();
    }

    bool is_completed() const noexcept
    {
        return m_core.is_completed();
    }

    bool has_failed() const noexcept
    {
        return m_core.has_failed();
    }

    const protocol_error& error() const noexcept
    {
        return m_core.error();
    }

    void set_trace(const trace_sink& sink) noexcept
    {
        m_core.set_trace(sink);
    }

    void clear_trace() noexcept
    {
        m_core.clear_trace();
    }

    const trace_sink& trace() const noexcept
    {
        return m_core.trace();
    }

private:
    template <typename>
    friend class runtime;

    template <typename, typename>
    friend class context;

    runtime_core& core() noexcept
    {
        return m_core;
    }

    const runtime_core& core() const noexcept
    {
        return m_core;
    }

    void bind_runtime(const detail::stack_descriptor& descriptor, void* request_object) noexcept
    {
        m_core.bind(descriptor, request_object);
    }

    void set_error(mc::string_view name, mc::string_view message)
    {
        m_core.set_error(name, message);
    }

    runtime_core m_core;
    states_type  m_states;
};

} // namespace mc::protocol

#endif // MC_PROTOCOL_REQUEST_H
