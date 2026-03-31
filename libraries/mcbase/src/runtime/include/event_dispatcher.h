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

#ifndef MC_RUNTIME_EVENT_DISPATCHER_H
#define MC_RUNTIME_EVENT_DISPATCHER_H

#include <mc/event.h>

#include <cstdint>
#include <memory>

namespace mc {
class event;
class object;
} // namespace mc

namespace mc::runtime {

class event_dispatcher {
public:
    event_dispatcher();
    ~event_dispatcher();

    event_dispatcher(const event_dispatcher&)            = delete;
    event_dispatcher& operator=(const event_dispatcher&) = delete;

    void send_event(mc::object& target, mc::event& e);
    void post_event(mc::object& target, std::unique_ptr<mc::event> e, int priority);
    void send_posted_events(mc::object* target = nullptr, mc::event_type_id type = mc::invalid_event_type);
    void remove_posted_events(mc::object* target = nullptr, mc::event_type_id type = mc::invalid_event_type);

private:
    class impl;

    void schedule_receiver_drain(mc::object& target, uint64_t receiver_token);
    void drain_receiver(uint64_t receiver_token);
    void deliver_to_object(mc::object& target, mc::event& e) const;

    std::unique_ptr<impl> m_impl;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_EVENT_DISPATCHER_H
