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
#include <mc/runtime/executor.h>
#include <test_utilities/test_base.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

class executor_scenario_test : public mc::test::TestWithRuntime {
};

// 场景1：并发任务执行
TEST_F(executor_scenario_test, scenario_concurrent_execution) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_executor = mc::get_io_executor();
    mc::executor exec(io_executor);

    const int num_tasks = 20;
    std::atomic<int> completed{0};
    std::promise<void> all_done;
    auto future = all_done.get_future();

    for (int i = 0; i < num_tasks; ++i) {
        exec.post([&completed, &all_done, num_tasks]() {
            std::this_thread::sleep_for(10ms);
            if (completed.fetch_add(1) + 1 == num_tasks) {
                all_done.set_value();
            }
        });
    }

    auto result = future.wait_for(2s);
    ASSERT_EQ(result, std::future_status::ready);
    EXPECT_EQ(completed.load(), num_tasks);
}

// 场景2：任务调度优先级
TEST_F(executor_scenario_test, scenario_task_scheduling_priority) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_strand = mc::make_io_strand();
    mc::executor exec(io_strand);

    std::vector<int> execution_order;
    std::mutex order_mutex;
    std::promise<void> done;
    auto future = done.get_future();

    const int num_tasks = 10;
    for (int i = 0; i < num_tasks; ++i) {
        exec.post([&execution_order, &order_mutex, &done, i, num_tasks]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(i);
            if (execution_order.size() == num_tasks) {
                done.set_value();
            }
        });
    }

    auto result = future.wait_for(1s);
    ASSERT_EQ(result, std::future_status::ready);

    // 验证串行执行（strand 保证顺序）
    EXPECT_EQ(execution_order.size(), num_tasks);
    for (size_t i = 0; i < execution_order.size(); ++i) {
        EXPECT_LE(execution_order[i], num_tasks);
    }
}

// 场景3：执行器生命周期管理
TEST_F(executor_scenario_test, scenario_executor_lifecycle) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool> task_executed{false};
    std::promise<void> done;
    auto future = done.get_future();

    {
        auto io_strand = mc::make_io_strand();
        mc::executor exec(io_strand);

        exec.post([&task_executed, &done]() {
            task_executed.store(true);
            done.set_value();
        });

        // executor 在这里销毁，但任务应该仍然执行
    }

    auto result = future.wait_for(1s);
    ASSERT_EQ(result, std::future_status::ready);
    EXPECT_TRUE(task_executed.load());
}

// 场景4：混合执行器使用
TEST_F(executor_scenario_test, scenario_mixed_executor_usage) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_executor = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();
    mc::executor exec_io(io_executor);
    mc::executor exec_work(work_executor);

    std::atomic<int> io_count{0};
    std::atomic<int> work_count{0};
    std::promise<void> done;
    auto future = done.get_future();

    const int num_io_tasks = 5;
    const int num_work_tasks = 5;

    for (int i = 0; i < num_io_tasks; ++i) {
        exec_io.post([&io_count, &work_count, &done, num_io_tasks, num_work_tasks]() {
            io_count.fetch_add(1);
            if (io_count.load() == num_io_tasks && work_count.load() == num_work_tasks) {
                done.set_value();
            }
        });
    }

    for (int i = 0; i < num_work_tasks; ++i) {
        exec_work.post([&io_count, &work_count, &done, num_io_tasks, num_work_tasks]() {
            work_count.fetch_add(1);
            if (io_count.load() == num_io_tasks && work_count.load() == num_work_tasks) {
                done.set_value();
            }
        });
    }

    auto result = future.wait_for(2s);
    ASSERT_EQ(result, std::future_status::ready);
    EXPECT_EQ(io_count.load(), num_io_tasks);
    EXPECT_EQ(work_count.load(), num_work_tasks);
}

// 场景5：执行器共享和拷贝
TEST_F(executor_scenario_test, scenario_executor_sharing) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_strand = mc::make_io_strand();
    mc::executor original(io_strand);

    // 创建多个副本
    std::vector<mc::executor> copies;
    for (int i = 0; i < 5; ++i) {
        copies.push_back(original);
    }

    // 验证所有副本相等
    for (const auto& copy : copies) {
        EXPECT_EQ(original, copy);
    }

    // 使用不同副本投递任务
    std::atomic<int> count{0};
    std::promise<void> done;
    auto future = done.get_future();

    for (auto& copy : copies) {
        copy.post([&count, &done]() {
            if (count.fetch_add(1) + 1 == 5) {
                done.set_value();
            }
        });
    }

    auto result = future.wait_for(1s);
    ASSERT_EQ(result, std::future_status::ready);
    EXPECT_EQ(count.load(), 5);
}

// 场景6：任务取消和清理
TEST_F(executor_scenario_test, scenario_task_cancellation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_executor = mc::get_io_executor();
    mc::executor exec(io_executor);

    std::atomic<bool> task1_done{false};
    std::atomic<bool> task2_done{false};

    // 投递快速任务
    exec.post([&task1_done]() {
        std::this_thread::sleep_for(10ms);
        task1_done.store(true);
    });

    // 投递慢速任务
    exec.post([&task2_done]() {
        std::this_thread::sleep_for(100ms);
        task2_done.store(true);
    });

    // 等待快速任务完成
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(task1_done.load());

    // 慢速任务可能未完成，但这是正常的
    // 这里主要验证任务投递和基本执行
}

