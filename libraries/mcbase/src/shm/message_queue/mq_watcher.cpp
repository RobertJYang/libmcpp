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

#include "mq_watcher.h"

#include "mq_private.h"

namespace mc::shm::detail {

mq_watcher::mq_watcher(mc::runtime::any_executor executor, mq_queue* queue, mq_queue_event_handler handler,
                       source watcher_source)
    : m_queue(queue), m_handler(std::move(handler)), m_running(false), m_source(watcher_source)
{
    auto* notifier =
        queue == nullptr || queue->m_impl == nullptr
            ? nullptr
            : (watcher_source == source::space ? &queue->m_impl->space_notifier : &queue->m_impl->data_notifier);
    if (queue == nullptr || !queue->is_valid() || notifier == nullptr || !notifier->is_valid() || notifier->m_impl == nullptr) {
        return;
    }

#ifndef _WIN32
    m_waiter = std::make_unique<mc::io::native_waiter>(
        std::move(executor), mc::io::native_waiter::from_descriptor(notifier->native_handle()));
#else
    const auto duplicated = duplicate_wait_handle(notifier->m_impl->handle);
    if (duplicated != nullptr) {
        m_waiter = std::make_unique<mc::io::native_waiter>(std::move(executor),
                                                           mc::io::native_waiter::from_waitable_handle(duplicated));
    }
#endif
}

mq_watcher::~mq_watcher()
{
    stop();
}

void mq_watcher::start()
{
    if (m_running || m_queue == nullptr || !m_queue->is_valid()) {
        return;
    }

    auto* notifier =
        m_queue->m_impl == nullptr
            ? nullptr
            : (m_source == source::space ? &m_queue->m_impl->space_notifier : &m_queue->m_impl->data_notifier);
    if (notifier == nullptr) {
        return;
    }

    m_running = true;
    arm_wait();
    notifier->drain();
    if (m_handler) {
        m_handler();
    }
}

void mq_watcher::stop()
{
    m_running = false;

    if (m_waiter != nullptr) {
        m_waiter->close();
    }
}

void mq_watcher::arm_wait()
{
    if (m_waiter == nullptr) {
        return;
    }

    auto self = shared_from_this();
    m_waiter->async_wait(
#ifndef _WIN32
        mc::io::native_waiter::wait_type::read,
#else
        mc::io::native_waiter::wait_type::signal,
#endif
        [self](const std::error_code& ec) {
        if (ec || !self->m_running || self->m_queue == nullptr) {
            return;
        }

        auto* notifier =
            self->m_queue->m_impl == nullptr
                ? nullptr
                : (self->m_source == source::space ? &self->m_queue->m_impl->space_notifier
                                                   : &self->m_queue->m_impl->data_notifier);
        if (notifier != nullptr) {
            notifier->drain();
        }
        if (self->m_handler) {
            self->m_handler();
        }

        if (self->m_running) {
            self->arm_wait();
        }
    });
}

} // namespace mc::shm::detail
