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

#include <mc/event.h>
#include <mc/memory.h>
#include <mc/object.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/thread_pool.h>
#include <mc/signal/signal.h>
#include <test_utilities/base.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

struct test_event : mc::event {
    static constexpr mc::event_type_id type_id = mc::builtin_event_type("tests.mcbase.event_dispatcher");

    test_event() : mc::event(type_id)
    {}
};

struct named_test_event : mc::event {
    static constexpr mc::event_type_id type_id = mc::builtin_event_type("tests.mcbase.event_dispatcher.named");

    explicit named_test_event(std::string value, mc::event_type_id event_type = type_id)
        : mc::event(event_type), name(std::move(value))
    {}

    std::string name;
};

struct secondary_test_event : mc::event {
    static constexpr mc::event_type_id type_id = mc::builtin_event_type("tests.mcbase.event_dispatcher.secondary");

    explicit secondary_test_event(std::string value) : mc::event(type_id), name(std::move(value))
    {}

    std::string name;
};

std::unique_ptr<mc::event> make_named_event(std::string name, mc::event_type_id type = named_test_event::type_id)
{
    if (type == secondary_test_event::type_id) {
        return std::make_unique<secondary_test_event>(std::move(name));
    }
    return std::make_unique<named_test_event>(std::move(name), type);
}

class event_recording_object : public mc::object {
public:
    using mc::object::object;

    std::vector<std::string> trace;
    bool                     accept_in_on_event{false};
    bool                     accept_in_event_filter{false};
    bool                     record_virtual_filter{true};
    std::promise<void>*      on_event_promise{nullptr};

protected:
    void on_event(mc::event& e) override
    {
        if (e.type() == test_event::type_id) {
            trace.push_back(std::string(get_name()) + ":on_event");
            if (on_event_promise != nullptr) {
                auto* promise    = on_event_promise;
                on_event_promise = nullptr;
                promise->set_value();
            }
        }
        if (accept_in_on_event) {
            e.accept();
        }
    }

    bool event_filter(mc::object* child, mc::event& e) override
    {
        if (record_virtual_filter && child != nullptr && e.type() == test_event::type_id) {
            auto child_name = child ? std::string(child->get_name()) : std::string("<null>");
            trace.push_back(std::string(get_name()) + ":event_filter:" + child_name);
        }
        if (accept_in_event_filter) {
            return true;
        }
        return false;
    }
};

class blocking_event_object : public event_recording_object {
public:
    using event_recording_object::event_recording_object;

    std::shared_future<void> release_gate;
    std::promise<void>*      entered_promise{nullptr};

protected:
    void on_event(mc::event& e) override
    {
        if (e.type() == test_event::type_id && entered_promise != nullptr) {
            entered_promise->set_value();
            release_gate.wait();
        }
        event_recording_object::on_event(e);
    }
};

class lane_recording_object : public mc::object {
public:
    using mc::object::object;

    ~lane_recording_object() noexcept override
    {
        if (destroyed_promise != nullptr) {
            destroyed_promise->set_value();
            destroyed_promise = nullptr;
        }
    }

    std::vector<std::string>        trace;
    std::promise<void>*             entered_promise{nullptr};
    std::promise<void>*             destroyed_promise{nullptr};
    std::shared_future<void>        release_gate;
    mc::runtime::any_executor       expected_executor;
    std::atomic<bool>               check_expected_executor{false};
    std::atomic<bool>               ran_on_expected_executor{false};
    std::atomic<int>                active_handlers{0};
    std::atomic<bool>               saw_parallel_entry{false};
    std::function<void(mc::event&)> before_wait;
    std::function<void(mc::event&)> after_record;

protected:
    void on_event(mc::event& e) override
    {
        auto active_now = active_handlers.fetch_add(1) + 1;
        if (active_now > 1) {
            saw_parallel_entry.store(true);
        }

        if (check_expected_executor.load()) {
            ran_on_expected_executor.store(expected_executor.running_in_this_thread());
        }

        if (entered_promise != nullptr) {
            auto* promise   = entered_promise;
            entered_promise = nullptr;
            promise->set_value();
        }

        if (before_wait) {
            before_wait(e);
        }

        if (release_gate.valid()) {
            release_gate.wait();
        }

        if (auto* named = dynamic_cast<named_test_event*>(&e)) {
            std::lock_guard<std::mutex> lock(trace_mutex);
            trace.push_back(named->name);
        } else if (auto* secondary = dynamic_cast<secondary_test_event*>(&e)) {
            std::lock_guard<std::mutex> lock(trace_mutex);
            trace.push_back(secondary->name);
        }

        if (after_record) {
            after_record(e);
        }

        active_handlers.fetch_sub(1);
    }

private:
    std::mutex trace_mutex;
};

class EventDispatcherTest : public mc::test::TestWithRuntime {};

} // namespace

TEST(EventTypeRegistryTest, RegisterEventTypeReturnsStableIdForSameName)
{
    auto first  = mc::register_event_type("tests.core.same_name");
    auto second = mc::register_event_type("tests.core.same_name");

    EXPECT_EQ(first, second);
    EXPECT_GE(first, mc::event_type_id_first_user);
    EXPECT_FALSE(mc::is_builtin_event_type(first));
}

TEST(EventTypeRegistryTest, RegisterEventTypeReturnsDifferentIdsForDifferentNames)
{
    auto first  = mc::register_event_type("tests.core.first_name");
    auto second = mc::register_event_type("tests.core.second_name");

    EXPECT_NE(first, second);
    EXPECT_GE(first, mc::event_type_id_first_user);
    EXPECT_GE(second, mc::event_type_id_first_user);
}

TEST(EventTypeRegistryTest, RegisterAnonymousEventTypeReturnsUniqueIds)
{
    auto first  = mc::register_event_type();
    auto second = mc::register_event_type();

    EXPECT_NE(first, second);
    EXPECT_GE(first, mc::event_type_id_first_user);
    EXPECT_GE(second, mc::event_type_id_first_user);
}

TEST(EventTypeRegistryTest, BuiltinEventTypeIdsStayInReservedRange)
{
    EXPECT_TRUE(mc::is_builtin_event_type(test_event::type_id));
    EXPECT_GE(test_event::type_id, mc::event_type_id_first_builtin);
    EXPECT_LE(test_event::type_id, mc::event_type_id_last_builtin);
    EXPECT_LT(test_event::type_id, mc::event_type_id_first_user);
}

TEST(EventTypeRegistryTest, ReservedEventTypeRangeIsSeparatedFromBuiltinAndUser)
{
    auto reserved_id = mc::event_type_id_last_builtin + 1;

    EXPECT_FALSE(mc::is_builtin_event_type(reserved_id));
    EXPECT_FALSE(mc::is_user_event_type(reserved_id));
    EXPECT_TRUE(mc::is_reserved_event_type(reserved_id));
}

TEST_F(EventDispatcherTest, SendEventAcceptStopsPropagation)
{
    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    auto child = mc::make_shared<event_recording_object>(parent.get());
    child->set_name("child");
    child->accept_in_on_event = true;

    test_event event;
    child->send_event(event);

    EXPECT_EQ(child->trace, std::vector<std::string>{"child:on_event"});
    EXPECT_TRUE(parent->trace.empty());
}

TEST_F(EventDispatcherTest, SendEventBubblesToParent)
{
    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    auto child = mc::make_shared<event_recording_object>(parent.get());
    child->set_name("child");

    test_event event;
    child->send_event(event);

    EXPECT_EQ(child->trace, std::vector<std::string>{"child:on_event"});
    EXPECT_EQ(parent->trace, (std::vector<std::string>{"parent:event_filter:child", "parent:on_event"}));
}

TEST_F(EventDispatcherTest, RuntimeContextSendEventBubblesToParent)
{
    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    auto child = mc::make_shared<event_recording_object>(parent.get());
    child->set_name("child");

    test_event event;
    mc::runtime::get_runtime_context().send_event(*child, event);

    EXPECT_EQ(child->trace, std::vector<std::string>{"child:on_event"});
    EXPECT_EQ(parent->trace, (std::vector<std::string>{"parent:event_filter:child", "parent:on_event"}));
}

TEST_F(EventDispatcherTest, PostEventRunsAsynchronouslyAndBubblesToParent)
{
    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    auto child = mc::make_shared<blocking_event_object>(parent.get());
    child->set_name("child");

    std::promise<void> entered;
    auto               entered_future = entered.get_future();
    std::promise<void> release;
    child->entered_promise = &entered;
    child->release_gate    = release.get_future().share();

    std::promise<void> parent_done;
    auto               parent_done_future = parent_done.get_future();
    parent->on_event_promise              = &parent_done;

    child->post_event(std::make_unique<test_event>());

    EXPECT_EQ(entered_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(parent->trace.empty());

    release.set_value();

    EXPECT_EQ(parent_done_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(child->trace, std::vector<std::string>{"child:on_event"});
    EXPECT_EQ(parent->trace, (std::vector<std::string>{"parent:event_filter:child", "parent:on_event"}));
}

TEST_F(EventDispatcherTest, PostEventDropsWhenTargetDestroyedBeforeDelivery)
{
    auto work_executor = mc::runtime::get_runtime_context().get_work_executor();

    std::promise<void> worker_started;
    auto               worker_started_future = worker_started.get_future();
    std::promise<void> release_worker;
    auto               release_worker_future = release_worker.get_future().share();
    work_executor.post([&]() {
        worker_started.set_value();
        release_worker_future.wait();
    });
    ASSERT_EQ(worker_started_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    auto child = mc::make_shared<event_recording_object>(parent.get());
    child->set_name("child");

    child->post_event(std::make_unique<test_event>());
    child->set_parent(nullptr);
    child.reset();

    std::promise<void> drain_done;
    auto               drain_done_future = drain_done.get_future();
    work_executor.post([&]() {
        drain_done.set_value();
    });

    release_worker.set_value();
    ASSERT_EQ(drain_done_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_TRUE(parent->trace.empty());
}

TEST_F(EventDispatcherTest, PostEventUsesTargetEventLaneInsteadOfGlobalWorkQueue)
{
    auto& ctx = mc::runtime::get_runtime_context();

    auto               work_executor = ctx.get_work_executor();
    std::promise<void> global_worker_started;
    auto               global_worker_started_future = global_worker_started.get_future();
    std::promise<void> release_global_worker;
    auto               release_global_worker_future = release_global_worker.get_future().share();
    work_executor.post([&global_worker_started, release_global_worker_future]() mutable {
        global_worker_started.set_value();
        release_global_worker_future.wait();
    });
    ASSERT_EQ(global_worker_started_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    mc::runtime::thread_pool target_pool(1, "mcbase_event_lane");
    target_pool.start();

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");
    target->set_executor(mc::runtime::any_executor(target_pool.get_executor()));
    target->expected_executor = mc::runtime::any_executor(target_pool.get_executor());
    target->check_expected_executor.store(true);

    std::promise<void> entered;
    auto               entered_future = entered.get_future();
    target->entered_promise           = &entered;

    ctx.post_event(*target, make_named_event("lane"), 0);

    EXPECT_EQ(entered_future.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    EXPECT_TRUE(target->ran_on_expected_executor.load());

    release_global_worker.set_value();
}

TEST_F(EventDispatcherTest, PostEventSerializesPerTargetObject)
{
    auto& ctx = mc::runtime::get_runtime_context();

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");

    std::promise<void> first_entered;
    auto               first_entered_future = first_entered.get_future();
    std::promise<void> second_recorded;
    auto               second_recorded_future = second_recorded.get_future();
    std::promise<void> release_first;
    target->entered_promise = &first_entered;
    target->release_gate    = release_first.get_future().share();
    target->after_record    = [&](mc::event& e) {
        if (auto* named = dynamic_cast<named_test_event*>(&e); named != nullptr && named->name == "second") {
            second_recorded.set_value();
        }
    };

    ctx.post_event(*target, make_named_event("first"), 0);
    ASSERT_EQ(first_entered_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ctx.post_event(*target, make_named_event("second"), 0);

    EXPECT_EQ(second_recorded_future.wait_for(std::chrono::milliseconds(300)), std::future_status::timeout);
    EXPECT_FALSE(target->saw_parallel_entry.load());

    release_first.set_value();

    ASSERT_EQ(second_recorded_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(target->trace, (std::vector<std::string>{"first", "second"}));
}

TEST_F(EventDispatcherTest, PostEventHonorsPriorityWithinSameReceiver)
{
    auto& ctx = mc::runtime::get_runtime_context();

    // 运行时 work 线程池可能有 N>1 个 worker；只阻塞一个 worker，剩余 worker 会
    // 在我们 post_event 期间见缝插针把已入队的"low"取走先跑，破坏优先级排序窗口。
    // 这里按 shard_count() 把所有 worker 同时阻塞，确保三个事件全部入优先级队列后
    // 才放行。
    auto               work_executor = ctx.get_work_executor();
    const std::size_t  worker_count  = ctx.work().shard_count();
    std::atomic<std::size_t> workers_running{0};
    std::promise<void>       release_global_workers;
    auto release_global_workers_future = release_global_workers.get_future().share();
    for (std::size_t i = 0; i < worker_count; ++i) {
        work_executor.post([&workers_running, release_global_workers_future]() mutable {
            workers_running.fetch_add(1, std::memory_order_acq_rel);
            release_global_workers_future.wait();
        });
    }
    // 等所有 worker 都进入阻塞状态。1 秒兜底，避免线程池被诡异占用时无限等。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (workers_running.load(std::memory_order_acquire) < worker_count) {
        ASSERT_LT(std::chrono::steady_clock::now(), deadline) << "等待 work workers 全部进入阻塞超时";
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");

    std::promise<void> done;
    auto               done_future = done.get_future();
    target->after_record           = [&](mc::event& e) {
        if (auto* named = dynamic_cast<named_test_event*>(&e); named != nullptr && named->name == "low") {
            done.set_value();
        }
    };

    ctx.post_event(*target, make_named_event("low"), -10);
    ctx.post_event(*target, make_named_event("high"), 10);
    ctx.post_event(*target, make_named_event("normal"), 0);

    release_global_workers.set_value();

    ASSERT_EQ(done_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(target->trace, (std::vector<std::string>{"high", "normal", "low"}));
}

TEST_F(EventDispatcherTest, RuntimeContextRemovePostedEventsCancelsPendingEvents)
{
    auto& ctx = mc::runtime::get_runtime_context();

    mc::runtime::thread_pool target_pool(1, "mcbase_remove_posted_events");
    target_pool.start();

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");
    target->set_executor(mc::runtime::any_executor(target_pool.get_executor()));

    std::promise<void> blocker_started;
    auto               blocker_started_future = blocker_started.get_future();
    std::promise<void> release_blocker;
    auto               release_blocker_future = release_blocker.get_future().share();
    target_pool.get_executor().post([&blocker_started, release_blocker_future]() mutable {
        blocker_started.set_value();
        release_blocker_future.wait();
    });
    ASSERT_EQ(blocker_started_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ctx.post_event(*target, make_named_event("drop-a"), 0);
    ctx.post_event(*target, make_named_event("drop-b", secondary_test_event::type_id), 0);
    ctx.remove_posted_events(target.get(), named_test_event::type_id);

    release_blocker.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(target->trace, (std::vector<std::string>{"drop-b"}));

    target->trace.clear();

    std::promise<void> blocker_started_again;
    auto               blocker_started_again_future = blocker_started_again.get_future();
    std::promise<void> release_blocker_again;
    auto               release_blocker_again_future = release_blocker_again.get_future().share();
    target_pool.get_executor().post([&blocker_started_again, release_blocker_again_future]() mutable {
        blocker_started_again.set_value();
        release_blocker_again_future.wait();
    });
    ASSERT_EQ(blocker_started_again_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ctx.post_event(*target, make_named_event("drop-all-a"), 0);
    ctx.post_event(*target, make_named_event("drop-all-b", secondary_test_event::type_id), 0);
    ctx.remove_posted_events(target.get());

    release_blocker_again.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(target->trace.empty());
}

TEST_F(EventDispatcherTest, RuntimeContextSendPostedEventsFlushesMatchingPendingEvents)
{
    auto& ctx = mc::runtime::get_runtime_context();

    auto               work_executor = ctx.get_work_executor();
    std::promise<void> global_worker_started;
    auto               global_worker_started_future = global_worker_started.get_future();
    std::promise<void> release_global_worker;
    auto               release_global_worker_future = release_global_worker.get_future().share();
    work_executor.post([&global_worker_started, release_global_worker_future]() mutable {
        global_worker_started.set_value();
        release_global_worker_future.wait();
    });
    ASSERT_EQ(global_worker_started_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");

    ctx.post_event(*target, make_named_event("queued-before-a"), 0);
    ctx.post_event(*target, make_named_event("queued-before-b"), 0);

    ctx.send_posted_events(target.get());
    EXPECT_EQ(target->trace, (std::vector<std::string>{"queued-before-a", "queued-before-b"}));

    ctx.post_event(*target, make_named_event("queued-after"), 0);
    EXPECT_EQ(target->trace, (std::vector<std::string>{"queued-before-a", "queued-before-b"}));

    release_global_worker.set_value();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(target->trace, (std::vector<std::string>{"queued-before-a", "queued-before-b", "queued-after"}));
}

TEST_F(EventDispatcherTest, RuntimeContextSendPostedEventsPreservesReceiverLaneSerialization)
{
    auto& ctx = mc::runtime::get_runtime_context();

    mc::runtime::thread_pool target_pool(1, "mcbase_send_posted_events_lane");
    target_pool.start();

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");
    target->set_executor(mc::runtime::any_executor(target_pool.get_executor()));
    target->expected_executor = mc::runtime::any_executor(target_pool.get_executor());
    target->check_expected_executor.store(true);

    std::promise<void> first_entered;
    auto               first_entered_future = first_entered.get_future();
    std::promise<void> second_recorded;
    auto               second_recorded_future = second_recorded.get_future();
    std::promise<void> release_first;
    target->entered_promise = &first_entered;
    target->release_gate    = release_first.get_future().share();
    target->after_record    = [&](mc::event& e) {
        if (auto* named = dynamic_cast<named_test_event*>(&e); named != nullptr && named->name == "second") {
            second_recorded.set_value();
        }
    };

    ctx.post_event(*target, make_named_event("first"), 0);
    ASSERT_EQ(first_entered_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    ctx.post_event(*target, make_named_event("second"), 0);

    auto flush_future = std::async(std::launch::async, [&]() {
        ctx.send_posted_events(target.get());
    });

    EXPECT_EQ(flush_future.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
    EXPECT_EQ(second_recorded_future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    EXPECT_FALSE(target->saw_parallel_entry.load());

    release_first.set_value();

    EXPECT_EQ(flush_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(second_recorded_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(target->ran_on_expected_executor.load());
    EXPECT_FALSE(target->saw_parallel_entry.load());
    EXPECT_EQ(target->trace, (std::vector<std::string>{"first", "second"}));
}

TEST_F(EventDispatcherTest, ObjectDeleteLaterDefersDestructionUntilQueuedTurn)
{
    mc::runtime::thread_pool target_pool(1, "mcbase_delete_later");
    target_pool.start();

    auto target = mc::make_shared<lane_recording_object>();
    target->set_name("target");
    target->set_executor(mc::runtime::any_executor(target_pool.get_executor()));

    std::promise<void> blocker_started;
    auto               blocker_started_future = blocker_started.get_future();
    std::promise<void> release_blocker;
    auto               release_blocker_future = release_blocker.get_future().share();
    target_pool.get_executor().post([&blocker_started, release_blocker_future]() mutable {
        blocker_started.set_value();
        release_blocker_future.wait();
    });
    ASSERT_EQ(blocker_started_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    std::promise<void> destroyed;
    auto               destroyed_future = destroyed.get_future();
    target->destroyed_promise           = &destroyed;

    auto weak_target = target->weak_from_this();

    target->delete_later();
    target.reset();

    EXPECT_EQ(destroyed_future.wait_for(std::chrono::milliseconds(200)), std::future_status::timeout);
    EXPECT_FALSE(weak_target.expired());

    release_blocker.set_value();

    EXPECT_EQ(destroyed_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(weak_target.expired());
}

TEST_F(EventDispatcherTest, EventFiltersRunByPriorityAndCanStopLowerPriorityFilters)
{
    auto parent = mc::make_shared<event_recording_object>();
    parent->set_name("parent");
    parent->record_virtual_filter = false;
    auto child                    = mc::make_shared<event_recording_object>(parent.get());
    child->set_name("child");

    parent->install_event_filter([&](mc::object*, mc::event&) {
        parent->trace.push_back("installed:low");
        return false;
    }, mc::signal_priority::low);

    parent->install_event_filter([&](mc::object*, mc::event&) {
        parent->trace.push_back("installed:high");
        return true;
    }, mc::signal_priority::high);

    test_event event;
    child->send_event(event);

    EXPECT_EQ(child->trace, std::vector<std::string>{"child:on_event"});
    EXPECT_EQ(parent->trace, std::vector<std::string>{"installed:high"});
}
