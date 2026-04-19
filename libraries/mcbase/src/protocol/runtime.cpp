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

#include <stdexcept>
#include <vector>

namespace mc::proto::detail {

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

    const mc::proto::buffer& buf = request.packet();
    trace_event              ev{};
    ev.layer_index  = layer_index;
    ev.direction    = direction;
    ev.bytes        = buf.data();
    ev.length       = buf.length();
    ev.flags        = buf.flags;
    ev.payload_base = buf.payload_base();

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

void finalize_async_result(request_core& core, execution_state state)
{
    if (!core.has_async_completion()) {
        return;
    }
    if (state == execution_state::completed) {
        core.resolve_async_completion();
        return;
    }
    if (state == execution_state::failed) {
        const auto& error = core.error();
        auto        ex    = std::make_exception_ptr(
            std::runtime_error(error.message.empty() ? "protocol request failed" : error.message.c_str()));
        core.fail_async_completion(std::move(ex));
    }
}

std::shared_ptr<request_core::control_state> request_core::ensure_control_state()
{
    if (m_control_state == nullptr) {
        m_control_state = std::make_shared<control_state>();
    }
    return m_control_state;
}

void request_core::notify_cancel_callbacks(const std::shared_ptr<control_state>& control) noexcept
{
    if (control == nullptr) {
        return;
    }

    mc::futures::callback_type callback;
    {
        std::lock_guard<std::mutex> lock(control->mutex);
        callback = control->cancel_callback;
    }

    if (callback) {
        callback();
    }
}

void request_core::mark_cancelled(const std::shared_ptr<control_state>& control) noexcept
{
    if (control == nullptr) {
        return;
    }

    if (control->cancelled.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    notify_cancel_callbacks(control);
}

mc::future<void> request_core::enable_async_completion()
{
    auto control = ensure_control_state();
    auto weak    = std::weak_ptr<control_state>(control);
    auto promise = mc::make_promise<void>();
    auto future  = promise.get_future();

    future.on_cancel([weak]() {
        request_core::mark_cancelled(weak.lock());
    });

    {
        std::lock_guard<std::mutex> lock(control->mutex);
        control->async_promise = promise;
    }

    if (control->cancelled.load(std::memory_order_acquire)) {
        future.cancel();
    }

    return future;
}

bool request_core::has_async_completion() const noexcept
{
    if (m_control_state == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_control_state->mutex);
    return static_cast<bool>(m_control_state->async_promise);
}

void request_core::resolve_async_completion() noexcept
{
    if (m_control_state == nullptr) {
        return;
    }

    mc::promise<void> promise;
    {
        std::lock_guard<std::mutex> lock(m_control_state->mutex);
        promise = std::move(m_control_state->async_promise);
        m_control_state->async_promise.reset();
    }
    if (!promise) {
        return;
    }
    try {
        promise.set_value();
    } catch (...) {
    }
}

void request_core::fail_async_completion(std::exception_ptr error) noexcept
{
    if (m_control_state == nullptr) {
        return;
    }

    mc::promise<void> promise;
    {
        std::lock_guard<std::mutex> lock(m_control_state->mutex);
        promise = std::move(m_control_state->async_promise);
        m_control_state->async_promise.reset();
    }
    if (!promise) {
        return;
    }
    try {
        promise.set_exception(std::move(error));
    } catch (...) {
    }
}

void request_core::set_deadline(mc::time_point deadline)
{
    auto control = ensure_control_state();

    std::lock_guard<std::mutex> lock(control->mutex);
    control->deadline = deadline;
}

void request_core::set_timeout(mc::milliseconds timeout)
{
    set_deadline(mc::time_point::now() + timeout);
}

bool request_core::has_deadline() const noexcept
{
    if (m_control_state == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_control_state->mutex);
    return m_control_state->deadline.has_value();
}

std::optional<mc::time_point> request_core::deadline() const
{
    if (m_control_state == nullptr) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_control_state->mutex);
    return m_control_state->deadline;
}

void request_core::cancel() noexcept
{
    auto              control = ensure_control_state();
    mc::promise<void> promise;
    {
        std::lock_guard<std::mutex> lock(control->mutex);
        promise = control->async_promise;
    }

    request_core::mark_cancelled(control);

    if (promise && !promise.is_cancelled()) {
        promise.cancel();
    }
}

bool request_core::is_cancelled() const noexcept
{
    return m_control_state != nullptr && m_control_state->cancelled.load(std::memory_order_acquire);
}

void request_core::add_cancel_callback(mc::futures::callback_type callback)
{
    auto control    = ensure_control_state();
    bool invoke_now = false;

    {
        std::lock_guard<std::mutex> lock(control->mutex);
        if (control->cancelled.load(std::memory_order_acquire)) {
            invoke_now = true;
        } else if (!control->cancel_callback) {
            control->cancel_callback = std::move(callback);
        } else {
            auto current             = control->cancel_callback;
            control->cancel_callback = mc::futures::callback_type([current, callback]() mutable {
                if (current) {
                    current();
                }
                if (callback) {
                    callback();
                }
            });
        }
    }

    if (invoke_now && callback) {
        callback();
    }
}

void request_core::bind(const stack_descriptor& descriptor, void* request_object, void* runtime_object) noexcept
{
    if (!can_rebind(descriptor, request_object, runtime_object)) {
        set_error("resume_source_mismatch", "suspended proto request resumed by different runtime source");
        m_execution_state = execution_state::failed;
        return;
    }
    m_descriptor     = &descriptor;
    m_request_object = request_object;
    m_runtime_object = runtime_object;
}

void* request_core::unsafe_layer_ptr(std::size_t index) noexcept
{
    if (!is_bound() || m_descriptor->unsafe_layer_ptr == nullptr) {
        return nullptr;
    }
    return m_descriptor->unsafe_layer_ptr(m_request_object, index);
}

void* request_core::unsafe_protocol_ptr(std::size_t index) noexcept
{
    if (!is_bound() || m_descriptor->unsafe_protocol_ptr == nullptr) {
        return nullptr;
    }
    return m_descriptor->unsafe_protocol_ptr(m_runtime_object, index);
}

void request_core::reset_runtime(flow_direction direction, std::size_t index)
{
    m_error.clear();
    m_execution_state  = execution_state::idle;
    m_resume_direction = direction;
    m_resume_index     = index;
}

bool request_core::can_rebind(const stack_descriptor& descriptor, void* request_object,
                              void* runtime_object) const noexcept
{
    if (m_execution_state != execution_state::suspended || !is_bound()) {
        return true;
    }

    return m_descriptor == &descriptor && m_request_object == request_object && m_runtime_object == runtime_object;
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
        const auto command =
            request.m_descriptor->invoke(request.m_request_object, request.m_runtime_object, index, direction);
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

bool continue_request(request_core& request, std::size_t source_index)
{
    if (!request.is_bound() || request.m_descriptor == nullptr || source_index >= request.m_descriptor->layer_count) {
        return false;
    }

    auto* proto = static_cast<mc::proto::protocol*>(request.unsafe_protocol_ptr(source_index));
    return proto != nullptr ? proto->on_continue(request) : false;
}

bool fail_request(request_core& request, std::size_t source_index)
{
    if (!request.is_bound() || request.m_descriptor == nullptr || source_index >= request.m_descriptor->layer_count) {
        return false;
    }

    auto* proto = static_cast<mc::proto::protocol*>(request.unsafe_protocol_ptr(source_index));
    return proto != nullptr ? proto->on_failed(request) : false;
}

} // namespace mc::proto::detail
