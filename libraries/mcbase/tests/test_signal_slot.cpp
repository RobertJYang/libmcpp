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
#include "mc/signal_slot.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include <mc/core/object.h>
#include <mc/string.h>

// 不使用using namespace mc;，避免命名冲突

// 测试基本的信号槽连接和触发
TEST(SignalSlotTest, BasicConnection)
{
    mc::signal<void(int)> sig;
    int                   value = 0;

    // 连接信号到lambda槽函数
    auto conn = sig.connect([&value](int v) {
        value = v;
    });

    // 触发信号
    sig(42);
    EXPECT_EQ(42, value);

    // 再次触发信号
    sig(100);
    EXPECT_EQ(100, value);
}

// 测试多个槽连接到同一个信号
TEST(SignalSlotTest, MultipleSlots)
{
    mc::signal<void(int)> sig;
    int                   value1 = 0;
    int                   value2 = 0;

    // 连接多个槽
    sig.connect([&value1](int v) {
        value1 = v;
    });
    sig.connect([&value2](int v) {
        value2 = v * 2;
    });

    // 触发信号
    sig(10);
    EXPECT_EQ(10, value1);
    EXPECT_EQ(20, value2);
}

// 测试断开连接
TEST(SignalSlotTest, Disconnect)
{
    mc::signal<void(int)> sig;
    int                   value = 0;

    // 连接信号到槽
    auto conn = sig.connect([&value](int v) {
        value = v;
    });

    // 触发信号
    sig(42);
    EXPECT_EQ(42, value);

    // 断开连接
    conn.disconnect();

    // 再次触发信号，值不应该改变
    sig(100);
    EXPECT_EQ(42, value);
}

// 测试断开所有连接
TEST(SignalSlotTest, DisconnectAll)
{
    mc::signal<void(int)> sig;
    int                   value1 = 0;
    int                   value2 = 0;

    // 连接多个槽
    sig.connect([&value1](int v) {
        value1 = v;
    });
    sig.connect([&value2](int v) {
        value2 = v;
    });

    // 触发信号
    sig(10);
    EXPECT_EQ(10, value1);
    EXPECT_EQ(10, value2);

    // 断开所有连接
    sig.disconnect_all();

    // 再次触发信号，值不应该改变
    sig(20);
    EXPECT_EQ(10, value1);
    EXPECT_EQ(10, value2);
}

// 测试连接管理器
TEST(SignalSlotTest, ConnectionManager)
{
    mc::signal<void(int)> sig;
    int                   value = 0;

    {
        // 创建一个作用域，在其中创建连接管理器
        mc::connection_manager manager;

        // 连接信号并将连接添加到管理器
        auto conn = sig.connect([&value](int v) {
            value = v;
        });
        manager.add(conn);

        // 触发信号
        sig(42);
        EXPECT_EQ(42, value);

        // 作用域结束，连接管理器被销毁，应该自动断开连接
    }

    // 再次触发信号，值不应该改变
    sig(100);
    EXPECT_EQ(42, value);
}

// 测试多参数信号
TEST(SignalSlotTest, MultipleParameters)
{
    mc::signal<void(int, mc::string)> sig;
    int                               value = 0;
    mc::string                        text;

    // 连接信号到槽
    sig.connect([&value, &text](int v, const mc::string& t) {
        value = v;
        text  = t;
    });

    // 触发信号
    sig(42, "hello");
    EXPECT_EQ(42, value);
    EXPECT_EQ("hello", text);
}

// 测试空信号
TEST(SignalSlotTest, EmptySignal)
{
    mc::signal<void(int)> sig;

    // 检查信号是否为空
    EXPECT_TRUE(sig.empty());

    // 连接一个槽
    auto conn = sig.connect([](int) {
    });
    EXPECT_FALSE(sig.empty());

    // 断开连接
    conn.disconnect();
    EXPECT_TRUE(sig.empty());
}

TEST(SignalSlotTest, SlotsAreInvokedByPriority)
{
    mc::signal<void()> sig("priority_signal");
    std::vector<int>   order;

    sig.connect([&order]() {
        order.push_back(2);
    });
    sig.connect([&order]() {
        order.push_back(3);
    }, mc::signal_priority::low);
    sig.connect([&order]() {
        order.push_back(1);
    }, mc::signal_priority::critical);

    sig();

    EXPECT_EQ((std::vector<int>{1, 2, 3}), order);
}

TEST(SignalSlotTest, ScopedConnectionDisconnectsOnScopeExit)
{
    mc::signal<void(int)> sig("scoped_connection_signal");
    int                   value = 0;

    {
        mc::scoped_connection conn(sig.connect([&value](int v) {
            value = v;
        }));

        sig(42);
        EXPECT_EQ(42, value);
    }

    sig(100);
    EXPECT_EQ(42, value);
}

TEST(SignalSlotTest, RecursiveEmitThrowsSignalRecursionException)
{
    mc::signal<void()> sig("recursive_signal");

    sig.connect([&sig]() {
        sig();
    });

    EXPECT_THROW(sig(), mc::signal_recursion_exception);
}

TEST(SignalSlotTest, CurrentSignalCallStackIsVisibleInsideSlot)
{
    mc::signal<void()> sig("stack_signal");
    bool               checked = false;

    sig.connect([&]() {
        auto stack = mc::current_signal_call_stack();
        ASSERT_EQ(1u, stack.size());
        EXPECT_EQ(&sig, stack[0].signal_addr);
        ASSERT_NE(nullptr, stack[0].signal_name);
        EXPECT_STREQ("stack_signal", stack[0].signal_name);
        EXPECT_EQ(1u, mc::current_signal_depth());
        checked = true;
    });

    sig();

    EXPECT_TRUE(checked);
    EXPECT_EQ(0u, mc::current_signal_depth());
    EXPECT_TRUE(mc::current_signal_call_stack().empty());
}

TEST(SignalSlotTest, ConcurrentEmitOnSameSignalIsSafe)
{
    mc::signal<void()> sig("concurrent_emit_signal");
    std::atomic<int>   count{0};

    sig.connect([&count]() {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int emit_thread_count      = 4;
    constexpr int emits_per_thread_count = 200;

    std::vector<std::thread> threads;
    threads.reserve(emit_thread_count);
    for (int index = 0; index < emit_thread_count; ++index) {
        threads.emplace_back([&sig]() {
            for (int emit_index = 0; emit_index < emits_per_thread_count; ++emit_index) {
                sig();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(emit_thread_count * emits_per_thread_count, count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, ConnectDuringEmitDoesNotAffectCurrentSnapshot)
{
    mc::signal<void()>       sig("connect_during_emit_signal");
    std::mutex               mutex;
    std::condition_variable  cv;
    bool                     entered     = false;
    bool                     allow_exit  = false;
    bool                     should_wait = true;
    std::atomic<int>         existing_count{0};
    std::atomic<int>         late_count{0};

    sig.connect([&]() {
        existing_count.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_one();
        if (should_wait) {
            cv.wait(lock, [&allow_exit]() {
                return allow_exit;
            });
            should_wait = false;
        }
    });

    std::thread emit_thread([&sig]() {
        sig();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&entered]() {
            return entered;
        });
    }

    sig.connect([&]() {
        late_count.fetch_add(1, std::memory_order_relaxed);
    });

    {
        std::lock_guard<std::mutex> lock(mutex);
        allow_exit = true;
    }
    cv.notify_one();
    emit_thread.join();

    EXPECT_EQ(1, existing_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, late_count.load(std::memory_order_relaxed));

    sig();

    EXPECT_EQ(2, existing_count.load(std::memory_order_relaxed));
    EXPECT_EQ(1, late_count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, DisconnectDuringEmitSkipsLaterSlot)
{
    mc::signal<void()>       sig("disconnect_during_emit_signal");
    std::mutex               mutex;
    std::condition_variable  cv;
    bool                     entered    = false;
    bool                     allow_exit = false;
    bool                     should_wait = true;
    std::atomic<int>         first_count{0};
    std::atomic<int>         second_count{0};

    sig.connect([&]() {
        first_count.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_one();
        if (should_wait) {
            cv.wait(lock, [&allow_exit]() {
                return allow_exit;
            });
            should_wait = false;
        }
    });

    auto second_conn = sig.connect([&]() {
        second_count.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread emit_thread([&sig]() {
        sig();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&entered]() {
            return entered;
        });
    }

    second_conn.disconnect();

    {
        std::lock_guard<std::mutex> lock(mutex);
        allow_exit = true;
    }
    cv.notify_one();
    emit_thread.join();

    EXPECT_EQ(1, first_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, second_count.load(std::memory_order_relaxed));

    sig();

    EXPECT_EQ(2, first_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, second_count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, DisconnectAllDuringEmitSkipsRemainingSlots)
{
    mc::signal<void()>       sig("disconnect_all_during_emit_signal");
    std::mutex               mutex;
    std::condition_variable  cv;
    bool                     entered     = false;
    bool                     allow_exit  = false;
    bool                     should_wait = true;
    std::atomic<int>         first_count{0};
    std::atomic<int>         second_count{0};
    std::atomic<int>         third_count{0};

    sig.connect([&]() {
        first_count.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_one();
        if (should_wait) {
            cv.wait(lock, [&allow_exit]() {
                return allow_exit;
            });
            should_wait = false;
        }
    });
    sig.connect([&]() {
        second_count.fetch_add(1, std::memory_order_relaxed);
    });
    sig.connect([&]() {
        third_count.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread emit_thread([&sig]() {
        sig();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&entered]() {
            return entered;
        });
    }

    sig.disconnect_all();

    {
        std::lock_guard<std::mutex> lock(mutex);
        allow_exit = true;
    }
    cv.notify_one();
    emit_thread.join();

    EXPECT_EQ(1, first_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, second_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, third_count.load(std::memory_order_relaxed));

    sig();

    EXPECT_EQ(1, first_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, second_count.load(std::memory_order_relaxed));
    EXPECT_EQ(0, third_count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, ConcurrentConnectsAreVisibleToLaterEmit)
{
    mc::signal<void()> sig("concurrent_connect_signal");
    std::atomic<int>   count{0};

    constexpr int connect_thread_count      = 4;
    constexpr int connects_per_thread_count = 50;

    std::vector<std::thread> threads;
    threads.reserve(connect_thread_count);
    for (int thread_index = 0; thread_index < connect_thread_count; ++thread_index) {
        threads.emplace_back([&sig, &count]() {
            for (int connect_index = 0; connect_index < connects_per_thread_count; ++connect_index) {
                sig.connect([&count]() {
                    count.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(connect_thread_count * connects_per_thread_count, static_cast<int>(sig.slot_count()));

    sig();

    EXPECT_EQ(connect_thread_count * connects_per_thread_count, count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, ConcurrentDisconnectsPreventLaterEmit)
{
    mc::signal<void()>            sig("concurrent_disconnect_signal");
    std::atomic<int>              count{0};
    std::vector<mc::connection>   connections;

    constexpr int connection_count = 200;
    connections.reserve(connection_count);
    for (int index = 0; index < connection_count; ++index) {
        connections.push_back(sig.connect([&count]() {
            count.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    constexpr int disconnect_thread_count = 4;
    std::vector<std::thread> threads;
    threads.reserve(disconnect_thread_count);
    for (int thread_index = 0; thread_index < disconnect_thread_count; ++thread_index) {
        threads.emplace_back([thread_index, &connections]() {
            for (std::size_t index = static_cast<std::size_t>(thread_index); index < connections.size();
                 index += disconnect_thread_count) {
                connections[index].disconnect();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(sig.empty());
    EXPECT_EQ(0u, sig.slot_count());

    sig();

    EXPECT_EQ(0, count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, RecursionGuardIsThreadLocalAcrossThreads)
{
    mc::signal<void()>      sig("thread_local_recursion_guard_signal");
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    first_entered = false;
    bool                    release_first = false;
    bool                    should_block  = true;
    std::atomic<int>        count{0};
    std::exception_ptr      second_error;

    sig.connect([&]() {
        count.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> lock(mutex);
        if (should_block) {
            should_block  = false;
            first_entered = true;
            cv.notify_one();
            cv.wait(lock, [&release_first]() {
                return release_first;
            });
        }
    });

    std::thread first_thread([&sig]() {
        sig();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&first_entered]() {
            return first_entered;
        });
    }

    std::thread second_thread([&]() {
        try {
            sig();
        } catch (...) {
            second_error = std::current_exception();
        }
    });

    second_thread.join();

    {
        std::lock_guard<std::mutex> lock(mutex);
        release_first = true;
    }
    cv.notify_one();
    first_thread.join();

    EXPECT_EQ(nullptr, second_error);
    EXPECT_EQ(2, count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, ConcurrentConnectDisconnectEmitStressMaintainsUsability)
{
    mc::signal<void()> sig("connect_disconnect_emit_stress_signal");
    std::atomic<bool>  stop{false};
    std::atomic<int>   stress_count{0};

    auto stable_conn = sig.connect([&stress_count]() {
        stress_count.fetch_add(1, std::memory_order_relaxed);
    });

    constexpr int emit_thread_count    = 3;
    constexpr int mutator_thread_count = 2;
    constexpr int iterations           = 400;

    std::vector<std::thread> threads;
    threads.reserve(emit_thread_count + mutator_thread_count);

    for (int thread_index = 0; thread_index < emit_thread_count; ++thread_index) {
        threads.emplace_back([&]() {
            for (int iteration = 0; iteration < iterations; ++iteration) {
                sig();
            }
        });
    }

    for (int thread_index = 0; thread_index < mutator_thread_count; ++thread_index) {
        threads.emplace_back([&]() {
            for (int iteration = 0; iteration < iterations; ++iteration) {
                auto temp = sig.connect([&stress_count]() {
                    stress_count.fetch_add(1, std::memory_order_relaxed);
                });
                if ((iteration % 2) == 0) {
                    sig();
                }
                temp.disconnect();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(stable_conn.connected());

    sig.disconnect_all();
    EXPECT_TRUE(sig.empty());

    std::atomic<int> final_count{0};
    sig.connect([&final_count]() {
        final_count.fetch_add(1, std::memory_order_relaxed);
    });

    sig();

    EXPECT_EQ(1, final_count.load(std::memory_order_relaxed));
}

TEST(SignalSlotTest, ObjectBindExecutorDispatchesOnExecutor)
{
    using namespace std::chrono_literals;

    mc::core::object         obj;
    mc::runtime::thread_pool pool(1, "object_bind_executor_test");
    pool.start();
    obj.set_executor(mc::runtime::any_executor(pool.get_executor()));

    std::promise<void> done;
    auto               future = done.get_future();
    std::atomic<bool>  ran{false};
    std::atomic<bool>  ran_on_executor{false};
    std::thread::id    callback_thread;

    auto bound_handler = obj.bind_executor([&]() {
        callback_thread = std::this_thread::get_id();
        ran.store(true);
        ran_on_executor.store(obj.get_executor().running_in_this_thread());
        done.set_value();
    });

    bound_handler();

    EXPECT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(ran_on_executor.load());
    EXPECT_NE(callback_thread, std::this_thread::get_id());
}