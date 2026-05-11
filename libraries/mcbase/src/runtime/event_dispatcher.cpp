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

#include <mc/object.h>
#include <mc/runtime/runtime_context.h>

#include <algorithm>
#include <cstdint>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "runtime/include/event_dispatcher.h"

namespace mc::runtime {

namespace {

bool should_continue_propagation(const mc::event& e) noexcept
{
    return !e.is_accepted();
}

} // namespace

class event_dispatcher::impl {
public:
    struct posted_event_record {
        uint64_t                   sequence{0};
        int                        priority{0};
        mc::event_type_id          type{mc::invalid_event_type};
        uint64_t                   receiver_token{0};
        mc::object*                raw_target{nullptr};
        mc::weak_ptr<mc::object>   weak_target;
        mc::shared_ptr<mc::object> strong_target;
        std::unique_ptr<mc::event> event;
        bool                       cancelled{false};
    };

    struct receiver_state {
        std::vector<posted_event_record> queue;
        bool                             drain_scheduled{false};
    };

    std::mutex                                   mutex;
    uint64_t                                     next_sequence{1};
    std::unordered_map<uint64_t, receiver_state> receivers;
    std::unordered_map<mc::event_type_id, global_event_filter> global_filters;
};

event_dispatcher::event_dispatcher() : m_impl(std::make_unique<impl>())
{}

event_dispatcher::~event_dispatcher() = default;

void event_dispatcher::send_event(mc::object& target, mc::event& e)
{
    global_event_filter filter_copy;
    {
        std::lock_guard lock(m_impl->mutex);
        auto it = m_impl->global_filters.find(e.type());
        if (it != m_impl->global_filters.end()) {
            filter_copy = it->second;
        }
    }
    if (filter_copy) {
        filter_copy(target, e);
    }

    if (!should_continue_propagation(e)) {
        return;
    }

    deliver_to_object(target, e);
    if (!should_continue_propagation(e)) {
        return;
    }

    auto parent = target.get_parent();
    if (!parent) {
        return;
    }

    parent->propagate_event_from_child(&target, e);
}

void event_dispatcher::post_event(mc::object& target, std::unique_ptr<mc::event> e, int priority)
{
    if (!e) {
        return;
    }

    auto weak_target = target.weak_from_this();
    if (!weak_target) {
        return;
    }

    auto receiver_token  = target.get_event_receiver_token();
    bool should_schedule = false;

    {
        std::lock_guard lock(m_impl->mutex);
        auto&           state  = m_impl->receivers[receiver_token];
        auto&           record = state.queue.emplace_back();
        record.sequence        = m_impl->next_sequence++;
        record.priority        = priority;
        record.type            = e->type();
        record.receiver_token  = receiver_token;
        record.raw_target      = &target;
        record.weak_target     = std::move(weak_target);
        if (record.type == mc::deferred_delete_event_type) {
            record.strong_target = target.shared_from_this();
        }
        record.event = std::move(e);

        if (!state.drain_scheduled) {
            state.drain_scheduled = true;
            should_schedule       = true;
        }
    }

    if (should_schedule) {
        schedule_receiver_drain(target, receiver_token);
    }
}

void event_dispatcher::send_posted_events(mc::object* target, mc::event_type_id type)
{
    struct flush_batch {
        mc::shared_ptr<mc::object>              target;
        std::vector<impl::posted_event_record> records;
    };
    auto matches_posted_event = [target, type](const impl::posted_event_record& record) {
        if (target != nullptr && record.raw_target != target) {
            return false;
        }
        if (type != mc::invalid_event_type && record.type != type) {
            return false;
        }
        return true;
    };
    auto sort_posted_events = [](std::vector<impl::posted_event_record>& records) {
        std::stable_sort(records.begin(), records.end(),
                         [](const auto& lhs, const auto& rhs) {
                             if (lhs.priority != rhs.priority) {
                                 return lhs.priority > rhs.priority;
                             }
                             return lhs.sequence < rhs.sequence;
                         });
    };

    std::unordered_map<uint64_t, flush_batch> batches;

    {
        std::lock_guard lock(m_impl->mutex);
        for (auto& [receiver_token, state] : m_impl->receivers) {
            auto& batch = batches[receiver_token];

            std::vector<impl::posted_event_record> remaining;
            remaining.reserve(state.queue.size());
            for (auto& record : state.queue) {
                if (matches_posted_event(record)) {
                    if (!batch.target) {
                        batch.target = record.strong_target ? record.strong_target : record.weak_target.lock();
                    }
                    batch.records.push_back(std::move(record));
                } else {
                    remaining.push_back(std::move(record));
                }
            }
            state.queue = std::move(remaining);
        }
    }

    std::vector<std::future<void>> waiters;
    waiters.reserve(batches.size());

    for (auto& [receiver_token, batch] : batches) {
        MC_UNUSED(receiver_token);
        if (!batch.target || batch.records.empty()) {
            continue;
        }

        sort_posted_events(batch.records);

        auto done_future = std::make_shared<std::promise<void>>();
        waiters.push_back(done_future->get_future());

        auto executor = batch.target->get_event_executor();
        executor.dispatch([this, records = std::move(batch.records), done_future]() mutable {
            for (auto& record : records) {
                if (record.cancelled || !record.event) {
                    continue;
                }

                auto locked_target = record.strong_target ? record.strong_target : record.weak_target.lock();
                if (!locked_target) {
                    continue;
                }

                if (record.type == mc::deferred_delete_event_type) {
                    continue;
                }

                send_event(*locked_target, *record.event);
            }
            done_future->set_value();
        });
    }

    for (auto& waiter : waiters) {
        waiter.wait();
    }
}

void event_dispatcher::remove_posted_events(mc::object* target, mc::event_type_id type)
{
    std::lock_guard lock(m_impl->mutex);
    for (auto receiver_it = m_impl->receivers.begin(); receiver_it != m_impl->receivers.end();) {
        auto& state = receiver_it->second;

        std::vector<impl::posted_event_record> remaining;
        remaining.reserve(state.queue.size());
        for (auto& record : state.queue) {
            bool matches_target = target == nullptr || record.raw_target == target;
            bool matches_type   = type == mc::invalid_event_type || record.type == type;
            if (!(matches_target && matches_type)) {
                remaining.push_back(std::move(record));
            }
        }
        state.queue = std::move(remaining);

        if (state.queue.empty() && !state.drain_scheduled) {
            receiver_it = m_impl->receivers.erase(receiver_it);
            continue;
        }
        ++receiver_it;
    }
}

void event_dispatcher::schedule_receiver_drain(mc::object& target, uint64_t receiver_token)
{
    auto executor = target.get_event_executor();
    executor.post([this, receiver_token]() {
        drain_receiver(receiver_token);
    });
}

void event_dispatcher::drain_receiver(uint64_t receiver_token)
{
    while (true) {
        impl::posted_event_record record;

        {
            std::lock_guard lock(m_impl->mutex);
            auto            receiver_it = m_impl->receivers.find(receiver_token);
            if (receiver_it == m_impl->receivers.end()) {
                return;
            }

            auto& state = receiver_it->second;
            if (state.queue.empty()) {
                state.drain_scheduled = false;
                m_impl->receivers.erase(receiver_it);
                return;
            }

            std::size_t selected_index = 0;
            for (std::size_t i = 1; i < state.queue.size(); ++i) {
                const auto& current  = state.queue[i];
                const auto& selected = state.queue[selected_index];
                if (current.priority > selected.priority) {
                    selected_index = i;
                    continue;
                }
                if (current.priority == selected.priority && current.sequence < selected.sequence) {
                    selected_index = i;
                }
            }

            record = std::move(state.queue[selected_index]);
            state.queue.erase(state.queue.begin() + selected_index);
        }

        if (record.cancelled || !record.event) {
            continue;
        }

        auto locked_target = record.strong_target ? record.strong_target : record.weak_target.lock();
        if (!locked_target) {
            continue;
        }

        if (record.type == mc::deferred_delete_event_type) {
            continue;
        }

        send_event(*locked_target, *record.event);
    }
}

void event_dispatcher::deliver_to_object(mc::object& target, mc::event& e) const
{
    target.dispatch_event_filters(nullptr, e);
    if (!should_continue_propagation(e)) {
        return;
    }

    target.on_event(e);
}

global_event_filter event_dispatcher::install_global_filter(mc::event_type_id type, global_event_filter filter)
{
    std::lock_guard lock(m_impl->mutex);
    auto old = std::move(m_impl->global_filters[type]);
    m_impl->global_filters[type] = std::move(filter);
    return old;
}

void event_dispatcher::remove_global_filter(mc::event_type_id type)
{
    std::lock_guard lock(m_impl->mutex);
    m_impl->global_filters.erase(type);
}

} // namespace mc::runtime
