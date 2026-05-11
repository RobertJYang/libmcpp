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

#include <mc/protocol/request.h>

namespace mc::proto {

proto_request::proto_request() = default;

proto_request::~proto_request() = default;

mc::proto::buffer& proto_request::buffer() noexcept
{
    return m_buffer;
}

const mc::proto::buffer& proto_request::buffer() const noexcept
{
    return m_buffer;
}

execution_state proto_request::state() const noexcept
{
    return m_state;
}

flow_direction proto_request::direction() const noexcept
{
    return m_direction;
}

const protocol_error& proto_request::error() const noexcept
{
    return m_error;
}

void proto_request::clear_error() noexcept
{
    m_error.clear();
}

void proto_request::set_error(mc::string_view name, mc::string_view message)
{
    m_error.name    = name;
    m_error.message = message;
}

protocol_list_view proto_request::route_trace() const noexcept
{
    return {m_route_trace.data(), m_route_trace.size()};
}

const protocol* proto_request::resume_target() const noexcept
{
    return m_resume_protocol;
}

void proto_request::begin(protocol& entry, flow_direction direction)
{
    m_state            = execution_state::running;
    m_direction        = direction;
    m_current          = &entry;
    m_resume_protocol  = nullptr;
    m_resume_direction = direction;
    m_error.clear();
    m_route_trace.clear();
    m_route_trace.push_back(&entry);
    m_route_index = 0;
}

proto_context& proto_request::_append_context(std::unique_ptr<proto_context> ctx, protocol* owner)
{
    auto* raw      = ctx.get();
    raw->m_prev    = m_context_tail;
    raw->m_owner   = owner;
    m_context_tail = raw;
    m_contexts.push_back(std::move(ctx));
    return *raw;
}

void proto_request::set_state(execution_state state) noexcept
{
    m_state = state;
}

void proto_request::set_resume(protocol* proto, flow_direction direction) noexcept
{
    m_resume_protocol  = proto;
    m_resume_direction = direction;
}

protocol* proto_request::resume_protocol() const noexcept
{
    return m_resume_protocol;
}

flow_direction proto_request::resume_direction() const noexcept
{
    return m_resume_direction;
}

protocol* proto_request::current_protocol() const noexcept
{
    return m_current;
}

protocol* proto_request::next_traced_child() const noexcept
{
    if (m_route_index + 1 >= m_route_trace.size()) {
        return nullptr;
    }
    return m_route_trace[m_route_index + 1];
}

protocol* proto_request::prev_traced_parent() const noexcept
{
    if (m_route_index == 0 || m_route_trace.empty()) {
        return nullptr;
    }
    return m_route_trace[m_route_index - 1];
}

bool proto_request::enter_route(protocol& target) noexcept
{
    if (m_route_trace.empty()) {
        return false;
    }

    auto index = std::min(m_route_index, m_route_trace.size() - 1U);
    while (true) {
        if (m_route_trace[index] == &target) {
            m_route_index = index;
            m_current     = &target;
            return true;
        }
        if (index == 0) {
            break;
        }
        --index;
    }
    return false;
}

void proto_request::enter_push(protocol& target)
{
    m_direction = flow_direction::push;
    if (m_route_trace.empty()) {
        m_route_trace.push_back(m_current);
        m_route_index = 0;
    }

    if (m_route_index + 1 < m_route_trace.size() && m_route_trace[m_route_index + 1] == &target) {
        ++m_route_index;
    } else {
        m_route_trace.resize(m_route_index + 1);
        m_route_trace.push_back(&target);
        m_route_index = m_route_trace.size() - 1;
    }
    m_current = &target;
}

void proto_request::enter_pop(protocol& target)
{
    m_direction = flow_direction::pop;
    if (m_route_trace.empty()) {
        m_route_trace.push_back(m_current);
        m_route_index = 0;
    }

    if (m_route_index > 0 && m_route_trace[m_route_index - 1] == &target) {
        --m_route_index;
    } else if (m_route_index == 0) {
        m_route_trace.insert(m_route_trace.begin(), &target);
        m_route_index = 0;
    } else {
        m_route_trace.insert(m_route_trace.begin() + static_cast<std::ptrdiff_t>(m_route_index), &target);
    }
    m_current = &target;
}

void proto_request::enter_pull(protocol& target)
{
    m_direction = flow_direction::pop;
    if (m_route_trace.empty()) {
        m_route_trace.push_back(m_current);
        m_route_index = 0;
    }

    if (m_route_index + 1 < m_route_trace.size() && m_route_trace[m_route_index + 1] == &target) {
        ++m_route_index;
    } else {
        m_route_trace.resize(m_route_index + 1);
        m_route_trace.push_back(&target);
        m_route_index = m_route_trace.size() - 1;
    }
    m_current = &target;
}

proto_context* proto_request::_find_context_if(const protocol* owner, context_matcher matcher) noexcept
{
    for (auto* ctx = m_context_tail; ctx != nullptr; ctx = ctx->prev()) {
        if (owner != nullptr && ctx->owner() != owner) {
            continue;
        }
        if (matcher(*ctx)) {
            return ctx;
        }
    }
    return nullptr;
}

const proto_context* proto_request::_find_context_if(const protocol* owner, context_matcher matcher) const noexcept
{
    for (auto* ctx = m_context_tail; ctx != nullptr; ctx = ctx->prev()) {
        if (owner != nullptr && ctx->owner() != owner) {
            continue;
        }
        if (matcher(*ctx)) {
            return ctx;
        }
    }
    return nullptr;
}

} // namespace mc::proto
