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

#ifndef _WIN32
#include <unistd.h>
#endif

namespace mc::shm::detail {

mq_watcher::mq_watcher(mc::runtime::any_executor executor, mq_queue* queue, mq_queue_event_handler handler,
                       source watcher_source)
    : m_queue(queue), m_handler(std::move(handler)), m_executor(executor), m_running(false), m_source(watcher_source)
{
    auto* notifier =
        queue == nullptr || queue->m_impl == nullptr
            ? nullptr
            : (watcher_source == source::space ? &queue->m_impl->space_notifier : &queue->m_impl->data_notifier);
    if (queue == nullptr || !queue->is_valid() || notifier == nullptr || !notifier->is_valid() ||
        notifier->m_impl == nullptr) {
        return;
    }

#ifndef _WIN32
    const auto duplicated_fd = ::dup(notifier->native_handle());
    if (duplicated_fd >= 0) {
        m_waiter =
            std::make_unique<mc::io::native_waiter>(m_executor, mc::io::native_waiter::from_descriptor(duplicated_fd));
    }
#else
    const auto duplicated = duplicate_wait_handle(notifier->m_impl->handle);
    if (duplicated != nullptr) {
        m_waiter = std::make_unique<mc::io::native_waiter>(m_executor,
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

    auto* notifier = m_queue->m_impl == nullptr ? nullptr
                                                : (m_source == source::space ? &m_queue->m_impl->space_notifier
                                                                             : &m_queue->m_impl->data_notifier);
    if (notifier == nullptr) {
        return;
    }

    m_running = true;
    notifier->drain();
    if (m_running) {
        arm_wait();
    }
    if (m_source == source::data && m_handler) {
        post_handler();
    }
}

void mq_watcher::stop()
{
    const auto current_thread = std::this_thread::get_id();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running         = false;
        m_queue           = nullptr;
        m_handler_pending = false;
    }

    if (m_waiter != nullptr) {
        m_waiter->close();
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_callback_thread != current_thread) {
        m_idle_cv.wait(lock, [this]() {
            return !m_callback_running;
        });
    }
    if (m_handler_thread != current_thread) {
        m_idle_cv.wait(lock, [this]() {
            return !m_handler_running;
        });
    }
}

void mq_watcher::arm_wait()
{
    auto                        self = shared_from_this();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || m_waiter == nullptr) {
        return;
    }

    m_waiter->async_wait(
#ifndef _WIN32
        mc::io::native_waiter::wait_type::read,
#else
        mc::io::native_waiter::wait_type::signal,
#endif
        [self](const std::error_code& ec) {
        mq_queue* queue = nullptr;
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_callback_running = true;
            self->m_callback_thread  = std::this_thread::get_id();
            if (ec || !self->m_running || self->m_queue == nullptr) {
                self->m_callback_running = false;
                self->m_callback_thread  = {};
                self->m_idle_cv.notify_all();
                return;
            }
            queue = self->m_queue;
        }

        auto* notifier = queue->m_impl == nullptr ? nullptr
                                                  : (self->m_source == source::space ? &queue->m_impl->space_notifier
                                                                                     : &queue->m_impl->data_notifier);
        if (notifier != nullptr) {
            notifier->drain();
        }
        self->post_handler();

        bool should_rearm = false;
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            should_rearm = self->m_running;
        }
        if (should_rearm) {
            self->arm_wait();
        }
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_callback_running = false;
            self->m_callback_thread  = {};
            self->m_idle_cv.notify_all();
        }
    });
}

void mq_watcher::post_handler()
{
    auto self = shared_from_this();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) {
            return;
        }
        if (m_handler_running) {
            m_handler_pending = true;
            return;
        }
        m_handler_running = true;
    }
    m_executor.post([self]() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                self->m_handler_thread = std::this_thread::get_id();
                if (!self->m_running) {
                    self->m_handler_running = false;
                    self->m_handler_thread  = {};
                    self->m_idle_cv.notify_all();
                    return;
                }
            }
            if (self->m_handler) {
                self->m_handler();
            }
            {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                if (!self->m_handler_pending) {
                    self->m_handler_running = false;
                    self->m_handler_thread  = {};
                    self->m_idle_cv.notify_all();
                    return;
                }
                self->m_handler_pending = false;
            }
        }
    });
}

} // namespace mc::shm::detail
