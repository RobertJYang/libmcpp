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

#include <gtest/gtest.h>
#include <mc/runtime.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_executor.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace mc::runtime;

namespace {

class RuntimeExecutorTest : public mc::test::TestWithRuntime {
protected:
    void SetUp() override {
        mc::test::TestWithRuntime::reset_runtime();
        runtime_config config;
        config.io_threads   = 2;
        config.work_threads = 2;
        mc::get_runtime_context().initialize(config);
        mc::get_runtime_context().start();
    }

    void TearDown() override {
        mc::get_runtime_context().stop();
        mc::get_runtime_context().join();
        mc::test::TestWithRuntime::reset_runtime();
    }
};

} // namespace

// 测试 runtime_executor 的基本构造
TEST_F(RuntimeExecutorTest, basic_construction) {
    // 默认构造
    runtime_executor exec1;
    EXPECT_TRUE(true);

    runtime_executor exec2(mc::get_runtime_context());
    EXPECT_TRUE(true);

    EXPECT_EQ(exec1, exec2);
}

// 测试 runtime_executor 的拷贝和移动
TEST_F(RuntimeExecutorTest, copy_and_move) {
    runtime_executor original;

    // 拷贝构造
    runtime_executor copied(original);
    EXPECT_EQ(original, copied);

    // 拷贝赋值
    runtime_executor assigned;
    assigned = original;
    EXPECT_EQ(original, assigned);

    // 移动构造
    runtime_executor moved(std::move(copied));
    EXPECT_EQ(original, moved);

    // 移动赋值
    runtime_executor move_assigned;
    move_assigned = std::move(assigned);
    EXPECT_EQ(original, move_assigned);
}

TEST_F(RuntimeExecutorTest, post_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();
    std::atomic<bool> executed{false};

    exec.post([&]() {
        executed = true;
        promise.set_value(42);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_TRUE(executed.load());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(RuntimeExecutorTest, dispatch_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    exec.dispatch([&]() {
        promise.set_value(100);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 100);
}

TEST_F(RuntimeExecutorTest, defer_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    exec.defer([&]() {
        promise.set_value(200);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 200);
}

TEST_F(RuntimeExecutorTest, create_strand) {
    runtime_executor    exec;
    mc::runtime::strand strand(exec);

    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    boost::asio::post(strand, [&]() {
        promise.set_value(300);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 300);
}

// 测试 strand 的串行化保证
TEST_F(RuntimeExecutorTest, strand_serialization) {
    runtime_executor    exec;
    mc::runtime::strand strand(exec);

    std::atomic<int>  counter{0};
    std::atomic<bool> concurrent_detected{false};
    std::atomic<bool> in_progress{false};
    const int         num_tasks = 100;

    std::promise<void> done_promise;
    std::future<void>  done_future = done_promise.get_future();

    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::post(strand, [&]() {
            // 检测是否有并发执行
            if (in_progress.exchange(true)) {
                concurrent_detected = true;
            }

            // 模拟一些工作
            std::this_thread::sleep_for(std::chrono::microseconds(10));

            ++counter;

            in_progress = false;

            // 最后一个任务通知完成
            if (counter == num_tasks) {
                done_promise.set_value();
            }
        });
    }

    auto result = done_future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(counter.load(), num_tasks);
    EXPECT_FALSE(concurrent_detected.load()) << "strand 应该保证串行化执行";
}

// 测试 strand 可以跨 dispatcher 执行
TEST_F(RuntimeExecutorTest, strand_cross_dispatcher) {
    runtime_executor    exec;
    mc::runtime::strand strand(exec);

    // 记录任务执行的线程 ID
    std::vector<std::thread::id> thread_ids;
    std::mutex                   ids_mutex;
    const int                    num_tasks = 20;

    std::promise<void> done_promise;
    std::future<void>  done_future = done_promise.get_future();

    std::atomic<int> completed{0};

    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::post(strand, [&]() {
            {
                std::lock_guard lock(ids_mutex);
                thread_ids.push_back(std::this_thread::get_id());
            }

            // 模拟一些工作
            std::this_thread::sleep_for(std::chrono::microseconds(100));

            if (++completed == num_tasks) {
                done_promise.set_value();
            }
        });
    }

    auto result = done_future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(thread_ids.size(), static_cast<size_t>(num_tasks));
}

// 测试从 io pool 内部执行时选择当前 pool
TEST_F(RuntimeExecutorTest, select_current_pool) {
    runtime_executor exec;

    std::promise<bool>        same_pool_promise;
    std::future<bool>         same_pool_future = same_pool_promise.get_future();
    std::atomic<thread_pool*> first_pool{nullptr};

    // 第一个任务记录执行的 pool
    exec.post([&]() {
        auto* shard = thread_pool::get_current_shard();
        ASSERT_NE(shard, nullptr);
        first_pool = shard->pool;

        // 在任务内部再 post 一个任务
        runtime_executor inner_exec;
        inner_exec.post([&]() {
            auto* inner_shard = thread_pool::get_current_shard();
            ASSERT_NE(inner_shard, nullptr);
            // 应该在同一个 pool 上执行
            same_pool_promise.set_value(inner_shard->pool == first_pool.load());
        });
    });

    auto result = same_pool_future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_TRUE(same_pool_future.get()) << "内部任务应该在同一个 pool 上执行";
}

// 测试多个 strand 相互独立
TEST_F(RuntimeExecutorTest, multiple_strands_independent) {
    runtime_executor    exec;
    mc::runtime::strand strand1(exec);
    mc::runtime::strand strand2(exec);

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};
    const int        num_tasks = 50;

    std::promise<void> done1_promise, done2_promise;
    std::future<void>  done1_future = done1_promise.get_future();
    std::future<void>  done2_future = done2_promise.get_future();

    // 向两个 strand 分别提交任务
    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::post(strand1, [&]() {
            ++counter1;
            if (counter1 == num_tasks) {
                done1_promise.set_value();
            }
        });

        boost::asio::post(strand2, [&]() {
            ++counter2;
            if (counter2 == num_tasks) {
                done2_promise.set_value();
            }
        });
    }

    auto result1 = done1_future.wait_for(std::chrono::seconds(10));
    auto result2 = done2_future.wait_for(std::chrono::seconds(10));

    EXPECT_EQ(result1, std::future_status::ready);
    EXPECT_EQ(result2, std::future_status::ready);
    EXPECT_EQ(counter1.load(), num_tasks);
    EXPECT_EQ(counter2.load(), num_tasks);
}

TEST_F(RuntimeExecutorTest, context_create_strand) {
    auto strand = mc::get_runtime_context().create_strand();

    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    boost::asio::post(strand, [&]() {
        promise.set_value(999);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 999);
}

TEST_F(RuntimeExecutorTest, io_pool_strand) {
    auto strand = mc::get_runtime_context().create_io_strand();

    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    mc::runtime::thread_pool* run_pool = nullptr;
    boost::asio::post(strand, [&]() {
        auto* shard = mc::runtime::thread_pool::get_current_shard();
        if (shard) {
            run_pool = shard->pool;
        }
        run_pool = shard->pool;

        promise.set_value(111);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 111);
    EXPECT_EQ(run_pool, &mc::get_runtime_context().io());
}

TEST_F(RuntimeExecutorTest, work_pool_strand) {
    auto strand = mc::get_runtime_context().create_work_strand();

    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    mc::runtime::thread_pool* run_pool = nullptr;
    boost::asio::post(strand, [&]() {
        auto* shard = mc::runtime::thread_pool::get_current_shard();
        if (shard) {
            run_pool = shard->pool;
        }

        promise.set_value(222);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 222);
    EXPECT_EQ(run_pool, &mc::get_runtime_context().work());
}

// 测试 scoped_pool_binding 基本功能
TEST_F(RuntimeExecutorTest, scoped_pool_binding_basic) {
    // 初始状态应该没有绑定
    EXPECT_EQ(scoped_pool_binding::current_bound_pool(), nullptr);

    // 绑定到 io pool
    {
        scoped_pool_binding binding(&mc::get_runtime_context().io());
        EXPECT_EQ(scoped_pool_binding::current_bound_pool(), &mc::get_runtime_context().io());
    }

    // 出作用域后应该恢复
    EXPECT_EQ(scoped_pool_binding::current_bound_pool(), nullptr);

    // 绑定到 work pool
    {
        scoped_pool_binding binding(&mc::get_runtime_context().work());
        EXPECT_EQ(scoped_pool_binding::current_bound_pool(), &mc::get_runtime_context().work());
    }

    EXPECT_EQ(scoped_pool_binding::current_bound_pool(), nullptr);
}

// 测试 scoped_pool_binding 嵌套
TEST_F(RuntimeExecutorTest, scoped_pool_binding_nested) {
    EXPECT_EQ(scoped_pool_binding::current_bound_pool(), nullptr);

    {
        scoped_pool_binding outer(&mc::get_runtime_context().io());
        EXPECT_EQ(scoped_pool_binding::current_bound_pool(), &mc::get_runtime_context().io());

        {
            scoped_pool_binding inner(&mc::get_runtime_context().work());
            EXPECT_EQ(scoped_pool_binding::current_bound_pool(), &mc::get_runtime_context().work());
        }

        // 内层出作用域后恢复到外层绑定
        EXPECT_EQ(scoped_pool_binding::current_bound_pool(), &mc::get_runtime_context().io());
    }

    EXPECT_EQ(scoped_pool_binding::current_bound_pool(), nullptr);
}

// 测试 scoped_pool_binding 与 runtime_executor 的结合：确保 post 到绑定的 pool
TEST_F(RuntimeExecutorTest, scoped_pool_binding_with_executor) {
    runtime_executor exec;

    std::promise<thread_pool*> promise;
    std::future<thread_pool*>  future = promise.get_future();

    // 在绑定 work pool 的情况下 post
    {
        scoped_pool_binding binding(&mc::get_runtime_context().work());
        exec.post([&]() {
            auto* shard = thread_pool::get_current_shard();
            promise.set_value(shard ? shard->pool : nullptr);
        });
    }

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), &mc::get_runtime_context().work()) << "应该在绑定的 work pool 上执行";
}

// 测试 scoped_pool_binding 与 strand 的结合
TEST_F(RuntimeExecutorTest, scoped_pool_binding_with_strand) {
    runtime_executor exec;
    strand           test_strand(exec);

    std::promise<thread_pool*> promise;
    std::future<thread_pool*>  future = promise.get_future();

    // 在绑定 work pool 的情况下 post 到 strand
    {
        scoped_pool_binding binding(&mc::get_runtime_context().work());
        boost::asio::post(test_strand, [&]() {
            auto* shard = thread_pool::get_current_shard();
            promise.set_value(shard ? shard->pool : nullptr);
        });
    }

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), &mc::get_runtime_context().work()) << "strand 应该在绑定的 work pool 上执行";
}
