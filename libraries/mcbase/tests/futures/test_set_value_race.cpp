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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <unordered_map>
#include <vector>

#include <mc/future.h>
#include <mc/runtime.h>
#include <mc/runtime/thread_pool.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

// 精确模拟 shm_pending_msgs 的并发行为
class test_pending_msgs {
public:
    bool send(mc::string_view name, uint32_t serial, mc::promise<int> promise)
    {
        std::lock_guard lock(m_mutex);
        auto&           serial_map = m_pending[mc::string(name)];
        if (serial_map.find(serial) != serial_map.end()) {
            return false;
        }
        return serial_map
            .emplace(std::piecewise_construct, std::forward_as_tuple(serial), std::forward_as_tuple(std::move(promise)))
            .second;
    }

    // 与 shm_pending_msgs::reply 完全一致的模式:
    //   持有 m_mutex → move promise → erase → set_value → 释放 m_mutex
    bool reply(mc::string_view name, uint32_t serial, int value)
    {
        std::lock_guard lock(m_mutex);
        auto&           serial_map = m_pending[mc::string(name)];
        auto            it         = serial_map.find(serial);
        if (it == serial_map.end()) {
            return false;
        }
        auto promise = std::move(it->second);
        serial_map.erase(it);
        promise.set_value(value);
        return true;
    }

    void remove(mc::string_view name, uint32_t serial)
    {
        std::lock_guard lock(m_mutex);
        auto            it = m_pending.find(mc::string(name));
        if (it == m_pending.end()) {
            return;
        }
        it->second.erase(serial);
    }

    void clear(mc::string_view name)
    {
        std::lock_guard lock(m_mutex);
        m_pending.erase(mc::string(name));
    }

private:
    std::mutex                                                                     m_mutex;
    std::unordered_map<mc::string, std::unordered_map<uint32_t, mc::promise<int>>> m_pending;
};

class SetValueRaceTest : public mc::test::TestWithRuntime {
    void SetUp() override
    {
        mc::test::TestWithRuntime::reset_runtime();
    }

    void TearDown() override
    {
        mc::test::TestWithRuntime::reset_runtime();
    }
};

// 复现“非 pool reply 线程 + pool waiter”形态下的竞态窗口:
//
//   coredump 调用链:
//     #0 condition_variable::notify_all()
//     #1 state_base::mark_ready()
//     #2 any_promise::set_value<local_msg>()
//     #3 shm_pending_msgs::reply()
//     #4 harbor::reply_shm_msg()
//     #5 harbor::process_local_message()
//
//   目标线程模型:
//     - 有 3 个独立 reply worker 线程（非 pool 线程）读消息队列并调用 reply
//     - 服务在 pool worker 线程上发送请求并 wait_for 等待回复
//     - pool 不析构，无外来线程
//
//   竞态窗口:
//     set_value 中 set_result_inner 设置 m_ready=true 后、mark_ready 的
//     notify_all 之前，pool worker 可能因超时退出 wait，释放 future 引用，
//     触发 state_pool 回收。下一轮 make_promise 复用同一内存时，如果
//     前一次的 notify_all 尚未完成，就可能访问已被重新初始化的内存。
TEST_F(SetValueRaceTest, non_pool_reply_workers_with_pool_waiters)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    test_pending_msgs pending;

    constexpr int  num_iterations    = 2500;
    constexpr int  num_reply_workers = 3;
    constexpr auto request_timeout   = 2ms;

    std::atomic<bool>     running{true};
    std::atomic<uint32_t> next_serial{1};
    std::atomic<int>      replied_count{0};
    std::atomic<int>      timeout_count{0};
    std::atomic<int>      sent_count{0};
    std::atomic<int>      done_count{0};
    auto                  all_done_promise = std::make_shared<std::promise<void>>();
    auto                  all_done_future  = all_done_promise->get_future();

    mc::string service_name = ":1.100";

    // 启动非 pool reply 线程，从队列读消息后调用 reply。
    std::vector<std::thread> reply_workers;
    for (int i = 0; i < num_reply_workers; ++i) {
        reply_workers.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                uint32_t serial = next_serial.load(std::memory_order_relaxed);
                for (uint32_t s = 1; s <= serial; ++s) {
                    if (pending.reply(service_name, s, static_cast<int>(s))) {
                        replied_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                std::this_thread::sleep_for(10us);
            }
        });
    }

    // pool worker 线程发送请求并等待
    for (int i = 0; i < num_iterations; ++i) {
        uint32_t serial = next_serial.fetch_add(1, std::memory_order_relaxed);

        auto promise = mc::make_promise<int>(pool.get_executor());
        auto future  = promise.get_future();
        pending.send(service_name, serial, std::move(promise));
        sent_count.fetch_add(1, std::memory_order_relaxed);

        // 在 pool worker 中等待 reply。
        pool.get_executor().post([f = std::move(future), &pending, &timeout_count, &done_count, serial, &service_name,
                                  request_timeout, all_done_promise]() mutable {
            auto status = f.wait_for(request_timeout);
            if (status == mc::futures::future_status::timeout) {
                timeout_count.fetch_add(1, std::memory_order_relaxed);
                pending.remove(service_name, serial);
            }
            if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == num_iterations) {
                all_done_promise->set_value();
            }
        });

        if (i % 1000 == 0) {
            std::this_thread::sleep_for(1ms);
        }
    }

    ASSERT_EQ(all_done_future.wait_for(10s), std::future_status::ready)
        << "等待 pool worker 完成超时: done=" << done_count.load() << "/" << num_iterations;

    running.store(false, std::memory_order_relaxed);

    for (auto& w : reply_workers) {
        w.join();
    }

    SUCCEED();
}

// 模拟“reply 中途清理 pending”并发:
//   非 pool reply 线程调 reply → set_value
//   另一个线程调 clear（模拟 unregister_service）销毁未完成的 promise
//   pool worker 线程等待 future
TEST_F(SetValueRaceTest, clear_pending_during_non_pool_reply)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    test_pending_msgs pending;

    constexpr int  num_rounds = 64;
    constexpr int  batch_size = 24;
    constexpr auto wait_time  = 1ms;

    mc::string service_name = ":1.200";

    for (int round = 0; round < num_rounds; ++round) {
        std::atomic<int> done_count{0};
        auto             round_done_promise = std::make_shared<std::promise<void>>();
        auto             round_done_future  = round_done_promise->get_future();

        // 批量创建 promise/future 并注册到 pending map
        for (int j = 0; j < batch_size; ++j) {
            auto promise = mc::make_promise<int>(pool.get_executor());
            auto future  = promise.get_future();
            pending.send(service_name, static_cast<uint32_t>(round * batch_size + j), std::move(promise));

            // future move 进 lambda，生命周期由 pool worker 管理
            pool.get_executor().post([f = std::move(future), &done_count, wait_time, round_done_promise]() mutable {
                f.wait_for(wait_time);
                if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == batch_size) {
                    round_done_promise->set_value();
                }
            });
        }
        std::this_thread::sleep_for(50us);

        // 非 pool reply 线程回复一半请求
        std::thread reply_thread([&]() {
            for (int j = 0; j < batch_size; j += 2) {
                pending.reply(service_name, static_cast<uint32_t>(round * batch_size + j), j);
            }
        });

        // 模拟 unregister_service: 清除所有未完成的 promise
        std::thread clear_thread([&]() {
            std::this_thread::sleep_for(50us);
            pending.clear(service_name);
        });

        reply_thread.join();
        clear_thread.join();

        ASSERT_EQ(round_done_future.wait_for(80ms), std::future_status::ready)
            << "等待 round 完成超时: round=" << round << " done=" << done_count.load() << "/" << batch_size;
    }

    SUCCEED();
}

// 高并发 state pool 回收 + notify_all 竞态:
//   关注 set_result_inner 设置 m_ready=true 到 mark_ready() 调用 notify_all()
//   之间的窗口。在该窗口中 waiter 超时退出 wait，future 引用释放，
//   state 被 pool 回收。新的 make_promise 可能复用同一块内存。
//
//   使用非 pool 线程调 set_value
TEST_F(SetValueRaceTest, state_pool_recycle_with_non_pool_reply)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    constexpr int num_iterations    = 15000;
    constexpr int num_reply_threads = 3;

    struct request {
        mc::promise<int>  promise;
        std::atomic<bool> wait_entered{false};
    };

    using req_ptr = std::shared_ptr<request>;

    std::mutex           queue_mutex;
    std::vector<req_ptr> request_queue;
    std::atomic<bool>    running{true};
    std::atomic<int>     done_count{0};
    auto                 all_done_promise = std::make_shared<std::promise<void>>();
    auto                 all_done_future  = all_done_promise->get_future();

    std::vector<std::thread> reply_threads;
    for (int i = 0; i < num_reply_threads; ++i) {
        reply_threads.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                req_ptr req;
                {
                    std::lock_guard lock(queue_mutex);
                    if (!request_queue.empty()) {
                        req = std::move(request_queue.back());
                        request_queue.pop_back();
                    }
                }
                if (req) {
                    while (!req->wait_entered.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                    {
                        auto local_promise = std::move(req->promise);
                        local_promise.set_value(42);
                    }
                } else {
                    std::this_thread::sleep_for(10us);
                }
            }
        });
    }

    for (int i = 0; i < num_iterations; ++i) {
        auto req     = std::make_shared<request>();
        req->promise = mc::make_promise<int>(pool.get_executor());
        auto future  = req->promise.get_future();

        {
            std::lock_guard lock(queue_mutex);
            request_queue.push_back(req);
        }

        pool.get_executor().post([f = std::move(future), req, &done_count, all_done_promise]() mutable {
            req->wait_entered.store(true, std::memory_order_release);
            f.wait_for(1ms);
            if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == num_iterations) {
                all_done_promise->set_value();
            }
        });

        if (i % 5000 == 0) {
            std::this_thread::sleep_for(2ms);
        }
    }

    ASSERT_EQ(all_done_future.wait_for(5s), std::future_status::ready)
        << "等待 state_pool_recycle_with_non_pool_reply 完成超时: done="
        << done_count.load() << "/" << num_iterations;
    running.store(false, std::memory_order_relaxed);

    for (auto& t : reply_threads) {
        t.join();
    }

    SUCCEED();
}

// 混合竞态: cancel + set_value + state_pool 回收
//   pool worker 等待超时后调 cancel
//   非 pool reply 线程几乎同时调 set_value
//   两者都可能触发 mark_ready → notify_all
//   同时 state_pool 快速回收复用
TEST_F(SetValueRaceTest, cancel_vs_reply_race_with_pool_recycle)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    test_pending_msgs pending;

    constexpr int  num_iterations = 10000;
    constexpr auto short_timeout  = 500us;

    mc::string service_name = ":1.300";

    std::atomic<bool>     running{true};
    std::atomic<uint32_t> reply_serial{0};
    std::atomic<int>      done_count{0};
    auto                  all_done_promise = std::make_shared<std::promise<void>>();
    auto                  all_done_future  = all_done_promise->get_future();

    // 非 pool reply 线程
    std::thread reply_thread([&]() {
        while (running.load(std::memory_order_relaxed)) {
            uint32_t serial = reply_serial.load(std::memory_order_acquire);
            for (uint32_t s = 1; s <= serial; ++s) {
                pending.reply(service_name, s, static_cast<int>(s));
            }
            std::this_thread::yield();
        }
    });

    for (int i = 0; i < num_iterations; ++i) {
        uint32_t serial = static_cast<uint32_t>(i + 1);

        auto promise = mc::make_promise<int>(pool.get_executor());
        auto future  = promise.get_future();

        pending.send(service_name, serial, std::move(promise));
        reply_serial.store(serial, std::memory_order_release);

        // pool worker: 短超时等待，超时后 cancel
        pool.get_executor().post(
            [f = std::move(future), &pending, serial, &service_name, short_timeout, &done_count,
             all_done_promise]() mutable {
            auto status = f.wait_for(short_timeout);
            if (status != mc::futures::future_status::ready) {
                f.cancel();
                pending.remove(service_name, serial);
            }
            // future 析构 → state_pool 回收
            if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == num_iterations) {
                all_done_promise->set_value();
            }
        });

        if (i % 5000 == 0) {
            std::this_thread::sleep_for(2ms);
        }
    }

    ASSERT_EQ(all_done_future.wait_for(5s), std::future_status::ready)
        << "等待 cancel_vs_reply_race_with_pool_recycle 完成超时: done="
        << done_count.load() << "/" << num_iterations;
    running.store(false, std::memory_order_relaxed);
    reply_thread.join();

    SUCCEED();
}

// 极端 state pool 复用: 保证前一个 state 被回收后立即被下一个复用
//   非 pool 线程 set_value → mark_ready → notify_all 的过程中，
//   如果 waiter 因为 set_result_inner 已设置 m_ready=true 而提前退出 wait，
//   释放 future 引用，当 promise 引用也释放时 state 回到 pool。
//   新的 make_promise 复用同一块内存，此时前次的 notify_all 可能还在访问
//   m_waiter_list（旧的 condition_variable 成员）
//
//   使用固定的非 pool reply 线程，避免大量线程创建
TEST_F(SetValueRaceTest, tight_state_recycle_loop)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    constexpr int num_iterations    = 15000;
    constexpr int num_reply_threads = 3;

    struct pending_item {
        mc::promise<int>  promise;
        std::atomic<bool> ready{false};
    };

    using item_ptr = std::shared_ptr<pending_item>;

    std::mutex            queue_mutex;
    std::vector<item_ptr> queue;
    std::atomic<bool>     running{true};
    std::atomic<int>      done_count{0};
    auto                  all_done_promise = std::make_shared<std::promise<void>>();
    auto                  all_done_future  = all_done_promise->get_future();

    std::vector<std::thread> reply_threads;
    for (int i = 0; i < num_reply_threads; ++i) {
        reply_threads.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                item_ptr item;
                {
                    std::lock_guard lock(queue_mutex);
                    if (!queue.empty()) {
                        item = std::move(queue.back());
                        queue.pop_back();
                    }
                }
                if (item) {
                    while (!item->ready.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                    item->promise.set_value(42);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (int i = 0; i < num_iterations; ++i) {
        auto item     = std::make_shared<pending_item>();
        item->promise = mc::make_promise<int>(pool.get_executor());
        auto future   = item->promise.get_future();

        {
            std::lock_guard lock(queue_mutex);
            queue.push_back(item);
        }

        pool.get_executor().post([f = std::move(future), item, &done_count, all_done_promise]() mutable {
            item->ready.store(true, std::memory_order_release);
            f.wait_for(2ms);
            if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == num_iterations) {
                all_done_promise->set_value();
            }
        });

        if (i % 10000 == 0) {
            std::this_thread::sleep_for(2ms);
        }
    }

    ASSERT_EQ(all_done_future.wait_for(5s), std::future_status::ready)
        << "等待 tight_state_recycle_loop 完成超时: done="
        << done_count.load() << "/" << num_iterations;
    running.store(false, std::memory_order_relaxed);
    for (auto& t : reply_threads) {
        t.join();
    }

    SUCCEED();
}

// 精确触发 wait_until_on_worker 中 timer handler 的 use-after-return:
//
//   wait_until_on_worker 的 timer handler 以引用方式捕获栈上的 bool timed_out。
//   poll_until 的循环是先检查谓词再调用 poll_tasks(ctx.poll)。
//   当超时为 0 时：
//     1. abs_time = now + 0 → 已过期
//     2. timer.async_wait(handler) → handler 注册到 shard 的 io_context
//     3. poll_until: 第一次 pred() → Clock::now() >= abs_time → true → 直接退出
//     4. poll_until 从未调用 ctx.poll()，timer handler 留在 io_context 队列中
//     5. wait_until_on_worker 返回 → 栈帧销毁 → timed_out 变量不再有效
//     6. shard 事件循环下一次 poll_tasks → ctx.poll() → 执行 timer handler
//     7. timer handler 写入已销毁的 timed_out → use-after-return
//
//   使用非 pool 线程同时调用 set_value 来触发 mark_ready → notify_all，
//   使 notify_all 可能访问被 use-after-return 破坏的 WaiterNode
TEST_F(SetValueRaceTest, timer_use_after_return_zero_timeout)
{
    auto& ctx  = mc::runtime::get_runtime_context();
    auto& pool = ctx.work();

    constexpr int num_iterations    = 30000;
    constexpr int num_reply_threads = 3;

    struct pending_item {
        mc::promise<int>  promise;
        std::atomic<bool> ready{false};
    };
    using item_ptr = std::shared_ptr<pending_item>;

    std::mutex            queue_mutex;
    std::vector<item_ptr> queue;
    std::atomic<bool>     running{true};

    // 非 pool 的 reply 线程
    std::vector<std::thread> reply_threads;
    for (int i = 0; i < num_reply_threads; ++i) {
        reply_threads.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                item_ptr item;
                {
                    std::lock_guard lock(queue_mutex);
                    if (!queue.empty()) {
                        item = std::move(queue.back());
                        queue.pop_back();
                    }
                }
                if (item) {
                    while (!item->ready.load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                    // set_value → mark_ready → notify_all（与 coredump 调用链一致）
                    item->promise.set_value(42);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::atomic<int> done_count{0};
    auto             all_done_promise = std::make_shared<std::promise<void>>();
    auto             all_done_future  = all_done_promise->get_future();

    for (int i = 0; i < num_iterations; ++i) {
        auto item     = std::make_shared<pending_item>();
        item->promise = mc::make_promise<int>(pool.get_executor());
        auto future   = item->promise.get_future();

        {
            std::lock_guard lock(queue_mutex);
            queue.push_back(item);
        }

        // pool worker: 0 超时等待，保证 timer handler 留在队列
        pool.get_executor().post([f = std::move(future), item, &done_count, all_done_promise]() mutable {
            item->ready.store(true, std::memory_order_release);
            // 0us 超时：poll_until 第一次 pred() 立即为真，不处理 timer handler
            f.wait_for(std::chrono::microseconds(0));
            if (done_count.fetch_add(1, std::memory_order_relaxed) + 1 == num_iterations) {
                all_done_promise->set_value();
            }
        });

        if (i % 10000 == 0) {
            std::this_thread::sleep_for(5ms);
        }
    }

    ASSERT_EQ(all_done_future.wait_for(10s), std::future_status::ready)
        << "等待 timer_use_after_return_zero_timeout 完成超时: done="
        << done_count.load() << "/" << num_iterations;

    running.store(false, std::memory_order_relaxed);
    for (auto& t : reply_threads) {
        t.join();
    }

    std::this_thread::sleep_for(50ms);
    SUCCEED();
}

} // namespace
