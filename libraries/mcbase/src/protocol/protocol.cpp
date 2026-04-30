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

#include <mc/protocol/protocol.h>

namespace mc::proto {

protocol::~protocol() = default;

void protocol::add_child(protocol& child)
{
    if (!_contains(m_children, child)) {
        m_children.push_back(&child);
    }
    if (!_contains(child.m_parents, *this)) {
        child.m_parents.push_back(this);
    }
}

protocol_list_view protocol::children() const noexcept
{
    return {m_children.data(), m_children.size()};
}

execution_state protocol::push(proto_request& req)
{
    req.begin(*this, flow_direction::push);
    return on_push(req);
}

execution_state protocol::pop(proto_request& req)
{
    req.begin(*this, flow_direction::pop);
    return on_pop(req);
}

execution_state protocol::resume(proto_request& req)
{
    auto*      resume_protocol  = req.resume_protocol();
    const auto resume_direction = req.resume_direction();
    if (req.state() != execution_state::suspended || resume_protocol == nullptr) {
        return fail(req, "invalid_resume", "请求未处于 suspend 状态");
    }

    req.set_state(execution_state::running);
    if (!req.enter_route(*resume_protocol)) {
        return fail(req, "invalid_resume", "请求 resume 路由已失效");
    }
    if (resume_direction == flow_direction::push) {
        return resume_protocol->on_push(req);
    }
    return resume_protocol->on_pop(req);
}

execution_state protocol::push_next(proto_request& req)
{
    if (!req.enter_route(*this)) {
        return fail(req, "invalid_route", "当前协议不在请求路由中");
    }
    if (auto* traced = req.next_traced_child(); traced != nullptr) {
        if (_contains(m_children, *traced)) {
            return push_to(req, *traced);
        }
    }
    if (m_children.empty()) {
        return complete(req);
    }
    if (m_children.size() == 1) {
        return push_to(req, *m_children.front());
    }
    return fail(req, "ambiguous_route", "push_next 无法在多分支下自动选择下一个协议");
}

execution_state protocol::push_to(proto_request& req, protocol& child)
{
    if (!_contains(m_children, child)) {
        return fail(req, "invalid_route", "目标协议不是当前协议的子节点");
    }
    req.enter_push(child);
    return child.on_push(req);
}

execution_state protocol::pop_next(proto_request& req)
{
    if (!req.enter_route(*this)) {
        return fail(req, "invalid_route", "当前协议不在请求路由中");
    }
    if (auto* traced = req.prev_traced_parent(); traced != nullptr) {
        if (_contains(m_parents, *traced)) {
            return pop_to(req, *traced);
        }
    }
    if (m_parents.empty()) {
        return complete(req);
    }
    if (m_parents.size() == 1) {
        return pop_to(req, *m_parents.front());
    }
    return fail(req, "ambiguous_route", "pop_next 无法在多入口下自动选择上一个协议");
}

execution_state protocol::pop_to(proto_request& req, protocol& parent)
{
    if (!_contains(m_parents, parent)) {
        return fail(req, "invalid_route", "目标协议不是当前协议的父节点");
    }
    req.enter_pop(parent);
    return parent.on_pop(req);
}

execution_state protocol::pull_next(proto_request& req)
{
    if (!req.enter_route(*this)) {
        return fail(req, "invalid_route", "当前协议不在请求路由中");
    }
    if (auto* traced = req.next_traced_child(); traced != nullptr) {
        if (_contains(m_children, *traced)) {
            return pull_from(req, *traced);
        }
    }
    if (m_children.empty()) {
        return fail(req, "invalid_route", "pull_next 缺少可拉取的子节点");
    }
    if (m_children.size() == 1) {
        return pull_from(req, *m_children.front());
    }
    return fail(req, "ambiguous_route", "pull_next 无法在多分支下自动选择子节点");
}

execution_state protocol::pull_from(proto_request& req, protocol& child)
{
    if (!_contains(m_children, child)) {
        return fail(req, "invalid_route", "目标协议不是当前协议的子节点");
    }
    req.enter_pull(child);
    return child.on_pop(req);
}

execution_state protocol::complete(proto_request& req)
{
    req.set_state(execution_state::completed);
    req.set_resume(nullptr, req.direction());
    return execution_state::completed;
}

void protocol::mark_running(proto_request& req)
{
    req.set_state(execution_state::running);
}

execution_state protocol::suspend(proto_request& req)
{
    req.set_state(execution_state::suspended);
    req.set_resume(this, req.direction());
    return execution_state::suspended;
}

execution_state protocol::fail(proto_request& req, mc::string_view name, mc::string_view message)
{
    req.set_error(name, message);
    req.set_state(execution_state::failed);
    req.set_resume(nullptr, req.direction());
    return execution_state::failed;
}

bool protocol::_contains(const std::vector<protocol*>& items, const protocol& target)
{
    return std::find(items.begin(), items.end(), &target) != items.end();
}

} // namespace mc::proto
