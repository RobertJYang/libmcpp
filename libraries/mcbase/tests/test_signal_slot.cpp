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
#include <future>
#include <thread>

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