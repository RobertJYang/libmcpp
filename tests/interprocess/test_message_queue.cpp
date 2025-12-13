/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <vector>
#include <algorithm>
#include <mqueue.h>
#include <test_utilities/test_base.h>
#include <mc/interprocess/message_queue.h>

using namespace mc::interprocess;

class message_queue_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 清理可能存在的测试队列
        mq_unlink("/test_queue_1");
        mq_unlink("/test_queue_2");
        mq_unlink("/test_queue_large");
    }
    
    void TearDown() override {
        // 清理所有测试队列
        mq_unlink("/test_queue_1");
        mq_unlink("/test_queue_2");
        mq_unlink("/test_queue_large");
        mq_unlink("/test_queue_thread");
        mq_unlink("/test_queue_move");
    }
    
    // 辅助函数：生成测试数据
    std::vector<uint8_t> generate_test_data(size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 256);
        }
        return data;
    }
    
    // 辅助函数：验证数据
    bool verify_test_data(const std::vector<uint8_t>& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] != static_cast<uint8_t>(i % 256)) {
                return false;
            }
        }
        return true;
    }
};


// 测试 1: 构造函数和析构函数
TEST_F(message_queue_test, constructor_and_destructor) {
    // 测试 CREATE_ONLY 模式
    {
        queue_configuration config("/test_queue_1");
        config.mode = queue_mode::CREATE_ONLY;
        
        EXPECT_NO_THROW([&]() {
            message_queue mq(config);
            EXPECT_TRUE(mq.is_valid());
            EXPECT_EQ(mq.name(), "/test_queue_1");
            EXPECT_NE(mq.descriptor(), -1);
        }());
    }
    
    // 测试重复创建（应该失败）
    {
        queue_configuration config("/test_queue_1");
        config.mode = queue_mode::CREATE_ONLY;
        
        EXPECT_THROW({
            message_queue mq(config);
        }, std::system_error);
    }
    
    // 测试 OPEN_ONLY 模式（队列不存在应该失败）
    {
        queue_configuration config("/test_queue_nonexistent");
        config.mode = queue_mode::OPEN_ONLY;
        
        EXPECT_THROW({
            message_queue mq(config);
        }, std::system_error);
    }
    
    // 测试 CREATE_OR_OPEN 模式
    {
        queue_configuration config("/test_queue_1");
        config.mode = queue_mode::CREATE_OR_OPEN;
        
        EXPECT_NO_THROW([&]() {
            message_queue mq(config);
            EXPECT_TRUE(mq.is_valid());
        }());
    }
}

// 测试 2: 移动语义
TEST_F(message_queue_test, move_semantics) {
    queue_configuration config("/test_queue_move");
    
    // 测试移动构造函数
    {
        message_queue mq1(config);
        int original_descriptor = mq1.descriptor();
        
        message_queue mq2(std::move(mq1));
        
        EXPECT_FALSE(mq1.is_valid());
        EXPECT_EQ(mq1.descriptor(), -1);
        EXPECT_TRUE(mq2.is_valid());
        EXPECT_EQ(mq2.descriptor(), original_descriptor);
        EXPECT_EQ(mq2.name(), "/test_queue_move");
    }
    
    // 测试移动赋值运算符
    {
        message_queue mq1(config);
        message_queue mq2(queue_configuration("/test_queue_2"));
        
        int mq1_descriptor = mq1.descriptor();
        int mq2_descriptor = mq2.descriptor();
        
        mq2 = std::move(mq1);
        
        EXPECT_FALSE(mq1.is_valid());
        EXPECT_TRUE(mq2.is_valid());
        EXPECT_EQ(mq2.descriptor(), mq1_descriptor);
        
        // 原来的 mq2 应该被正确关闭
        EXPECT_NE(mq2_descriptor, -1);
    }
}

// 测试 3: 基本消息发送和接收
TEST_F(message_queue_test, basic_send_recv) {
    queue_configuration config("/test_queue_1");
    message_queue mq(config);
    
    // 测试发送和接收空消息
    {
        std::vector<uint8_t> empty_data;
        EXPECT_TRUE(mq.send(empty_data));
        
        auto received = mq.receive();
        EXPECT_TRUE(received.empty());
    }
    
    // 测试发送和接收小消息
    {
        std::string message = "Hello, Message Queue!";
        std::vector<uint8_t> data(message.begin(), message.end());
        
        EXPECT_TRUE(mq.send(data));
        
        auto received = mq.receive();
        std::string received_str(received.begin(), received.end());
        
        EXPECT_EQ(received_str, message);
    }
    
    // 测试发送和接收二进制数据
    {
        auto test_data = generate_test_data(100);
        EXPECT_TRUE(mq.send(test_data));
        
        auto received = mq.receive();
        EXPECT_EQ(received.size(), 100);
        EXPECT_TRUE(verify_test_data(received));
    }
}

// 测试 4: 消息优先级
TEST_F(message_queue_test, message_with_priority) {
    queue_configuration config("/test_queue_1");
    config.max_messages = 10;
    message_queue mq(config);
    
    // 清空队列
    mq.clear();
    
    // 发送不同优先级的消息
    std::vector<std::pair<unsigned int, std::string>> messages = {
        {1, "Low priority message"},
        {5, "Medium priority message"},
        {10, "High priority message"},
        {3, "Another medium priority"},
        {8, "Higher priority"}
    };
    
    // 以随机顺序发送
    for (const auto& [priority, message] : messages) {
        std::vector<uint8_t> data(message.begin(), message.end());
        EXPECT_TRUE(mq.send(data, priority));
    }
    
    // 接收并验证按优先级顺序
    std::vector<std::string> received_messages;
    while (true) {
        unsigned int priority;
        auto data = mq.receive_with_priority(priority, 100);
        if (data.empty()) break;
        
        std::string msg(data.begin(), data.end());
        received_messages.push_back(msg);
    }
    
    // 验证接收顺序（高优先级先收到）
    EXPECT_EQ(received_messages.size(), messages.size());
    EXPECT_EQ(received_messages[0], "High priority message");  // 优先级 10
    EXPECT_EQ(received_messages[1], "Higher priority");        // 优先级 8
}

// 测试 5: 超时功能
TEST_F(message_queue_test, recv_with_timeout) {
    queue_configuration config("/test_queue_1");
    message_queue mq(config);
    
    // 清空队列
    mq.clear();
    
    // 测试非阻塞接收（应该立即返回空）
    auto start = std::chrono::steady_clock::now();
    auto result = mq.receive(0);  // 非阻塞
    auto end = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(result.empty());
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 10);  // 应该很快返回
    
    // 测试带超时的接收（应该超时返回）
    start = std::chrono::steady_clock::now();
    result = mq.receive(100);  // 100ms 超时
    end = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(result.empty());
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 100);  // 应该至少等待100ms
    EXPECT_LT(duration.count(), 150);  // 但不会超过太多
    
    // 测试带超时的发送（队列满时）
    {
        queue_configuration small_config("/test_queue_small");
        small_config.max_messages = 1;
        small_config.max_message_size = 10;
        message_queue small_mq(small_config);
        
        // 填满队列
        std::vector<uint8_t> data1 = {1, 2, 3};
        EXPECT_TRUE(small_mq.send(data1));
        
        // 尝试发送第二个消息（应该超时）
        std::vector<uint8_t> data2 = {4, 5, 6};
        start = std::chrono::steady_clock::now();
        bool sent = small_mq.send(data2, 0, 50);  // 50ms 超时
        end = std::chrono::steady_clock::now();
        
        EXPECT_FALSE(sent);  // 应该发送失败
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        EXPECT_GE(duration.count(), 50);
    }
}

// 测试 6: 队列属性
TEST_F(message_queue_test, queue_attributes) {
    queue_configuration config("/test_queue_1");
    config.max_messages = 5;
    config.max_message_size = 1024;
    message_queue mq(config);
    
    // 获取属性
    auto attr = mq.get_attributes();
    EXPECT_EQ(attr.max_messages, 5);
    EXPECT_EQ(attr.max_message_size, 1024);
    EXPECT_EQ(attr.current_messages, 0);
    
    // 发送一些消息
    for (int i = 0; i < 3; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        mq.send(data);
    }
    
    // 验证当前消息数
    attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 3);
    
    // 接收一个消息
    mq.receive();
    attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 2);
    
    // 清空队列
    mq.clear();
    attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 0);
}

// 测试 7: 大消息处理
TEST_F(message_queue_test, large_message_handling) {
    queue_configuration config("/test_queue_large");
    config.max_message_size = 8192;  // 8KB
    message_queue mq(config);
    
    // 测试发送最大大小的消息
    auto large_data = generate_test_data(8192);
    EXPECT_NO_THROW([&]() {
        bool sent = mq.send(large_data);
        EXPECT_TRUE(sent);
    }());
    
    // 测试接收大消息
    auto received = mq.receive();
    EXPECT_EQ(received.size(), 8192);
    EXPECT_TRUE(verify_test_data(received));
    
    // 测试发送超过最大大小的消息（应该抛出异常）
    auto too_large_data = generate_test_data(8193);
    EXPECT_THROW({
        mq.send(too_large_data);
    }, std::runtime_error);
}

// 测试 8: 多线程安全
TEST_F(message_queue_test, thread_safety) {
    queue_configuration config("/test_queue_thread");
    config.max_messages = 100;
    message_queue mq(config);
    
    const int NUM_MESSAGES = 1000;
    const int NUM_THREADS = 4;
    
    std::atomic<int> messages_sent{0};
    std::atomic<int> messages_received{0};
    std::vector<std::thread> threads;
    
    // 创建发送线程
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&mq, &messages_sent, t]() {
            for (int i = 0; i < NUM_MESSAGES / NUM_THREADS; ++i) {
                std::string message = 
                    "Thread " + std::to_string(t) + 
                    " Message " + std::to_string(i);
                std::vector<uint8_t> data(message.begin(), message.end());
                
                if (mq.send(data, t % 3)) {
                    ++messages_sent;
                }
            }
        });
    }
    
    // 创建接收线程
    threads.emplace_back([&mq, &messages_received]() {
        while (messages_received < NUM_MESSAGES) {
            auto data = mq.receive(10);  // 10ms 超时
            if (!data.empty()) {
                ++messages_received;
            }
        }
    });
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有消息都被处理
    EXPECT_EQ(messages_sent, NUM_MESSAGES);
    EXPECT_EQ(messages_received, NUM_MESSAGES);
    
    // 验证队列为空
    auto attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 0);
}

// 测试 9: 边界条件
TEST_F(message_queue_test, edge_conditions) {
    // 测试无效队列名
    {
        queue_configuration config("invalid_name");  // 没有以 '/' 开头
        EXPECT_THROW({
            message_queue mq(config);
        }, std::invalid_argument);
    }
    
    // 测试零大小配置
    {
        queue_configuration config("/test_edge");
        config.max_messages = 0;
        config.max_message_size = 0;
        
        EXPECT_THROW({
            message_queue mq(config);
        }, std::invalid_argument);
    }
    
    // 测试已经关闭的队列
    {
        queue_configuration config("/test_edge");
        message_queue* mq = new message_queue(config);
        int descriptor = mq->descriptor();
        delete mq;  // 析构函数会关闭队列
        
        // 尝试操作已关闭的队列
        queue_configuration open_config("/test_edge");
        open_config.mode = queue_mode::OPEN_ONLY;
        message_queue mq2(open_config);
        
        // 应该能正常操作
        std::vector<uint8_t> data = {1, 2, 3};
        EXPECT_TRUE(mq2.send(data));
        EXPECT_FALSE(mq2.receive(0).empty());
    }
}

// 测试 10: 异常处理
TEST_F(message_queue_test, exception_handling) {
    // 测试发送到不存在的队列
    {
        // 先创建队列
        queue_configuration config1("/test_exception");
        message_queue mq1(config1);
        
        // 关闭队列（模拟队列不存在）
        mq_unlink("/test_exception");
        
        // 尝试发送应该失败
        std::vector<uint8_t> data = {1, 2, 3};
        EXPECT_THROW({
            mq1.send(data);
        }, std::system_error);
    }
    
    // 测试从无效描述符接收
    {
        message_queue mq(queue_configuration("/test_exception2"));
        
        // 手动关闭描述符（模拟错误情况）
        mq_unlink("/test_exception2");
        
        EXPECT_THROW({
            mq.receive();
        }, std::system_error);
    }
}

// 测试 11: 性能测试
TEST_F(message_queue_test, performance_testing) {
    queue_configuration config("/test_performance");
    config.max_messages = 1000;
    message_queue mq(config);
    
    const int NUM_ITERATIONS = 1000;
    const size_t MESSAGE_SIZE = 1024;  // 1KB
    
    // 生成测试数据
    auto test_data = generate_test_data(MESSAGE_SIZE);
    
    // 测试发送性能
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        EXPECT_TRUE(mq.send(test_data));
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto send_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double send_time_per_msg = static_cast<double>(send_duration.count()) / NUM_ITERATIONS;
    
    std::cout << "Average send time: " << send_time_per_msg << " microseconds" << std::endl;
    
    // 测试接收性能
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        auto received = mq.receive();
        EXPECT_EQ(received.size(), MESSAGE_SIZE);
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto receive_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double receive_time_per_msg = static_cast<double>(receive_duration.count()) / NUM_ITERATIONS;
    
    std::cout << "Average receive time: " << receive_time_per_msg << " microseconds" << std::endl;
    
    // 验证队列为空
    auto attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 0);
}

// 测试 12: 跨进程通信模拟（使用线程模拟）
TEST_F(message_queue_test, cross_process_communication) {
    queue_configuration config("/test_cross_process");
    message_queue mq(config);
    
    // 模拟进程A
    auto process_a = [&mq]() {
        for (int i = 0; i < 10; ++i) {
            std::string message = "Process A: " + std::to_string(i);
            std::vector<uint8_t> data(message.begin(), message.end());
            mq.send(data);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 发送结束信号
        std::vector<uint8_t> end_data = {'E', 'N', 'D'};
        mq.send(end_data);
    };
    
    // 模拟进程B
    auto process_b = [&mq]() -> int {
        int count = 0;
        while (true) {
            auto data = mq.receive(100);  // 100ms 超时
            if (!data.empty()) {
                if (data.size() == 3 && 
                    data[0] == 'E' && 
                    data[1] == 'N' && 
                    data[2] == 'D') {
                    break;  // 收到结束信号
                }
                ++count;
            }
        }
        return count;
    };
    
    // 启动"进程B"作为异步任务
    auto future = std::async(std::launch::async, process_b);
    
    // 运行"进程A"
    std::thread thread_a(process_a);
    thread_a.join();
    
    // 获取结果
    int received_count = future.get();
    
    EXPECT_EQ(received_count, 10);
}

// 测试 13: 权限测试
TEST_F(message_queue_test, permission_testing) {
    queue_configuration config("/test_permission");
    config.permissions.owner_read = 1;
    config.permissions.owner_write = 1;
    config.permissions.group_read = 1;
    config.permissions.group_write = 0;
    config.permissions.others_read = 0;
    config.permissions.others_write = 0;
    
    // 创建带权限的队列
    EXPECT_NO_THROW([&]() {
        message_queue mq(config);
    }());
}

// 测试 14: 清空功能
TEST_F(message_queue_test, clear_testing) {
    queue_configuration config("/test_clear");
    message_queue mq(config);
    
    // 填充队列
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        mq.send(data);
    }
    
    // 验证队列非空
    auto attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 10);
    
    // 清空队列
    mq.clear();
    
    // 验证队列为空
    attr = mq.get_attributes();
    EXPECT_EQ(attr.current_messages, 0);
    
    // 测试非阻塞接收应该立即返回空
    auto result = mq.receive(0);
    EXPECT_TRUE(result.empty());
}

// 测试 15: 资源清理
TEST_F(message_queue_test, resource_cleanup) {
    // 测试析构函数自动清理
    {
        queue_configuration config("/test_auto_cleanup");
        {
            message_queue mq(config);
            // 发送一些消息
            for (int i = 0; i < 5; ++i) {
                std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
                mq.send(data);
            }
        }  // mq 析构，应该自动清理
        
        // 尝试重新创建（应该成功，因为已清理）
        queue_configuration new_config("/test_auto_cleanup");
        new_config.mode = queue_mode::CREATE_ONLY;
        
        EXPECT_NO_THROW([&]() {
            message_queue mq(new_config);
            EXPECT_TRUE(mq.is_valid());
        }());
    }
}
