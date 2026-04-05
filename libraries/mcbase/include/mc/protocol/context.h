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

namespace mc::protocol {

template <typename Spec, typename CurrentLayer>
class context;

template <typename CurrentLayer, typename... Layers>
class context<stack_spec<Layers...>, CurrentLayer> : public detail::context_core {
public:
    using spec_type    = stack_spec<Layers...>;
    using request_type = mc::protocol::request<spec_type>;

    explicit context(request_type& request) : detail::context_core(request.core()), m_request(request)
    {}

    detail::layer_state_t<CurrentLayer>& self()
    {
        return m_request.template get<CurrentLayer>();
    }

    const detail::layer_state_t<CurrentLayer>& self() const
    {
        return m_request.template get<CurrentLayer>();
    }

    template <typename Layer>
    detail::layer_state_t<Layer>& get()
    {
        return m_request.template get<Layer>();
    }

    template <typename Layer>
    const detail::layer_state_t<Layer>& get() const
    {
        return m_request.template get<Layer>();
    }

    const context& prepare_packet() const noexcept
    {
        m_request.prepare_packet();
        return *this;
    }

    const context& prepare_inbound() const noexcept
    {
        m_request.prepare_inbound();
        return *this;
    }

    const context& append_payload(const void* data, std::size_t len) const
    {
        m_request.append_payload(data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    const context& append_payload(const T (&arr)[N]) const
    {
        m_request.append_payload(arr);
        return *this;
    }

    command jump_to(std::size_t index, flow_direction direction = flow_direction::push) const noexcept
    {
        return detail::context_core::jump_to(index, direction);
    }

    template <typename Layer>
    command jump_to(flow_direction direction = flow_direction::push) const noexcept
    {
        return jump_to(detail::layer_index<Layer, Layers...>::value, direction);
    }

private:
    request_type& m_request;
};

} // namespace mc::protocol

#endif // MC_PROTOCOL_CONTEXT_H
