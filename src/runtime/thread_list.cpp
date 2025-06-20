/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/runtime/thread_list.h>

namespace mc::runtime {

/* ------------------------ thread_node ----------------------- */

thread_node::thread_node(std::function<void()> func) : m_thread(std::move(func)) {
}

thread_node::thread_node(thread_node&& other) noexcept : m_thread(std::move(other.m_thread)) {
}

thread_node::~thread_node() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool thread_node::joinable() const {
    return m_thread.joinable();
}

void thread_node::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

std::thread::id thread_node::get_id() const {
    return m_thread.get_id();
}

/* ------------------------ thread_list ----------------------- */

thread_list::~thread_list() {
    clear();
}

void thread_list::start_threads(std::size_t thread_count, std::function<void()> func) {
    for (std::size_t i = 0; i < thread_count; ++i) {
        add_thread(func);
    }
}

void thread_list::start_threads(std::size_t thread_count, std::function<void(std::size_t)> func) {
    for (std::size_t i = 0; i < thread_count; ++i) {
        add_thread([func, i]() {
            func(i);
        });
    }
}

thread_node* thread_list::add_thread(std::function<void()> func) {
    // 创建新的线程节点，由 thread_list 完全拥有
    auto* node = new thread_node(std::move(func));
    m_threads.push_back(*node);
    return node;
}

bool thread_list::remove_thread(thread_node* node) {
    if (!node) {
        return false;
    }

    // 检查节点是否在链表中
    bool found = false;
    for (auto& existing_node : m_threads) {
        if (&existing_node == node) {
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    // 等待线程完成
    node->join();

    // 从链表中移除并删除
    m_threads.erase(m_threads.iterator_to(*node));
    delete node;

    return true;
}

void thread_list::join_all() {
    for (auto& node : m_threads) {
        node.join();
    }
}

void thread_list::clear() {
    // 等待所有线程完成并清理
    m_threads.clear_and_dispose([](thread_node* node) {
        // 线程节点的析构函数会处理 join
        delete node;
    });
}

std::size_t thread_list::get_thread_count() const {
    return m_threads.size();
}

bool thread_list::empty() const {
    return m_threads.empty();
}

} // namespace mc::runtime