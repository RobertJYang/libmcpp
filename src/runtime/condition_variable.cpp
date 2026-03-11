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

#include <mc/log.h>
#include <mc/runtime/condition_variable.h>

namespace mc::runtime {

void condition_variable::WaiterNode::link(WaiterList& list)
{
    if (!list.head) {
        list.head = this;
        list.tail = this;
    } else {
        list.tail->next = this;
        this->prev      = list.tail;
        list.tail       = this;
    }
}

void condition_variable::WaiterNode::unlink(WaiterList& list)
{
    if (this->prev) {
        this->prev->next = this->next;
    } else {
        if (list.head == this) {
            list.head = this->next;
        }
    }
    if (this->next) {
        this->next->prev = this->prev;
    } else {
        if (list.tail == this) {
            list.tail = this->prev;
        }
    }
    this->next = nullptr;
    this->prev = nullptr;
}

void condition_variable::add_waiter(WaiterNode* node)
{
    auto lock = m_waiter_list.lock();
    node->link(*lock);
}

void condition_variable::remove_waiter(WaiterNode* node)
{
    auto lock = m_waiter_list.lock();
    node->unlink(*lock);
}

void condition_variable::notify_one() noexcept
{
    auto lock = m_waiter_list.lock();
    if (lock->head) {
        auto* node = lock->head;
        node->unlink(*lock);

        node->notified.store(true, std::memory_order_release);
        boost::asio::post(node->shard->ctx->get_executor(), [] {
        });
        m_cv.notify_one();
    }
}

void condition_variable::notify_all() noexcept
{
    auto  lock = m_waiter_list.lock();
    auto* node = lock->head;
    lock->head = nullptr;
    lock->tail = nullptr;
    while (node) {
        auto* next = node->next;
        node->next = nullptr;
        node->prev = nullptr;
        node->notified.store(true, std::memory_order_release);
        boost::asio::post(node->shard->ctx->get_executor(), [] {
        });
        node = next;
    }
    m_cv.notify_all();
}

void condition_variable::report_recursion_depth_exceeded()
{
    wlog("嵌套调度深度超过最大递归深度 {}, 回退到传统的条件变量等待", thread_pool::MAX_RECURSION_DEPTH);
}

} // namespace mc::runtime
