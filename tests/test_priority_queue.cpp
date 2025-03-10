#include "appbase/application.h"
#include "appbase/priority_queue.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <vector>

using namespace appbase;

// 测试优先级队列执行器
TEST(PriorityQueueTest, ExecutorBasic) {
    boost::asio::io_context io;
    priority_queue_executor executor(io);

    // 测试执行器相等性
    priority_queue_executor executor2(io);
    EXPECT_EQ(executor, executor2);

    boost::asio::io_context io2;
    priority_queue_executor executor3(io2);
    EXPECT_NE(executor, executor3);
}

// 测试任务优先级
TEST(PriorityQueueTest, TaskPriority) {
    boost::asio::io_context io;
    priority_queue_executor executor(io);

    std::vector<int> execution_order;
    std::mutex mutex;

    // 提交低优先级任务
    executor.execute(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(3); // 应该最后执行
        },
        priority::low);

    // 提交中优先级任务
    executor.execute(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(2); // 应该第二个执行
        },
        priority::normal);

    // 提交高优先级任务
    executor.execute(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(1); // 应该第一个执行
        },
        priority::high);

    // 运行IO上下文，执行所有任务
    io.run();

    // 验证执行顺序
    ASSERT_EQ(3, execution_order.size());
    EXPECT_EQ(1, execution_order[0]); // 高优先级任务应该第一个执行
    EXPECT_EQ(2, execution_order[1]); // 中优先级任务应该第二个执行
    EXPECT_EQ(3, execution_order[2]); // 低优先级任务应该最后执行
}

// 测试应用程序中的优先级任务队列
TEST(PriorityQueueTest, ApplicationIntegration) {
    application& app = application::instance();

    std::vector<int> execution_order;
    std::mutex mutex;

    // 提交低优先级任务
    app.post(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(3); // 应该最后执行
        },
        priority::low);

    // 提交中优先级任务
    app.post(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(2); // 应该第二个执行
        },
        priority::normal);

    // 提交高优先级任务
    app.post(
        [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(1); // 应该第一个执行
        },
        priority::high);

    // 直接执行所有任务
    app.get_priority_executor().execute_all();

    // 验证执行顺序
    ASSERT_EQ(3, execution_order.size()) << "应该有3个任务被执行";
    EXPECT_EQ(1, execution_order[0]) << "高优先级任务应该第一个执行";
    EXPECT_EQ(2, execution_order[1]) << "中优先级任务应该第二个执行";
    EXPECT_EQ(3, execution_order[2]) << "低优先级任务应该最后执行";
}

// 测试多线程并发提交任务时的优先级顺序
TEST(PriorityQueueTest, MultithreadedPriorityOrder) {
    boost::asio::io_context io;
    auto work_guard = boost::asio::make_work_guard(io);
    priority_queue_executor executor(io);

    std::vector<int> execution_order;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<int> completed_tasks{0};

    // 配置测试参数
    const int tasks_per_thread = 10;
    const int thread_count = 2;
    const int total_tasks = tasks_per_thread * thread_count;

    // 提交任务
    for (int t = 0; t < thread_count; ++t) {
        for (int i = 0; i < tasks_per_thread; ++i) {
            // 确定任务优先级 (高、中、低循环)
            int p = (t * tasks_per_thread + i) % 3;
            int priority_value;

            switch (p) {
            case 0:
                priority_value = priority::high;
                break;
            case 1:
                priority_value = priority::normal;
                break;
            case 2:
                priority_value = priority::low;
                break;
            }

            // 提交任务
            executor.execute(
                [&mutex, &execution_order, &completed_tasks, &cv, priority_value]() {
                    // 记录执行顺序
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        execution_order.push_back(priority_value);
                    }

                    // 增加已完成任务计数并通知
                    if (++completed_tasks == total_tasks) {
                        cv.notify_one();
                    }
                },
                priority_value);
        }
    }

    // 启动IO线程
    std::thread io_thread([&io]() {
        io.run();
    });

    // 等待所有任务完成
    {
        std::unique_lock<std::mutex> lock(mutex);
        bool completed = cv.wait_for(lock, std::chrono::seconds(5), [&completed_tasks]() {
            return completed_tasks == total_tasks;
        });

        ASSERT_TRUE(completed) << "任务执行超时";
    }

    // 停止IO上下文并等待线程结束
    io.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }

    // 验证执行顺序
    ASSERT_EQ(total_tasks, execution_order.size());

    // 计算各优先级任务的数量
    int high_count = std::count(execution_order.begin(), execution_order.end(), priority::high);
    int normal_count = std::count(execution_order.begin(), execution_order.end(), priority::normal);
    int low_count = std::count(execution_order.begin(), execution_order.end(), priority::low);

    // 验证总任务数量正确
    EXPECT_EQ(total_tasks, high_count + normal_count + low_count);

    // 验证前1/3的任务中高优先级任务的比例高于后1/3的任务
    int third_size = total_tasks / 3;
    int high_in_first_third =
        std::count(execution_order.begin(), execution_order.begin() + third_size, priority::high);

    int high_in_last_third =
        std::count(execution_order.begin() + 2 * third_size, execution_order.end(), priority::high);

    EXPECT_GT(high_in_first_third, high_in_last_third)
        << "高优先级任务应该优先执行，但在结果中发现高优先级任务未能集中在执行序列的前部分";
}

// 测试优先级队列的状态控制（停止、启动）
TEST(PriorityQueueTest, StateControl) {
    // 创建IO上下文
    boost::asio::io_context io_context;
    auto work_guard = boost::asio::make_work_guard(io_context);

    std::mutex mutex;
    std::condition_variable cv;

    // 创建优先级队列，初始状态为停止
    appbase::priority_queue_executor<> pri_queue(io_context, false); // 初始状态为停止

    // 任务计数器
    std::atomic<int> task_count{0};

    // 添加一个简单的任务
    pri_queue.add(appbase::priority::normal, [&task_count, &cv]() {
        task_count++;
        cv.notify_one();
    });

    // 验证任务未开始执行
    EXPECT_EQ(task_count, 0) << "队列处于停止状态时不应执行任务";

    // 启动IO上下文
    std::thread io_thread([&io_context]() {
        io_context.run();
    });

    pri_queue.start();

    // 等待所有任务完成
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock);

    EXPECT_EQ(task_count, 1) << "任务应该已执行";

    io_context.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }
}