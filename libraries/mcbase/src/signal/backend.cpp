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

#include <mc/signal/detail/backend.h>

#include <algorithm>
#include <string>

namespace mc::detail {
std::vector<signal_frame>& current_signal_stack() noexcept
{
    thread_local std::vector<signal_frame> stack;
    return stack;
}

namespace {
std::string signal_name_or_default(const char* signal_name)
{
    return signal_name != nullptr ? signal_name : "<unnamed>";
}

std::string format_signal_call_chain(const std::vector<signal_frame>& stack, const void* current_addr,
                                     const char* current_name)
{
    std::string chain;

    for (std::size_t index = 0; index < stack.size(); ++index) {
        if (!chain.empty()) {
            chain += " -> ";
        }
        chain += signal_name_or_default(stack[index].signal_name);
    }

    if (current_addr != nullptr) {
        if (!chain.empty()) {
            chain += " -> ";
        }
        chain += signal_name_or_default(current_name);
    }

    return chain;
}

} // namespace

struct signal_backend::state {
    mutable std::mutex                mutex;
    std::shared_ptr<slot_record_list> slots{std::make_shared<slot_record_list>()};
    std::uint64_t                     next_id{0};
};

bool connection_state::connected() const noexcept
{
    return m_connected.load(std::memory_order_acquire);
}

void connection_state::disconnect() noexcept
{
    bool expected = true;
    if (!m_connected.compare_exchange_strong(expected, false, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }

    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        callbacks.reserve(m_disconnect_callbacks.size());
        for (auto& [id, callback] : m_disconnect_callbacks) {
            MC_UNUSED(id);
            callbacks.push_back(std::move(callback));
        }
        m_disconnect_callbacks.clear();
    }

    for (auto& callback : callbacks) {
        try {
            if (callback) {
                callback();
            }
        } catch (...) {
        }
    }
}

connection_state::disconnect_callback_id connection_state::add_disconnect_callback(std::function<void()> callback)
{
    if (!callback) {
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        if (m_connected.load(std::memory_order_acquire)) {
            const auto callback_id = m_next_callback_id++;
            m_disconnect_callbacks.emplace_back(callback_id, std::move(callback));
            return callback_id;
        }
    }

    try {
        callback();
    } catch (...) {
    }
    return 0;
}

void connection_state::remove_disconnect_callback(disconnect_callback_id callback_id) noexcept
{
    if (callback_id == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_disconnect_callbacks.erase(std::remove_if(m_disconnect_callbacks.begin(), m_disconnect_callbacks.end(),
                                                [callback_id](const auto& entry) {
        return entry.first == callback_id;
    }),
                                 m_disconnect_callbacks.end());
}

signal_emit_guard::signal_emit_guard(const void* signal_addr, const char* signal_name, std::size_t max_depth)
{
    auto& stack = current_signal_stack();

    for (const auto& frame : stack) {
        if (frame.signal_addr == signal_addr) {
            auto chain = format_signal_call_chain(stack, signal_addr, signal_name);
            MC_THROW(signal_recursion_exception,
                     "信号循环调用: signal '${name}' 在 emit 过程中被递归触发，调用链: ${chain}",
                     ("name", signal_name_or_default(signal_name))("chain", chain));
        }
    }

    if (stack.size() >= max_depth) {
        auto chain = format_signal_call_chain(stack, signal_addr, signal_name);
        MC_THROW(signal_recursion_exception, "信号调用链过深: 当前深度 ${depth}，最大允许 ${max}，调用链: ${chain}",
                 ("depth", stack.size())("max", max_depth)("chain", chain));
    }

    stack.push_back(signal_frame{signal_addr, signal_name});
    m_depth = stack.size();
}

signal_emit_guard::~signal_emit_guard() noexcept
{
    current_signal_stack().pop_back();
}

std::size_t signal_emit_guard::depth() const noexcept
{
    return m_depth;
}

signal_backend::signal_backend(const char* name, std::size_t max_depth)
    : m_name(name), m_max_depth(max_depth), m_state(std::make_shared<state>())
{}

signal_backend::~signal_backend() = default;

const char* signal_backend::name() const noexcept
{
    return m_name;
}

connection signal_backend::connect_erased(std::shared_ptr<void> callable, int priority)
{
    auto          connection_state = std::make_shared<class connection_state>();
    auto          backend_state    = m_state;
    std::uint64_t slot_id          = 0;

    {
        std::lock_guard<std::mutex> lock(backend_state->mutex);
        ensure_unique_slots_locked(*backend_state);
        compact_locked(*backend_state);
        slot_id = backend_state->next_id++;
        backend_state->slots->push_back(slot_record{priority, slot_id, std::move(callable), connection_state});
        std::stable_sort(backend_state->slots->begin(), backend_state->slots->end(),
                         [](const slot_record& lhs, const slot_record& rhs) {
            return lhs.priority < rhs.priority;
        });
    }

    connection_state->add_disconnect_callback(
        [weak_backend_state = std::weak_ptr<signal_backend::state>(backend_state), slot_id]() noexcept {
        if (const auto locked_state = weak_backend_state.lock()) {
            std::lock_guard<std::mutex> lock(locked_state->mutex);
            if (!locked_state->slots) {
                return;
            }
            if (locked_state->slots.use_count() > 1) {
                locked_state->slots = std::make_shared<slot_record_list>(*locked_state->slots);
            }
            locked_state->slots->erase(std::remove_if(locked_state->slots->begin(), locked_state->slots->end(),
                                                      [slot_id](const slot_record& entry) {
                return entry.id == slot_id;
            }),
                                       locked_state->slots->end());
        }
    });

    return connection(std::move(connection_state));
}

void signal_backend::disconnect_all()
{
    std::vector<std::shared_ptr<connection_state>> states;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        states.reserve(m_state->slots->size());
        for (const auto& entry : *m_state->slots) {
            states.push_back(entry.state);
        }
        m_state->slots = std::make_shared<slot_record_list>();
    }

    for (auto& state : states) {
        state->disconnect();
    }
}

bool signal_backend::empty() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return active_slot_count_locked(*m_state) == 0;
}

std::size_t signal_backend::slot_count() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return active_slot_count_locked(*m_state);
}

std::size_t signal_backend::max_depth() const noexcept
{
    return m_max_depth;
}

std::shared_ptr<const slot_record_list> signal_backend::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->slots;
}

void signal_backend::ensure_unique_slots_locked(state& backend_state)
{
    if (!backend_state.slots) {
        backend_state.slots = std::make_shared<slot_record_list>();
        return;
    }

    if (backend_state.slots.use_count() > 1) {
        backend_state.slots = std::make_shared<slot_record_list>(*backend_state.slots);
    }
}

void signal_backend::compact_locked(state& backend_state)
{
    if (!backend_state.slots) {
        return;
    }

    backend_state.slots->erase(std::remove_if(backend_state.slots->begin(), backend_state.slots->end(),
                                              [](const slot_record& entry) {
        return !entry.state->connected();
    }),
                               backend_state.slots->end());
}

std::size_t signal_backend::active_slot_count_locked(const state& backend_state) const
{
    if (!backend_state.slots) {
        return 0;
    }

    return static_cast<std::size_t>(
        std::count_if(backend_state.slots->begin(), backend_state.slots->end(), [](const slot_record& entry) {
        return entry.state->connected();
    }));
}

} // namespace mc::detail

namespace mc {

signal_call_stack_view current_signal_call_stack() noexcept
{
    auto& stack = detail::current_signal_stack();
    return signal_call_stack_view(stack.data(), stack.size());
}

std::size_t current_signal_depth() noexcept
{
    return detail::current_signal_stack().size();
}

} // namespace mc
