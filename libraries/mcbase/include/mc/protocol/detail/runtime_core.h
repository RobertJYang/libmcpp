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

#ifndef MC_PROTOCOL_DETAIL_RUNTIME_CORE_H
#define MC_PROTOCOL_DETAIL_RUNTIME_CORE_H

#include <mc/common.h>
#include <mc/protocol/common.h>
#include <mc/protocol/packet.h>
#include <mc/protocol/trace.h>

#include <cstddef>

namespace mc::protocol::detail {

struct stack_descriptor {
    std::size_t layer_count{0};
    command (*invoke)(void* request_object, std::size_t index, flow_direction direction){nullptr};
    void* (*unsafe_layer_ptr)(void* request_object, std::size_t index) noexcept {nullptr};
};

class request_core {
public:
    request_core() = default;

    MC_API void bind(const stack_descriptor& descriptor, void* request_object) noexcept;
    MC_API void set_error(mc::string_view name, mc::string_view message);

    mc::protocol::packet& packet() noexcept
    {
        return m_packet;
    }

    const mc::protocol::packet& packet() const noexcept
    {
        return m_packet;
    }

    MC_API void* unsafe_layer_ptr(std::size_t index) noexcept;

    execution_state state() const noexcept
    {
        return m_execution_state;
    }

    bool is_suspended() const noexcept
    {
        return m_execution_state == execution_state::suspended;
    }

    bool is_completed() const noexcept
    {
        return m_execution_state == execution_state::completed;
    }

    bool has_failed() const noexcept
    {
        return m_execution_state == execution_state::failed;
    }

    const protocol_error& error() const noexcept
    {
        return m_error;
    }

    void set_trace(const trace_sink& sink) noexcept
    {
        m_trace_sink = sink;
    }

    void clear_trace() noexcept
    {
        m_trace_sink = {};
    }

    const trace_sink& trace() const noexcept
    {
        return m_trace_sink;
    }

private:
    friend MC_API execution_state drive_request(request_core& request, std::size_t index, flow_direction direction);
    friend MC_API execution_state resume_request(request_core& request);

    void reset_runtime(flow_direction direction, std::size_t index);

    bool is_bound() const noexcept
    {
        return m_descriptor != nullptr && m_request_object != nullptr && m_descriptor->invoke != nullptr;
    }

    const stack_descriptor* m_descriptor{nullptr};
    void*                   m_request_object{nullptr};
    mc::protocol::packet    m_packet;
    protocol_error          m_error;
    execution_state         m_execution_state{execution_state::idle};
    flow_direction          m_resume_direction{flow_direction::push};
    std::size_t             m_resume_index{0};
    trace_sink              m_trace_sink{};
};

[[nodiscard]] MC_API execution_state drive_request(request_core& request, std::size_t index, flow_direction direction);
[[nodiscard]] MC_API execution_state resume_request(request_core& request);

class context_core {
public:
    explicit context_core(request_core& request) noexcept : m_request(&request)
    {}

    mc::protocol::packet& packet() noexcept
    {
        return m_request->packet();
    }

    const mc::protocol::packet& packet() const noexcept
    {
        return m_request->packet();
    }

    void* unsafe_layer_ptr(std::size_t index) noexcept
    {
        return m_request->unsafe_layer_ptr(index);
    }

    command push_next() const noexcept
    {
        return {step_kind::push_next, 0, flow_direction::push};
    }

    command pop_next() const noexcept
    {
        return {step_kind::pop_next, 0, flow_direction::pop};
    }

    command suspend() const noexcept
    {
        return {step_kind::suspend, 0, flow_direction::push};
    }

    command complete() const noexcept
    {
        return {step_kind::complete, 0, flow_direction::push};
    }

    command jump_to(std::size_t index, flow_direction direction) const noexcept
    {
        return {step_kind::jump, index, direction};
    }

    command fail(mc::string_view name, mc::string_view message)
    {
        m_request->set_error(name, message);
        return {step_kind::fail, 0, flow_direction::push};
    }

private:
    request_core* m_request{nullptr};
};

} // namespace mc::protocol::detail

#endif // MC_PROTOCOL_DETAIL_RUNTIME_CORE_H
