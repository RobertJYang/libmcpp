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
        // 预清理：确保所有可能的测试队列都被删除
        cleanup_all_queues();
        
        // 公共测试配置
        default_config.name = "/test_queue";
        default_config.mode = queue_mode::CREATE_OR_OPEN;
        default_config.max_messages = 10;
        default_config.max_message_size = 1024;
    }

    void TearDown() override {
        // 清理测试队列
        cleanup_all_queues();
    }
    
    // 清理所有测试使用的队列
    void cleanup_all_queues() {
        mq_unlink("/test_queue");
        mq_unlink("/test_queue2");
        mq_unlink("/test_queue3");
        mq_unlink("/another_queue");
    }

    queue_configuration default_config;
    
    // 辅助函数：发送测试消息
    bool send_test_message(message_queue& mq, 
                          const std::string& msg, 
                          unsigned int priority = 0) {
        std::vector<uint8_t> data(msg.begin(), msg.end());
        return mq.send(data, priority);
    }
    
    // 辅助函数：接收并验证消息
    std::string receive_and_convert(message_queue& mq) {
        auto data = mq.receive();
        return std::string(data.begin(), data.end());
    }
};

TEST_F(message_queue_test, DefaultConstructor) {
    message_queue mq;
    EXPECT_FALSE(mq.is_open());
    // 默认构造的对象应该可以后续使用 open() 方法
    EXPECT_NO_THROW(mq.open("/test_queue", queue_mode::CREATE_OR_OPEN));
    EXPECT_TRUE(mq.is_open());
    mq.close();
}

TEST_F(message_queue_test, ParameterizedConstructor) {
    EXPECT_NO_THROW({
        message_queue mq(default_config);
        // 构造时应该自动打开队列
        EXPECT_TRUE(mq.is_open());
    });
}

TEST_F(message_queue_test, MoveConstructor) {
    message_queue mq1(default_config);
    EXPECT_TRUE(mq1.is_open());
    
    // 移动构造
    message_queue mq2(std::move(mq1));
    EXPECT_TRUE(mq2.is_open());
    EXPECT_FALSE(mq1.is_open());  // mq1 应该被移动到无效状态
    
    // 测试移动后仍然可以使用
    EXPECT_TRUE(send_test_message(mq2, "test after move"));
    auto received = receive_and_convert(mq2);
    EXPECT_EQ("test after move", received);
}

TEST_F(message_queue_test, DestructorClosesQueue) {
    bool was_open = false;
    
    {
        message_queue mq(default_config);
        was_open = mq.is_open();
        // 退出作用域，mq 应该自动关闭
    }
    
    EXPECT_TRUE(was_open);
    
    // 队列应该已经被关闭和删除（如果是创建者）
    message_queue mq2;
    EXPECT_THROW(mq2.open("/test_queue", queue_mode::OPEN_ONLY), 
                 std::runtime_error);
}

TEST_F(message_queue_test, OwnerFlag) {
    // 测试 owner_ 标志是否正确工作
    // 创建者应该删除队列，非创建者不应该
    {
        message_queue creator;
        creator.open("/test_queue", queue_mode::CREATE_ONLY);
        // creator 应该是 owner
        
        {
            message_queue opener;
            opener.open("/test_queue", queue_mode::OPEN_ONLY);
            // opener 应该不是 owner
        }
        // opener 析构不应该删除队列
        
        EXPECT_NO_THROW({
            message_queue another_opener;
            another_opener.open("/test_queue", queue_mode::OPEN_ONLY);
        });
    }
    // creator 析构应该删除队列
    
    // 队列应该不存在了
    message_queue mq;
    EXPECT_THROW(mq.open("/test_queue", queue_mode::OPEN_ONLY), 
                 std::runtime_error);
}

TEST_F(message_queue_test, MoveAssignment) {
    message_queue mq1(default_config);
    message_queue mq2;
    
    mq2 = std::move(mq1);
    EXPECT_TRUE(mq2.is_open());
    EXPECT_FALSE(mq1.is_open());
}

TEST_F(message_queue_test, CopyConstructorDeleted) {
    message_queue mq1(default_config);
    // 应该无法拷贝构造
    // message_queue mq2(mq1);  // 这行应该编译失败
    // 验证编译时检查，我们使用 SFINAE 或静态断言
    EXPECT_TRUE(std::is_move_constructible<message_queue>::value);
    EXPECT_FALSE(std::is_copy_constructible<message_queue>::value);
}

TEST_F(message_queue_test, OpenClose) {
    message_queue mq;
    EXPECT_FALSE(mq.is_open());
    
    // 正常打开
    EXPECT_NO_THROW(mq.open("/test_queue", queue_mode::CREATE_ONLY));
    EXPECT_TRUE(mq.is_open());
    
    // 在已打开的对象上再次调用 open 会先关闭再打开新队列
    // 这是允许的行为
    EXPECT_NO_THROW(mq.open("/another_queue", queue_mode::CREATE_OR_OPEN));
    EXPECT_TRUE(mq.is_open());
    
    // 正常关闭
    EXPECT_NO_THROW(mq.close());
    EXPECT_FALSE(mq.is_open());
    
    // 重复关闭应该没问题
    EXPECT_NO_THROW(mq.close());
}

TEST_F(message_queue_test, OpenWithDifferentModes) {
    // CREATE 模式
    {
        message_queue mq;
        mq.open("/test_queue", queue_mode::CREATE_ONLY);
        EXPECT_TRUE(mq.is_open());
    }
    
    // OPEN_ONLY 模式
    {
        message_queue mq;
        // 队列不存在时应该失败
        EXPECT_THROW(mq.open("/nonexistent", queue_mode::OPEN_ONLY), 
                     std::runtime_error);
        
        // 先创建队列（保持打开状态，这样队列不会被删除）
        message_queue creator;
        creator.open("/test_queue2", queue_mode::CREATE_OR_OPEN);
        
        // 以 OPEN_ONLY 模式打开（队列已存在）
        EXPECT_NO_THROW(mq.open("/test_queue2", queue_mode::OPEN_ONLY));
        EXPECT_TRUE(mq.is_open());
        
        // 两个都关闭（creator 关闭时不会删除队列，因为还有其他引用）
        mq.close();
        creator.close();
    }
    
    // CREATE_OR_OPEN 模式
    {
        message_queue mq;
        EXPECT_NO_THROW(mq.open("/test_queue3", queue_mode::CREATE_OR_OPEN));
        EXPECT_TRUE(mq.is_open());
    }
}

TEST_F(message_queue_test, SendAndReceive) {
    message_queue mq(default_config);
    
    // 发送简单消息
    std::string test_msg = "Hello, Message Queue!";
    EXPECT_TRUE(send_test_message(mq, test_msg));
    
    // 接收消息
    auto received = receive_and_convert(mq);
    EXPECT_EQ(test_msg, received);
}

TEST_F(message_queue_test, SendReceiveMultipleMessages) {
    message_queue mq(default_config);
    
    const int num_messages = 5;
    std::vector<std::string> messages = {
        "Message 1",
        "Message 2", 
        "Message 3",
        "Message 4",
        "Message 5"
    };
    
    // 发送所有消息
    for (const auto& msg : messages) {
        EXPECT_TRUE(send_test_message(mq, msg));
    }
    
    // 接收所有消息（应该按优先级和发送顺序）
    for (int i = 0; i < num_messages; ++i) {
        auto received = receive_and_convert(mq);
        EXPECT_EQ(messages[i], received);
    }
}

TEST_F(message_queue_test, PriorityQueue) {
    message_queue mq(default_config);
    
    // 发送不同优先级的消息（优先级数字越大，优先级越高）
    EXPECT_TRUE(send_test_message(mq, "Low priority", 1));
    EXPECT_TRUE(send_test_message(mq, "Medium priority", 5));
    EXPECT_TRUE(send_test_message(mq, "High priority", 10));
    
    // 应该按优先级接收（高优先级先出）
    auto msg1 = receive_and_convert(mq);
    EXPECT_EQ("High priority", msg1);
    
    auto msg2 = receive_and_convert(mq);
    EXPECT_EQ("Medium priority", msg2);
    
    auto msg3 = receive_and_convert(mq);
    EXPECT_EQ("Low priority", msg3);
}

TEST_F(message_queue_test, ReceiveWithPriority) {
    message_queue mq(default_config);
    
    unsigned int test_priority = 7;
    EXPECT_TRUE(send_test_message(mq, "Priority test", test_priority));
    
    unsigned int received_priority = 0;
    auto data = mq.receive_with_priority(received_priority);
    
    std::string received_msg(data.begin(), data.end());
    EXPECT_EQ("Priority test", received_msg);
    EXPECT_EQ(test_priority, received_priority);
}

TEST_F(message_queue_test, SendToClosedQueue) {
    message_queue mq;
    std::vector<uint8_t> data = {1, 2, 3, 4};
    
    // 向未打开的队列发送消息应该返回 false
    EXPECT_FALSE(mq.send(data));
}

TEST_F(message_queue_test, ReceiveFromEmptyQueueNonBlocking) {
    message_queue mq(default_config);
    
    // 非阻塞接收空队列应该立即返回
    auto data = mq.receive(0);  // 0 表示非阻塞
    EXPECT_TRUE(data.empty());
}

TEST_F(message_queue_test, MessageSizeLimits) {
    message_queue mq;
    mq.open("/test_queue", queue_mode::CREATE_ONLY, 10, 100);  // 最大消息大小100字节
    
    // 发送恰好100字节的消息
    std::vector<uint8_t> max_size_msg(100, 'A');
    EXPECT_TRUE(mq.send(max_size_msg));
    
    // 发送超过限制的消息应该抛出异常
    std::vector<uint8_t> oversized_msg(101, 'B');
    EXPECT_THROW(mq.send(oversized_msg), std::runtime_error);
}

TEST_F(message_queue_test, QueueCapacityLimits) {
    int max_msgs = 3;
    message_queue mq;
    mq.open("/test_queue", queue_mode::CREATE_ONLY, max_msgs, 10);
    
    // 填充队列
    for (int i = 0; i < max_msgs; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        EXPECT_TRUE(mq.send(data));
    }
    
    // 队列已满，非阻塞发送应该失败
    std::vector<uint8_t> extra_data = {99};
    EXPECT_FALSE(mq.send(extra_data, 0, 0));  // 非阻塞模式
}

TEST_F(message_queue_test, SendTimeout) {
    message_queue mq;
    mq.open("/test_queue", queue_mode::CREATE_ONLY, 1, 10);  // 容量只有1
    
    // 填充队列
    std::vector<uint8_t> data1 = {1};
    EXPECT_TRUE(mq.send(data1));
    
    // 再次发送应该阻塞，但设置超时
    std::vector<uint8_t> data2 = {2};
    
    auto start = std::chrono::steady_clock::now();
    bool result = mq.send(data2, 0, 100);  // 100ms 超时
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(result);  // 应该超时失败
    EXPECT_GE(duration.count(), 100);  // 至少等待了100ms
    EXPECT_LT(duration.count(), 150);  // 但不超过150ms（考虑误差）
}

TEST_F(message_queue_test, ReceiveTimeout) {
    message_queue mq(default_config);
    
    auto start = std::chrono::steady_clock::now();
    auto data = mq.receive(50);  // 50ms 超时
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(data.empty());
    EXPECT_GE(duration.count(), 50);
    EXPECT_LT(duration.count(), 100);
}

TEST_F(message_queue_test, ProducerConsumer) {
    message_queue mq(default_config);
    const int num_messages = 100;
    std::atomic<int> received_count{0};
    
    // 消费者线程
    std::thread consumer([&mq, &received_count, num_messages]() {
        for (int i = 0; i < num_messages; ++i) {
            auto data = mq.receive();
            if (!data.empty()) {
                ++received_count;
            }
        }
    });
    
    // 生产者线程
    std::thread producer([&mq, num_messages]() {
        for (int i = 0; i < num_messages; ++i) {
            std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
            mq.send(data);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(num_messages, received_count);
}

TEST_F(message_queue_test, MultipleProducers) {
    message_queue mq(default_config);
    const int messages_per_producer = 50;
    const int num_producers = 4;
    
    std::atomic<int> total_received{0};
    
    // 消费者
    std::thread consumer([&mq, &total_received]() {
        while (total_received < messages_per_producer * num_producers) {
            auto data = mq.receive(10);  // 10ms 超时
            if (!data.empty()) {
                ++total_received;
            }
        }
    });
    
    // 多个生产者
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&mq, p, messages_per_producer]() {
            for (int i = 0; i < messages_per_producer; ++i) {
                std::vector<uint8_t> data = {
                    static_cast<uint8_t>(p),
                    static_cast<uint8_t>(i)
                };
                mq.send(data);
            }
        });
    }
    
    for (auto& t : producers) t.join();
    consumer.join();
    
    EXPECT_EQ(messages_per_producer * num_producers, total_received);
}

TEST_F(message_queue_test, GetAttributes) {
    message_queue mq;
    const int max_msgs = 10;  // 使用较小的值
    const int max_msg_size = 1024;  // 使用较小的值，避免超过系统限制
    
    mq.open("/test_queue", queue_mode::CREATE_ONLY, max_msgs, max_msg_size);
    
    auto attrs = mq.get_attributes();
    
    EXPECT_EQ("/test_queue", attrs.name);
    EXPECT_GT(attrs.queue_id, 0);  // 应该有一个有效的队列ID
    EXPECT_EQ(max_msgs, attrs.max_messages);
    EXPECT_EQ(max_msg_size, attrs.max_message_size);
    EXPECT_EQ(0, attrs.current_messages);  // 初始时队列为空
    
    // 发送消息后再次检查
    send_test_message(mq, "test");
    attrs = mq.get_attributes();
    EXPECT_EQ(1, attrs.current_messages);
}