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

#ifndef MC_PROTOCOL_CONTEXT_H
#define MC_PROTOCOL_CONTEXT_H

#include <mc/protocol/request.h>

namespace mc::proto::detail {

template <typename Spec, typename CurrentProtocol>
class runtime_context;

template <typename CurrentProtocol, typename... Protocols>
class runtime_context<stack_spec<Protocols...>, CurrentProtocol> : public context_core {
public:
    using spec_type    = stack_spec<Protocols...>;
    using request_type = mc::proto::request<spec_type>;

    explicit runtime_context(request_type& request) : context_core(request.core()), m_request(request)
    {}

    detail::protocol_request_t<CurrentProtocol>& self()
    {
        return m_request.template packet<CurrentProtocol>();
    }

    const detail::protocol_request_t<CurrentProtocol>& self() const
    {
        return m_request.template packet<CurrentProtocol>();
    }

    detail::protocol_context_t<CurrentProtocol>& self_context()
    {
        return m_request.template context<CurrentProtocol>();
    }

    const detail::protocol_context_t<CurrentProtocol>& self_context() const
    {
        return m_request.template context<CurrentProtocol>();
    }

    template <typename Protocol>
    detail::protocol_request_t<Protocol>& get()
    {
        return m_request.template packet<Protocol>();
    }

    template <typename Protocol>
    const detail::protocol_request_t<Protocol>& get() const
    {
        return m_request.template packet<Protocol>();
    }

    template <typename Protocol>
    detail::protocol_context_t<Protocol>& context()
    {
        return m_request.template context<Protocol>();
    }

    template <typename Protocol>
    const detail::protocol_context_t<Protocol>& context() const
    {
        return m_request.template context<Protocol>();
    }

    const runtime_context& prepare_buffer() const noexcept
    {
        m_request.prepare_buffer();
        return *this;
    }

    const runtime_context& prepare_inbound_buffer() const noexcept
    {
        m_request.prepare_inbound_buffer();
        return *this;
    }

    const runtime_context& append_payload(const void* data, std::size_t len) const
    {
        m_request.append_payload(data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    const runtime_context& append_payload(const T (&arr)[N]) const
    {
        m_request.append_payload(arr);
        return *this;
    }

    const runtime_context& prepare_packet() const noexcept
    {
        return prepare_buffer();
    }

    const runtime_context& prepare_inbound() const noexcept
    {
        return prepare_inbound_buffer();
    }

private:
    request_type& m_request;
};

} // namespace mc::proto::detail

#endif // MC_PROTOCOL_CONTEXT_H
