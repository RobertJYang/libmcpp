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

#include <boost/asio.hpp>

#include <mcpy/runtime/actor_base.h>

namespace mcpy {

actor_base::actor_base()
    : m_executor(mc::runtime::runtime_context::create_strand()) {
}

actor_base::actor_base(const actor_base&)
    : m_executor(mc::runtime::runtime_context::create_strand()) {
}

actor_base& actor_base::operator=(const actor_base&) {
    std::lock_guard lock(m_queue_mutex);
    m_exclusive_queue.clear();
    m_exclusive_running = false;
    m_executor          = mc::runtime::runtime_context::create_strand();
    return *this;
}

mc::runtime::any_executor actor_base::get_executor() const {
    return m_executor;
}

bool actor_base::submit_exclusive_task(mcpy::runtime::operation_base* task) {
    std::lock_guard lock(m_queue_mutex);

    task->m_actor = this;
    if (m_exclusive_running) {
        m_exclusive_queue.push_back(*task);
        return false;
    }

    m_exclusive_queue.push_back(*task);
    m_exclusive_running = true;
    return true;
}

void actor_base::complete_exclusive_task() {
    std::lock_guard lock(m_queue_mutex);

    if (m_exclusive_queue.empty()) {
        m_exclusive_running = false;
        return;
    }

    auto* task = m_exclusive_queue.front();
    task->execute();

    m_exclusive_queue.pop_front();
    task->destroy();
}

bool actor_base::is_exclusive_running() const {
    std::lock_guard lock(m_queue_mutex);
    return m_exclusive_running;
}

std::size_t actor_base::exclusive_queue_size() const {
    std::lock_guard lock(m_queue_mutex);

    std::size_t count = 0;
    for (auto it = m_exclusive_queue.begin(); it != m_exclusive_queue.end(); ++it) {
        ++count;
    }
    return count;
}

void actor_base::remove_exclusive_task(mcpy::runtime::operation_base* task) {
    std::lock_guard lock(m_queue_mutex);

    if (task->is_queued()) {
        m_exclusive_queue.erase(m_exclusive_queue.s_iterator_to(*task));
    }
}

} // namespace mcpy
