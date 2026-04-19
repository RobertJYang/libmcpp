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

#include <optional>
#include <tuple>
#include <type_traits>

namespace mc::proto {

template <typename Spec>
class runtime;

namespace detail {
template <typename Spec, typename CurrentProtocol>
class runtime_context;

template <typename Spec>
void prepare_push_view_buffer(request_core& core) noexcept
{
    buffer_access::arm(core.packet(), Spec::push_headroom, Spec::push_tailroom, Spec::push_headroom);
}

template <typename Spec>
void prepare_pop_view_buffer(request_core& core) noexcept
{
    buffer_access::arm(core.packet(), Spec::pop_headroom, Spec::pop_tailroom, Spec::pop_headroom);
}
} // namespace detail

template <typename Spec>
class request;

template <typename... Protocols>
using request_for_t = request<stack_spec<Protocols...>>;

template <typename Packet, typename Context>
class request_view : public detail::request_view_base {
public:
    request_view(detail::request_view_base base, Packet& packet, Context& context) noexcept
        : detail::request_view_base(base), m_packet(&packet), m_context(&context)
    {}

    Packet& packet() noexcept
    {
        return *m_packet;
    }

    const Packet& packet() const noexcept
    {
        return *m_packet;
    }

    Context& context() noexcept
    {
        return *m_context;
    }

    const Context& context() const noexcept
    {
        return *m_context;
    }

private:
    Packet*  m_packet{nullptr};
    Context* m_context{nullptr};
};

template <typename... Protocols>
request_for_t<Protocols...> make_request()
{
    static_assert(sizeof...(Protocols) > 0, "make_request requires at least one protocol");
    return request_for_t<Protocols...>();
}

template <typename... Protocols>
class request<stack_spec<Protocols...>> {
public:
    using spec_type            = stack_spec<Protocols...>;
    using packets_type         = std::tuple<detail::protocol_packet_t<Protocols>...>;
    using contexts_type        = std::tuple<detail::protocol_context_t<Protocols>...>;
    using protocols_type       = std::tuple<Protocols...>;
    using owned_protocols_type = std::tuple<Protocols...>;
    using runtime_core         = detail::request_core;

    static constexpr std::size_t layer_count = sizeof...(Protocols);

    request() = default;

    request(const request&)            = delete;
    request& operator=(const request&) = delete;
    request(request&&)                 = delete;
    request& operator=(request&&)      = delete;

    mc::proto::buffer& buffer() noexcept
    {
        return m_core.packet();
    }

    const mc::proto::buffer& buffer() const noexcept
    {
        return m_core.packet();
    }

    request& prepare_buffer() noexcept
    {
        detail::prepare_push_view_buffer<spec_type>(m_core);
        return *this;
    }

    request& prepare_inbound_buffer() noexcept
    {
        detail::prepare_pop_view_buffer<spec_type>(m_core);
        return *this;
    }

    request& append_payload(const void* data, std::size_t len)
    {
        buffer().append_payload(data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    request& append_payload(const T (&arr)[N])
    {
        buffer().append_payload(arr);
        return *this;
    }

    request& prepare_packet() noexcept
    {
        return prepare_buffer();
    }

    request& prepare_inbound() noexcept
    {
        return prepare_inbound_buffer();
    }

    mc::proto::buffer& packet() noexcept
    {
        return buffer();
    }

    const mc::proto::buffer& packet() const noexcept
    {
        return buffer();
    }

    template <typename Protocol>
    detail::protocol_packet_t<Protocol>& packet()
    {
        constexpr std::size_t index = detail::protocol_index<Protocol, Protocols...>::value;
        return std::get<index>(m_packets);
    }

    template <typename Protocol>
    const detail::protocol_packet_t<Protocol>& packet() const
    {
        constexpr std::size_t index = detail::protocol_index<Protocol, Protocols...>::value;
        return std::get<index>(m_packets);
    }

    command push_next() const noexcept
    {
        return {step_kind::push_next};
    }

    command pop_next() const noexcept
    {
        return {step_kind::pop_next};
    }

    command suspend() const noexcept
    {
        return {step_kind::suspend};
    }

    command complete() const noexcept
    {
        return {step_kind::complete};
    }

    command fail(mc::string_view name, mc::string_view message)
    {
        m_core.set_error(name, message);
        return {step_kind::fail};
    }

    void* unsafe_packet_ptr(std::size_t index) noexcept
    {
        return detail::tuple_ptr_at(m_packets, index);
    }

    const void* unsafe_packet_ptr(std::size_t index) const noexcept
    {
        return detail::tuple_ptr_at_const(m_packets, index);
    }

    void* unsafe_layer_ptr(std::size_t index) noexcept
    {
        return unsafe_packet_ptr(index);
    }

    const void* unsafe_layer_ptr(std::size_t index) const noexcept
    {
        return unsafe_packet_ptr(index);
    }

    execution_state status() const noexcept
    {
        return m_core.state();
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

    template <typename Protocol>
    detail::protocol_context_t<Protocol>& context()
    {
        constexpr std::size_t index = detail::protocol_index<Protocol, Protocols...>::value;
        return std::get<index>(m_contexts);
    }

    template <typename Protocol>
    const detail::protocol_context_t<Protocol>& context() const
    {
        constexpr std::size_t index = detail::protocol_index<Protocol, Protocols...>::value;
        return std::get<index>(m_contexts);
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

    mc::future<void> enable_async_completion()
    {
        return m_core.enable_async_completion();
    }

    bool has_async_completion() const noexcept
    {
        return m_core.has_async_completion();
    }

    request& set_deadline(mc::time_point deadline)
    {
        m_core.set_deadline(deadline);
        return *this;
    }

    request& set_timeout(mc::milliseconds timeout)
    {
        m_core.set_timeout(timeout);
        return *this;
    }

    bool has_deadline() const noexcept
    {
        return m_core.has_deadline();
    }

    std::optional<mc::time_point> deadline() const
    {
        return m_core.deadline();
    }

    request& cancel() noexcept
    {
        m_core.cancel();
        return *this;
    }

    bool is_cancelled() const noexcept
    {
        return m_core.is_cancelled();
    }

    runtime_core& unsafe_core() noexcept
    {
        return m_core;
    }

    const runtime_core& unsafe_core() const noexcept
    {
        return m_core;
    }

private:
    template <typename>
    friend class runtime;

    template <typename, typename>
    friend class detail::runtime_context;

    runtime_core& core() noexcept
    {
        return m_core;
    }

    const runtime_core& core() const noexcept
    {
        return m_core;
    }

    void bind_runtime(const detail::stack_descriptor& descriptor, void* request_object, void* runtime_object) noexcept
    {
        m_core.bind(descriptor, request_object, runtime_object);
    }

    void set_error(mc::string_view name, mc::string_view message)
    {
        m_core.set_error(name, message);
    }

    owned_protocols_type& owned_protocols()
    {
        static_assert((std::is_default_constructible_v<Protocols> && ...),
                      "single-argument runtime path requires default-constructible protocols; "
                      "use proto::instance or runtime(..., protocols) otherwise");
        if (!m_owned_protocols.has_value()) {
            m_owned_protocols.emplace();
        }
        return *m_owned_protocols;
    }

    const owned_protocols_type& owned_protocols() const
    {
        static_assert((std::is_default_constructible_v<Protocols> && ...),
                      "single-argument runtime path requires default-constructible protocols; "
                      "use proto::instance or runtime(..., protocols) otherwise");
        return const_cast<request*>(this)->owned_protocols();
    }

    runtime_core                        m_core;
    packets_type                        m_packets;
    contexts_type                       m_contexts;
    std::optional<owned_protocols_type> m_owned_protocols;
};

} // namespace mc::proto

#endif // MC_PROTOCOL_REQUEST_H
