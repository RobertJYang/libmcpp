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
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {

class RuntimeBasicTest : public mc::test::TestWithRuntime {
};

} // namespace

// 测试基本用法示例
TEST_F(RuntimeBasicTest, BasicUsageExample) {
    // 获取全局运行时上下文
    auto& runtime = mc::get_runtime_context();

    std::atomic<bool> network_task_done{false};
    std::atomic<bool> hardware_task_done{false};

    // 网络相关任务 - 使用默认IO执行器
    mc::post([&network_task_done]() {
        // 模拟网络IO操作
        std::this_thread::sleep_for(50ms);
        network_task_done.store(true);
    });

    // 硬件相关任务 - 使用系统执行器
    mc::post([&hardware_task_done]() {
        // 模拟阻塞的硬件IO操作（如i2c读写）
        std::this_thread::sleep_for(100ms);
        hardware_task_done.store(true);
    }, mc::work_executor);

    // 等待任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (!network_task_done.load() || !hardware_task_done.load()) {
        std::this_thread::sleep_for(10ms);

        // 防止无限等待
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 5s) << "任务执行超时";
    }

    EXPECT_TRUE(network_task_done.load());
    EXPECT_TRUE(hardware_task_done.load());
}

// 测试执行器对象的使用
TEST_F(RuntimeBasicTest, ExecutorObjectUsage) {
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> task_count{0};

    // 获取不同类型的执行器
    auto io_executor      = mc::get_io_executor();
    auto system_executor  = mc::get_work_executor();
    auto default_executor = mc::get_default_executor();

    // 使用执行器对象投递任务
    boost::asio::post(io_executor, [&task_count]() {
        task_count.fetch_add(1);
    });

    boost::asio::post(system_executor, [&task_count]() {
        task_count.fetch_add(1);
    });

    boost::asio::post(default_executor, [&task_count]() {
        task_count.fetch_add(1);
    });

    // 等待任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (task_count.load() < 3) {
        std::this_thread::sleep_for(10ms);

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 2s) << "任务执行超时";
    }

    EXPECT_EQ(task_count.load(), 3);
}

// 测试标签的使用
TEST_F(RuntimeBasicTest, ExecutorTagUsage) {
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> io_task_count{0};
    std::atomic<int> system_task_count{0};

    // 使用标签投递任务
    mc::post([&io_task_count]() {
        io_task_count.fetch_add(1);
    }, mc::io_executor);

    mc::defer([&system_task_count]() {
        system_task_count.fetch_add(1);
    }, mc::work_executor);

    mc::dispatch([&io_task_count]() {
        io_task_count.fetch_add(1);
    }); // 默认使用IO执行器

    // 等待任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (io_task_count.load() < 2 || system_task_count.load() < 1) {
        std::this_thread::sleep_for(10ms);

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 2s) << "任务执行超时";
    }

    EXPECT_EQ(io_task_count.load(), 2);
    EXPECT_EQ(system_task_count.load(), 1);
}

// 测试嵌入式系统场景
TEST_F(RuntimeBasicTest, EmbeddedSystemScenario) {
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> sensor_readings{0};
    std::atomic<int> network_responses{0};

    // 模拟传感器读取（阻塞硬件操作）
    for (int i = 0; i < 3; ++i) {
        mc::post([&sensor_readings]() {
            // 模拟i2c读取传感器数据
            std::this_thread::sleep_for(10ms);
            sensor_readings.fetch_add(1);
        }, mc::work_executor);
    }

    // 模拟网络请求处理（事件驱动）
    for (int i = 0; i < 5; ++i) {
        mc::post([&network_responses]() {
            // 模拟HTTP响应处理
            std::this_thread::sleep_for(5ms);
            network_responses.fetch_add(1);
        }); // 默认使用IO执行器
    }

    // 等待所有任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (sensor_readings.load() < 3 || network_responses.load() < 5) {
        std::this_thread::sleep_for(20ms);

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 5s) << "任务执行超时";
    }

    EXPECT_EQ(sensor_readings.load(), 3);
    EXPECT_EQ(network_responses.load(), 5);
}