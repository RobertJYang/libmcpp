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

bool connection_state::connected() const noexcept
{
    return m_connected.load(std::memory_order_acquire);
}

void connection_state::disconnect() noexcept
{
    m_connected.store(false, std::memory_order_release);
}

signal_emit_guard::signal_emit_guard(const void* signal_addr, const char* signal_name,
                                     std::size_t max_depth)
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
    : m_name(name), m_max_depth(max_depth), m_slots(std::make_shared<slot_record_list>())
{}

signal_backend::~signal_backend() = default;

const char* signal_backend::name() const noexcept
{
    return m_name;
}

connection signal_backend::connect_erased(std::shared_ptr<void> callable, int priority)
{
    auto state = std::make_shared<connection_state>();

    std::lock_guard<std::mutex> lock(m_mutex);
    ensure_unique_slots_locked();
    compact_locked();
    m_slots->push_back(slot_record{priority, m_next_id++, std::move(callable), state});
    std::stable_sort(m_slots->begin(), m_slots->end(), [](const slot_record& lhs, const slot_record& rhs) {
        return lhs.priority < rhs.priority;
    });
    return connection(std::move(state));
}

void signal_backend::disconnect_all()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : *m_slots) {
        entry.state->disconnect();
    }
    m_slots = std::make_shared<slot_record_list>();
}

bool signal_backend::empty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return active_slot_count_locked() == 0;
}

std::size_t signal_backend::slot_count() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return active_slot_count_locked();
}

std::size_t signal_backend::max_depth() const noexcept
{
    return m_max_depth;
}

std::shared_ptr<const slot_record_list> signal_backend::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_slots;
}

void signal_backend::ensure_unique_slots_locked()
{
    if (!m_slots) {
        m_slots = std::make_shared<slot_record_list>();
        return;
    }

    if (m_slots.use_count() > 1) {
        m_slots = std::make_shared<slot_record_list>(*m_slots);
    }
}

void signal_backend::compact_locked()
{
    if (!m_slots) {
        return;
    }

    m_slots->erase(std::remove_if(m_slots->begin(), m_slots->end(),
                                  [](const slot_record& entry) {
        return !entry.state->connected();
    }),
                   m_slots->end());
}

std::size_t signal_backend::active_slot_count_locked() const
{
    if (!m_slots) {
        return 0;
    }

    return static_cast<std::size_t>(std::count_if(m_slots->begin(), m_slots->end(), [](const slot_record& entry) {
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
