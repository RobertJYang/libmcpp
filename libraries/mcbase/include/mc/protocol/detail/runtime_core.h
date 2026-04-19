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
#include <mc/future.h>
#include <mc/protocol/common.h>
#include <mc/protocol/packet.h>
#include <mc/protocol/trace.h>
#include <mc/time.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>

namespace mc::proto::detail {

struct stack_descriptor {
    std::size_t layer_count{0};
    command (*invoke)(void* request_object, void* runtime_object, std::size_t index, flow_direction direction){nullptr};
    void* (*unsafe_layer_ptr)(void* request_object, std::size_t index) noexcept {nullptr};
    void* (*unsafe_protocol_ptr)(void* runtime_object, std::size_t index) noexcept {nullptr};
};

class request_core {
public:
    request_core() = default;

    MC_API mc::future<void> enable_async_completion();
    MC_API bool             has_async_completion() const noexcept;
    MC_API void             resolve_async_completion() noexcept;
    MC_API void             fail_async_completion(std::exception_ptr error) noexcept;
    MC_API void             set_deadline(mc::time_point deadline);
    MC_API void             set_timeout(mc::milliseconds timeout);
    MC_API bool             has_deadline() const noexcept;
    MC_API std::optional<mc::time_point> deadline() const;
    MC_API void                          cancel() noexcept;
    MC_API bool                          is_cancelled() const noexcept;
    MC_API void                          add_cancel_callback(mc::futures::callback_type callback);

    MC_API void bind(const stack_descriptor& descriptor, void* request_object, void* runtime_object) noexcept;
    MC_API void set_error(mc::string_view name, mc::string_view message);

    mc::proto::packet& packet() noexcept
    {
        return m_packet;
    }

    const mc::proto::packet& packet() const noexcept
    {
        return m_packet;
    }

    MC_API void* unsafe_layer_ptr(std::size_t index) noexcept;
    MC_API void* unsafe_protocol_ptr(std::size_t index) noexcept;

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
    struct control_state {
        mutable std::mutex            mutex;
        std::optional<mc::time_point> deadline;
        mc::futures::callback_type    cancel_callback;
        mc::promise<void>             async_promise;
        std::atomic<bool>             cancelled{false};
    };

    friend MC_API execution_state drive_request(request_core& request, std::size_t index, flow_direction direction);
    friend MC_API execution_state resume_request(request_core& request);
    friend MC_API bool            continue_request(request_core& request, std::size_t source_index);
    friend MC_API bool            fail_request(request_core& request, std::size_t source_index);

    std::shared_ptr<control_state> ensure_control_state();
    static void                    notify_cancel_callbacks(const std::shared_ptr<control_state>& control) noexcept;
    static void                    mark_cancelled(const std::shared_ptr<control_state>& control) noexcept;
    void                           reset_runtime(flow_direction direction, std::size_t index);
    bool can_rebind(const stack_descriptor& descriptor, void* request_object, void* runtime_object) const noexcept;

    bool is_bound() const noexcept
    {
        return m_descriptor != nullptr && m_request_object != nullptr && m_runtime_object != nullptr &&
               m_descriptor->invoke != nullptr;
    }

    const stack_descriptor*        m_descriptor{nullptr};
    void*                          m_request_object{nullptr};
    void*                          m_runtime_object{nullptr};
    mc::proto::packet              m_packet;
    protocol_error                 m_error;
    execution_state                m_execution_state{execution_state::idle};
    flow_direction                 m_resume_direction{flow_direction::push};
    std::size_t                    m_resume_index{0};
    trace_sink                     m_trace_sink{};
    std::shared_ptr<control_state> m_control_state;
};

[[nodiscard]] MC_API execution_state drive_request(request_core& request, std::size_t index, flow_direction direction);
[[nodiscard]] MC_API execution_state resume_request(request_core& request);
[[nodiscard]] MC_API bool            continue_request(request_core& request, std::size_t source_index);
[[nodiscard]] MC_API bool            fail_request(request_core& request, std::size_t source_index);

class context_core {
public:
    explicit context_core(request_core& request) noexcept : m_request(&request)
    {}

    mc::proto::packet& packet() noexcept
    {
        return m_request->packet();
    }

    const mc::proto::packet& packet() const noexcept
    {
        return m_request->packet();
    }

    void* unsafe_layer_ptr(std::size_t index) noexcept
    {
        return m_request->unsafe_layer_ptr(index);
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
        m_request->set_error(name, message);
        return {step_kind::fail};
    }

private:
    request_core* m_request{nullptr};
};

class request_view_base {
public:
    using prepare_fn = void (*)(request_core&) noexcept;

    request_view_base(request_core& request, prepare_fn prepare_push, prepare_fn prepare_pop) noexcept
        : m_request(&request), m_prepare_push(prepare_push), m_prepare_pop(prepare_pop)
    {}

    mc::proto::buffer& buffer() noexcept
    {
        return m_request->packet();
    }

    const mc::proto::buffer& buffer() const noexcept
    {
        return m_request->packet();
    }

    request_view_base& prepare_buffer() noexcept
    {
        if (m_prepare_push != nullptr) {
            m_prepare_push(*m_request);
        }
        return *this;
    }

    request_view_base& prepare_inbound_buffer() noexcept
    {
        if (m_prepare_pop != nullptr) {
            m_prepare_pop(*m_request);
        }
        return *this;
    }

    request_view_base& prepare_packet() noexcept
    {
        return prepare_buffer();
    }

    request_view_base& prepare_inbound() noexcept
    {
        return prepare_inbound_buffer();
    }

    request_view_base& append_payload(const void* data, std::size_t len)
    {
        buffer().append_payload(data, len);
        return *this;
    }

    template <typename T, std::size_t N>
    request_view_base& append_payload(const T (&arr)[N])
    {
        buffer().append_payload(arr);
        return *this;
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
        m_request->set_error(name, message);
        return {step_kind::fail};
    }

    request_core& unsafe_core() noexcept
    {
        return *m_request;
    }

    const request_core& unsafe_core() const noexcept
    {
        return *m_request;
    }

private:
    request_core* m_request{nullptr};
    prepare_fn    m_prepare_push{nullptr};
    prepare_fn    m_prepare_pop{nullptr};
};

} // namespace mc::proto::detail

#endif // MC_PROTOCOL_DETAIL_RUNTIME_CORE_H
