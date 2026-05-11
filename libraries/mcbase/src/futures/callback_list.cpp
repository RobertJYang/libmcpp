/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/futures/callback_list.h>

#include <algorithm>
#include <mc/futures/any_promise.h>
#include <mc/futures/state.h>

namespace mc::futures {

namespace detail {

bool handle_result_state_common(any_promise& promise, state_base& state);

void invoke_future_result_ready_holder(void* ptr)
{
    auto* holder = static_cast<future_result_ready_holder_base*>(ptr);
    try {
        if (holder->promise != nullptr && holder->state != nullptr &&
            detail::handle_result_state_common(*holder->promise, *holder->state)) {
            return;
        }

        if (holder->invoke_body != nullptr) {
            holder->invoke_body(ptr);
        }
    } catch (...) {
        if (holder->promise != nullptr) {
            holder->promise->set_current_exception();
        }
    }
}

void invoke_future_error_handler_holder(void* ptr)
{
    auto* holder = static_cast<future_result_ready_holder_base*>(ptr);
    try {
        if (holder->invoke_body != nullptr) {
            holder->invoke_body(ptr);
        }
    } catch (...) {
        if (holder->promise != nullptr) {
            holder->promise->set_current_exception();
        }
    }
}

void* clone_future_result_ready_holder(const void* ptr)
{
    auto* holder =
        const_cast<future_result_ready_holder_base*>(static_cast<const future_result_ready_holder_base*>(ptr));
    holder->ref_count.fetch_add(1, std::memory_order_relaxed);
    return holder;
}

void destroy_future_result_ready_holder(void* ptr) noexcept
{
    auto* holder = static_cast<future_result_ready_holder_base*>(ptr);
    if (holder == nullptr) {
        return;
    }

    if (holder->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (holder->destroy_self != nullptr) {
            holder->destroy_self(holder);
        }
    }
}

} // namespace detail

callback_type::~callback_type()
{
    reset();
}

void callback_type::operator()()
{
    if (m_invoke) {
        m_invoke(m_data);
    }
}

void callback_type::reset() noexcept
{
    if (m_destroy) {
        m_destroy(m_data);
    }
    m_data    = nullptr;
    m_invoke  = nullptr;
    m_clone   = nullptr;
    m_destroy = nullptr;
}

callback_pool& callback_pool::instance()
{
    return mc::singleton<callback_pool>::instance();
}

std::unique_ptr<callback_node> callback_pool::acquire_node(callback_type callback)
{
    std::unique_ptr<callback_node> node;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pool_head) {
            node        = std::move(m_pool_head);
            m_pool_head = std::move(node->m_next);
            m_pool_size--;
        }
    }

    if (node) {
        node->m_callback = std::move(callback);
        node->m_next     = nullptr;
    } else {
        node = std::make_unique<callback_node>(std::move(callback));
    }

    return node;
}

void callback_pool::release_node(std::unique_ptr<callback_node> node)
{
    if (!node) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查池大小限制
    if (m_pool_size >= m_max_pool_size) {
        return;
    }

    node->m_callback = nullptr;

    node->m_next = std::move(m_pool_head);
    m_pool_head  = std::move(node);
    m_pool_size++;
}

void callback_pool::set_max_pool_size(std::size_t max_size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_pool_size = max_size;

    // 如果当前池大小超过限制，释放多余的节点
    while (m_pool_size > m_max_pool_size && m_pool_head) {
        auto next   = std::move(m_pool_head->m_next);
        m_pool_head = std::move(next);
        m_pool_size--;
    }
}

callback_pool::pool_stats callback_pool::get_stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_pool_size, m_max_pool_size};
}

void callback_pool::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool_head = nullptr;
    m_pool_size = 0;
}

void callback_list::push_back(callback_type callback)
{
    auto node = callback_pool::instance().acquire_node(std::move(callback));
    if (!m_head) {
        m_tail = node.get();
        m_head = std::move(node);
    } else {
        m_tail->m_next = std::move(node);
        m_tail         = m_tail->m_next.get();
    }
}

void callback_list::execute_and_clear()
{
    auto current = std::move(m_head);
    m_tail       = nullptr;

    while (current) {
        auto next = std::move(current->m_next);
        safe_invoke(std::move(current->m_callback));
        current->m_callback = nullptr;
        callback_pool::instance().release_node(std::move(current));
        current = std::move(next);
    }
}

void callback_list::swap(callback_list& other) noexcept
{
    std::swap(m_head, other.m_head);
    std::swap(m_tail, other.m_tail);
}

void callback_list::clear()
{
    auto current = std::move(m_head);
    m_tail       = nullptr;

    auto& pool = callback_pool::instance();
    while (current) {
        current->m_callback = nullptr;
        auto next           = std::move(current->m_next);
        pool.release_node(std::move(current));
        current = std::move(next);
    }
}

bool callback_list::empty() const
{
    return m_head == nullptr;
}

std::size_t callback_list::size() const
{
    std::size_t count  = 0;
    auto*       cursor = m_head.get();
    while (cursor != nullptr) {
        ++count;
        cursor = cursor->m_next.get();
    }
    return count;
}

} // namespace mc::futures
