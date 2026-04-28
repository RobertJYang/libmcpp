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

#ifndef MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_WATCHER_H
#define MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_WATCHER_H

#include <mc/common.h>
#include <memory>

#include <mc/io/native_waiter.h>
#include <mc/runtime/any_executor.h>
#include <mc/shm/message_queue/mq_queue.h>

namespace mc::shm::detail {

using mq_queue_event_handler = mc::small_function<void(), 64>;

class MC_API mq_watcher : public std::enable_shared_from_this<mq_watcher> {
public:
    enum class source : std::uint8_t {
        data  = 0,
        space = 1,
    };

    mq_watcher(mc::runtime::any_executor executor, mq_queue* queue, mq_queue_event_handler handler,
               source watcher_source = source::data);
    ~mq_watcher();

    mq_watcher(const mq_watcher&)            = delete;
    mq_watcher& operator=(const mq_watcher&) = delete;

    void start();
    void stop();

private:
    void arm_wait();

    mq_queue*                              m_queue;
    mq_queue_event_handler                 m_handler;
    bool                                   m_running;
    source                                 m_source;
    std::unique_ptr<mc::io::native_waiter> m_waiter;
};

} // namespace mc::shm::detail

#endif // MC_SHM_MESSAGE_QUEUE_PRIVATE_MQ_WATCHER_H
