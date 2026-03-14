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

#include "test_future_helpers.h"

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

namespace {

class RuntimeBasicTest : public mc::test::TestWithRuntime {};

} // namespace

// 测试基本用法示例
TEST_F(RuntimeBasicTest, BasicUsageExample)
{
    // 获取全局运行时上下文
    auto& runtime = mc::get_runtime_context();

    std::atomic<bool>              network_task_done{false};
    std::atomic<bool>              hardware_task_done{false};
    mc::test::runtime::future_flag network_ready;
    mc::test::runtime::future_flag hardware_ready;

    // 网络相关任务 - 使用默认IO执行器
    mc::post([&network_task_done, network_ready]() mutable {
        network_task_done.store(true);
        network_ready.set();
    });

    // 硬件相关任务 - 使用系统执行器
    mc::post([&hardware_task_done, hardware_ready]() mutable {
        hardware_task_done.store(true);
        hardware_ready.set();
    }, mc::work_executor);

    EXPECT_TRUE(network_ready.wait_for(3s));
    EXPECT_TRUE(hardware_ready.wait_for(3s));
    EXPECT_TRUE(network_task_done.load());
    EXPECT_TRUE(hardware_task_done.load());
}

// 测试执行器对象的使用
TEST_F(RuntimeBasicTest, ExecutorObjectUsage)
{
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> task_count{0};

    mc::test::runtime::countdown_future tasks_done(3);
    auto                                io_executor      = mc::get_io_executor();
    auto                                system_executor  = mc::get_work_executor();
    auto                                default_executor = mc::get_default_executor();

    boost::asio::post(io_executor, [&, tasks_done]() mutable {
        task_count.fetch_add(1);
        tasks_done.arrive();
    });

    boost::asio::post(system_executor, [&, tasks_done]() mutable {
        task_count.fetch_add(1);
        tasks_done.arrive();
    });

    boost::asio::post(default_executor, [&, tasks_done]() mutable {
        task_count.fetch_add(1);
        tasks_done.arrive();
    });

    EXPECT_TRUE(tasks_done.wait_for(3s));
    EXPECT_EQ(task_count.load(), 3);
}

// 测试标签的使用
TEST_F(RuntimeBasicTest, ExecutorTagUsage)
{
    auto& runtime = mc::get_runtime_context();

    std::atomic<int>               io_task_count{0};
    std::atomic<int>               system_task_count{0};
    mc::test::runtime::future_flag io_done;
    mc::test::runtime::future_flag system_done;

    auto notify_io = [&io_task_count, io_done]() mutable {
        auto current = io_task_count.fetch_add(1) + 1;
        if (current >= 2) {
            io_done.set();
        }
    };

    auto notify_system = [&system_task_count, system_done]() mutable {
        auto current = system_task_count.fetch_add(1) + 1;
        if (current >= 1) {
            system_done.set();
        }
    };

    mc::post([notify_io]() mutable {
        notify_io();
    }, mc::io_executor);

    mc::dispatch([notify_io]() mutable {
        notify_io();
    });

    mc::defer([notify_system]() mutable {
        notify_system();
    }, mc::work_executor);

    EXPECT_TRUE(io_done.wait_for(3s));
    EXPECT_TRUE(system_done.wait_for(3s));
    EXPECT_EQ(io_task_count.load(), 2);
    EXPECT_EQ(system_task_count.load(), 1);
}

// 测试嵌入式系统场景
TEST_F(RuntimeBasicTest, EmbeddedSystemScenario)
{
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> sensor_readings{0};
    std::atomic<int> network_responses{0};

    mc::test::runtime::future_flag sensors_done;
    mc::test::runtime::future_flag network_done;

    for (int i = 0; i < 3; ++i) {
        mc::post([&sensor_readings, sensors_done]() mutable {
            sensor_readings.fetch_add(1);
            if (sensor_readings.load() >= 3) {
                sensors_done.set();
            }
        }, mc::work_executor);
    }

    for (int i = 0; i < 5; ++i) {
        mc::post([&network_responses, network_done]() mutable {
            network_responses.fetch_add(1);
            if (network_responses.load() >= 5) {
                network_done.set();
            }
        }); // 默认使用IO执行器
    }

    EXPECT_TRUE(sensors_done.wait_for(3s));
    EXPECT_TRUE(network_done.wait_for(3s));
    EXPECT_EQ(sensor_readings.load(), 3);
    EXPECT_EQ(network_responses.load(), 5);
}