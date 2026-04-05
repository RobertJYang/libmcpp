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

#include <mc/protocol/detail/runtime_core.h>

#include <vector>

namespace mc::protocol::detail {

namespace {

void emit_trace_if_needed(request_core& request, std::size_t layer_index, flow_direction direction)
{
    const trace_sink& sink = request.trace();
    if (sink.emit == nullptr) {
        return;
    }

    if (sink.layer_mask != 0) {
        if (layer_index >= 64) {
            return;
        }
        if ((sink.layer_mask & (std::uint64_t{1} << layer_index)) == 0) {
            return;
        }
    }

    const mc::protocol::packet& pkt = request.packet();
    trace_event                 ev{};
    ev.layer_index  = layer_index;
    ev.direction    = direction;
    ev.bytes        = pkt.data();
    ev.length       = pkt.length();
    ev.flags        = pkt.flags;
    ev.payload_base = pkt.payload_base();

    if (sink.filter != nullptr && !sink.filter(sink.user_data, ev)) {
        return;
    }

    std::vector<std::uint8_t> snapshot;
    if (ev.length > 0) {
        snapshot.assign(ev.bytes, ev.bytes + ev.length);
    }
    ev.bytes  = snapshot.data();
    ev.length = snapshot.size();

    sink.emit(sink.user_data, ev);
}

} // namespace

void request_core::bind(const stack_descriptor& descriptor, void* request_object) noexcept
{
    m_descriptor     = &descriptor;
    m_request_object = request_object;
}

void* request_core::unsafe_layer_ptr(std::size_t index) noexcept
{
    if (!is_bound() || m_descriptor->unsafe_layer_ptr == nullptr) {
        return nullptr;
    }
    return m_descriptor->unsafe_layer_ptr(m_request_object, index);
}

void request_core::reset_runtime(flow_direction direction, std::size_t index)
{
    m_error.clear();
    m_execution_state  = execution_state::idle;
    m_resume_direction = direction;
    m_resume_index     = index;
}

void request_core::set_error(mc::string_view name, mc::string_view message)
{
    m_error.name    = name;
    m_error.message = message;
}

execution_state drive_request(request_core& request, std::size_t index, flow_direction direction)
{
    if (!request.is_bound()) {
        request.set_error("unbound_stack", "protocol stack runtime is not bound");
        request.m_execution_state = execution_state::failed;
        return request.m_execution_state;
    }

    request.reset_runtime(direction, index);
    request.m_execution_state = execution_state::running;

    while (true) {
        const auto command = request.m_descriptor->invoke(request.m_request_object, index, direction);
        emit_trace_if_needed(request, index, direction);

        switch (command.kind) {
            case step_kind::push_next:
                if (index + 1 >= request.m_descriptor->layer_count) {
                    request.m_execution_state = execution_state::completed;
                    return request.m_execution_state;
                }
                index     = index + 1;
                direction = flow_direction::push;
                break;
            case step_kind::pop_next:
                if (index == 0) {
                    request.m_execution_state = execution_state::completed;
                    return request.m_execution_state;
                }
                index     = index - 1;
                direction = flow_direction::pop;
                break;
            case step_kind::jump:
                if (command.target_index >= request.m_descriptor->layer_count) {
                    request.set_error("invalid_jump", "target layer index is out of range");
                    request.m_execution_state = execution_state::failed;
                    return request.m_execution_state;
                }
                index     = command.target_index;
                direction = command.target_direction;
                break;
            case step_kind::suspend:
                request.m_resume_index     = index;
                request.m_resume_direction = direction;
                request.m_execution_state  = execution_state::suspended;
                return request.m_execution_state;
            case step_kind::complete:
                request.m_execution_state = execution_state::completed;
                return request.m_execution_state;
            case step_kind::fail:
                request.m_execution_state = execution_state::failed;
                return request.m_execution_state;
        }
    }
}

execution_state resume_request(request_core& request)
{
    if (!request.is_bound()) {
        request.set_error("unbound_stack", "protocol stack runtime is not bound");
        request.m_execution_state = execution_state::failed;
        return request.m_execution_state;
    }

    if (request.m_execution_state != execution_state::suspended) {
        return request.m_execution_state;
    }

    return drive_request(request, request.m_resume_index, request.m_resume_direction);
}

} // namespace mc::protocol::detail
