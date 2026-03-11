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

#include <dbus/dbus.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/dbus/message.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <mc/log.h>
#include <test_utilities/test_base.h>

using namespace mc::dbus;

// 辅助宏：从tuple中提取bool值用于ASSERT_TRUE
#define REQUEST_NAME_SUCCESS(conn, name) std::get<0>((conn).request_name(name))

/**
 * @brief 端到端测试：两个DBUS服务互相订阅和接收信号
 */
class signal_send_receive_test : public mc::test::TestWithDbusDaemon {
protected:
    signal_send_receive_test()
    {
    }

    static void SetUpTestSuite()
    {
        mc::log::default_logger().set_level(mc::log::level::info);
        TestWithDbusDaemon::SetUpTestSuite();
    }

    static void TearDownTestSuite()
    {
        TestWithDbusDaemon::TearDownTestSuite();
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    mc::io_context& get_io_context()
    {
        return mc::runtime::get_io_context();
    }
};

/**
 * @brief 测试两个服务互相订阅和接收信号（带调试信息）
 *
 * 测试场景：
 * 1. 创建两个DBUS连接（服务A和服务B）
 * 2. 添加全局消息过滤器，记录所有消息
 * 3. 服务A订阅服务B发送的信号
 * 4. 服务B订阅服务A发送的信号
 * 5. 服务A发送信号，验证服务B能接收到
 * 6. 服务B发送信号，验证服务A能接收到
 */
TEST_F(signal_send_receive_test, test_two_services_bidirectional_signal)
{
    // 创建两个独立的DBUS连接
    auto conn_a = connection::open_session_bus(get_io_context());
    auto conn_b = connection::open_session_bus(get_io_context());

    ASSERT_TRUE(conn_a.start());
    ASSERT_TRUE(conn_b.start());
    ASSERT_TRUE(conn_a.is_connected());
    ASSERT_TRUE(conn_b.is_connected());

    // 注册服务名称
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_a, "org.test.ServiceA"));
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_b, "org.test.ServiceB"));

    ilog("服务A唯一名称: ${name}", ("name", conn_a.get_unique_name()));
    ilog("服务B唯一名称: ${name}", ("name", conn_b.get_unique_name()));

    // 等待服务名称注册完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 定义信号参数
    const std::string signal_path      = "/org/test/Signal";
    const std::string signal_interface = "org.test.SignalInterface";
    const std::string signal_member_a  = "SignalFromA";
    const std::string signal_member_b  = "SignalFromB";

    // 用于接收信号的计数器
    std::atomic<int> signal_a_received_count{0};
    std::atomic<int> signal_b_received_count{0};
    std::atomic<int> signal_a_data_received{0};
    std::atomic<int> signal_b_data_received{0};

    // 用于同步的互斥锁和条件变量
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    all_signals_received = false;

    // 服务A订阅服务B发送的信号
    auto rule_a = match_rule::new_signal(signal_member_b, signal_interface);
    rule_a.with_path(signal_path);
    ilog("服务A订阅规则: ${rule}", ("rule", rule_a.as_string()));

    match_cb_t callback_a = [&signal_b_received_count, &signal_b_data_received,
                             &mutex, &cv, &all_signals_received](message& msg) {
        signal_b_received_count.fetch_add(1);

        // 读取并验证信号数据
        ASSERT_FALSE(msg.reader().at_end()) << "信号应该包含数据";
        std::string data;
        msg.reader() >> data;
        EXPECT_EQ(data, "Hello from Service B") << "接收到的数据应该匹配发送的数据";
        if (data == "Hello from Service B") {
            signal_b_data_received.fetch_add(1);
        }

        // 检查是否所有信号都已接收
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (signal_b_received_count.load() >= 1) {
                all_signals_received = true;
            }
        }
        cv.notify_one();
    };
    conn_a.add_match(rule_a, std::move(callback_a), 1);

    // 服务B订阅服务A发送的信号
    auto rule_b = match_rule::new_signal(signal_member_a, signal_interface);
    rule_b.with_path(signal_path);
    ilog("服务B订阅规则: ${rule}", ("rule", rule_b.as_string()));

    match_cb_t callback_b = [&signal_a_received_count, &signal_a_data_received,
                             &mutex, &cv, &all_signals_received](message& msg) {
        signal_a_received_count.fetch_add(1);

        // 读取并验证信号数据
        ASSERT_FALSE(msg.reader().at_end()) << "信号应该包含数据";
        std::string data;
        msg.reader() >> data;
        EXPECT_EQ(data, "Hello from Service A") << "接收到的数据应该匹配发送的数据";
        if (data == "Hello from Service A") {
            signal_a_data_received.fetch_add(1);
        }

        // 检查是否所有信号都已接收
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (signal_a_received_count.load() >= 1) {
                all_signals_received = true;
            }
        }
        cv.notify_one();
    };
    conn_b.add_match(rule_b, std::move(callback_b), 2);

    // 等待订阅完成
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 服务A发送信号给服务B
    auto signal_a = message::new_signal(signal_path, signal_interface, signal_member_a);
    {
        auto writer = signal_a.writer();
        writer << std::string("Hello from Service A");
    }
    ilog("服务A发送信号: path=${path}, interface=${iface}, member=${member}",
         ("path", signal_path)("iface", signal_interface)("member", signal_member_a));
    ASSERT_TRUE(conn_a.send(std::move(signal_a)));

    // 服务B发送信号给服务A
    auto signal_b = message::new_signal(signal_path, signal_interface, signal_member_b);
    {
        auto writer = signal_b.writer();
        writer << std::string("Hello from Service B");
    }
    ilog("服务B发送信号: path=${path}, interface=${iface}, member=${member}",
         ("path", signal_path)("iface", signal_interface)("member", signal_member_b));
    ASSERT_TRUE(conn_b.send(std::move(signal_b)));
    auto start   = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(3000);

    while ((std::chrono::steady_clock::now() - start) < timeout) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (signal_a_received_count.load() >= 1 && signal_b_received_count.load() >= 1) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // 验证信号接收
    EXPECT_EQ(signal_a_received_count.load(), 1)
        << "服务B应该接收到服务A发送的信号";
    EXPECT_EQ(signal_b_received_count.load(), 1)
        << "服务A应该接收到服务B发送的信号";

    // 验证信号数据
    EXPECT_EQ(signal_a_data_received.load(), 1)
        << "服务B应该接收到服务A发送的信号数据";
    EXPECT_EQ(signal_b_data_received.load(), 1)
        << "服务A应该接收到服务B发送的信号数据";

    // 清理
    conn_a.remove_match(1);
    conn_b.remove_match(2);
    conn_a.disconnect();
    conn_b.disconnect();
}

/**
 * @brief 测试多个信号发送和接收（带调试信息）
 */
TEST_F(signal_send_receive_test, test_multiple_signals_bidirectional)
{
    // 创建两个独立的DBUS连接
    auto conn_a = connection::open_session_bus(get_io_context());
    auto conn_b = connection::open_session_bus(get_io_context());

    ASSERT_TRUE(conn_a.start());
    ASSERT_TRUE(conn_b.start());
    ASSERT_TRUE(conn_a.is_connected());
    ASSERT_TRUE(conn_b.is_connected());

    // 注册服务名称
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_a, "org.test.ServiceA"));
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_b, "org.test.ServiceB"));

    ilog("多信号测试 - 服务A: ${name}", ("name", conn_a.get_unique_name()));
    ilog("多信号测试 - 服务B: ${name}", ("name", conn_b.get_unique_name()));

    // 等待服务名称注册完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 定义信号参数
    const std::string signal_path      = "/org/test/MultipleSignal";
    const std::string signal_interface = "org.test.MultipleSignalInterface";
    const std::string signal_member_a  = "MultipleSignalFromA";
    const std::string signal_member_b  = "MultipleSignalFromB";

    // 用于接收信号的计数器
    std::atomic<int> signal_a_received_count{0};
    std::atomic<int> signal_b_received_count{0};

    // 用于验证数据正确性的集合
    std::set<int> signal_a_data_received; // 服务B接收到的服务A发送的数据
    std::set<int> signal_b_data_received; // 服务A接收到的服务B发送的数据
    std::mutex    data_mutex;

    // 服务A订阅服务B发送的信号
    auto rule_a = match_rule::new_signal(signal_member_b, signal_interface);
    rule_a.with_path(signal_path);
    ilog("服务A订阅: ${rule}", ("rule", rule_a.as_string()));

    match_cb_t callback_a = [&signal_b_received_count, &signal_b_data_received, &data_mutex](message& msg) {
        signal_b_received_count.fetch_add(1);

        // 读取并验证信号数据
        ASSERT_FALSE(msg.reader().at_end()) << "信号应该包含数据";
        int data = -1;
        msg.reader() >> data;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            signal_b_data_received.insert(data);
        }
    };
    conn_a.add_match(rule_a, std::move(callback_a), 1);

    // 服务B订阅服务A发送的信号
    auto rule_b = match_rule::new_signal(signal_member_a, signal_interface);
    rule_b.with_path(signal_path);
    ilog("服务B订阅: ${rule}", ("rule", rule_b.as_string()));

    match_cb_t callback_b = [&signal_a_received_count, &signal_a_data_received, &data_mutex](message& msg) {
        signal_a_received_count.fetch_add(1);

        // 读取并验证信号数据
        ASSERT_FALSE(msg.reader().at_end()) << "信号应该包含数据";
        int data = -1;
        msg.reader() >> data;
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            signal_a_data_received.insert(data);
        }
    };
    conn_b.add_match(rule_b, std::move(callback_b), 2);

    // 等待订阅完成
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 发送多个信号
    const int num_signals = 5;
    for (int i = 0; i < num_signals; ++i) {
        // 服务A发送信号
        auto signal_a = message::new_signal(signal_path, signal_interface, signal_member_a);
        {
            auto writer = signal_a.writer();
            writer << i;
        }
        ASSERT_TRUE(conn_a.send(std::move(signal_a)));

        // 服务B发送信号
        auto signal_b = message::new_signal(signal_path, signal_interface, signal_member_b);
        {
            auto writer = signal_b.writer();
            writer << i;
        }
        ASSERT_TRUE(conn_b.send(std::move(signal_b)));

        // 短暂等待，让 io_context 线程处理信号
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    auto start   = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(3000);

    while ((std::chrono::steady_clock::now() - start) < timeout) {
        if (signal_a_received_count.load() >= num_signals &&
            signal_b_received_count.load() >= num_signals) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // 验证信号接收
    EXPECT_EQ(signal_a_received_count.load(), num_signals)
        << "服务B应该接收到服务A发送的所有信号";
    EXPECT_EQ(signal_b_received_count.load(), num_signals)
        << "服务A应该接收到服务B发送的所有信号";

    // 验证数据内容正确性
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        EXPECT_EQ(signal_a_data_received.size(), static_cast<size_t>(num_signals))
            << "服务B应该接收到服务A发送的所有数据";
        EXPECT_EQ(signal_b_data_received.size(), static_cast<size_t>(num_signals))
            << "服务A应该接收到服务B发送的所有数据";

        // 验证数据值是否正确（应该是 0, 1, 2, 3, 4）
        for (int i = 0; i < num_signals; ++i) {
            EXPECT_TRUE(signal_a_data_received.count(i) > 0)
                << "服务B应该接收到数据 " << i;
            EXPECT_TRUE(signal_b_data_received.count(i) > 0)
                << "服务A应该接收到数据 " << i;
        }
    }

    // 清理
    conn_a.remove_match(1);
    conn_b.remove_match(2);
    conn_a.disconnect();
    conn_b.disconnect();
}

/**
 * @brief 测试信号订阅和取消订阅（带调试信息）
 */
TEST_F(signal_send_receive_test, test_signal_subscribe_unsubscribe)
{
    // 创建两个独立的DBUS连接
    auto conn_a = connection::open_session_bus(get_io_context());
    auto conn_b = connection::open_session_bus(get_io_context());

    ASSERT_TRUE(conn_a.start());
    ASSERT_TRUE(conn_b.start());
    ASSERT_TRUE(conn_a.is_connected());
    ASSERT_TRUE(conn_b.is_connected());

    // 注册服务名称
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_a, "org.test.ServiceA"));
    ASSERT_TRUE(REQUEST_NAME_SUCCESS(conn_b, "org.test.ServiceB"));

    ilog("取消订阅测试 - 服务A: ${name}", ("name", conn_a.get_unique_name()));
    ilog("取消订阅测试 - 服务B: ${name}", ("name", conn_b.get_unique_name()));

    // 等待服务名称注册完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 定义信号参数
    const std::string signal_path      = "/org/test/UnsubscribeSignal";
    const std::string signal_interface = "org.test.UnsubscribeSignalInterface";
    const std::string signal_member    = "UnsubscribeSignal";

    // 用于接收信号的计数器
    std::atomic<int> signal_received_count{0};

    // 服务A订阅服务B发送的信号
    auto rule = match_rule::new_signal(signal_member, signal_interface);
    rule.with_path(signal_path);
    ilog("服务A订阅: ${rule}", ("rule", rule.as_string()));

    match_cb_t callback = [&signal_received_count](message& msg) {
        signal_received_count.fetch_add(1);
    };
    conn_a.add_match(rule, std::move(callback), 1);

    // 等待订阅完成
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 服务B发送第一个信号（应该被接收）
    auto signal1 = message::new_signal(signal_path, signal_interface, signal_member);
    ASSERT_TRUE(conn_b.send(std::move(signal1)));

    // 等待 io_context 线程异步处理信号
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证第一个信号被接收
    EXPECT_EQ(signal_received_count.load(), 1)
        << "第一个信号应该被接收";

    // 取消订阅
    ilog("取消订阅");
    conn_a.remove_match(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int count_before_second_signal = signal_received_count.load();
    ASSERT_EQ(count_before_second_signal, 1) << "取消订阅前应该只收到一个信号";

    // 服务B发送第二个信号（不应该被接收）
    auto signal2 = message::new_signal(signal_path, signal_interface, signal_member);
    ASSERT_TRUE(conn_b.send(std::move(signal2)));

    // 等待 io_context 线程处理（如果有的话）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 验证第二个信号没有被接收（计数应该保持不变）
    int count_after_unsubscribe = signal_received_count.load();
    EXPECT_EQ(count_after_unsubscribe, count_before_second_signal)
        << "取消订阅后，信号不应该被匹配回调接收";

    // 清理
    conn_a.disconnect();
    conn_b.disconnect();
}
