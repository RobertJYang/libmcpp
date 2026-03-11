/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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

#include "../runtime/test_future_helpers.h"
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/runtime/thread_list.h>

class thread_safe_object : public mc::enable_shared_from_this<thread_safe_object> {
public:
    thread_safe_object()  = default;
    ~thread_safe_object() = default;

    void increment()
    {
        ++m_counter;
    }

    int get_counter() const
    {
        return m_counter;
    }

private:
    std::atomic<int> m_counter{0};
};

class SharedPtrThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

// 测试在多线程环境下 try_add_ref 与 release_ref 的竞争条件
TEST_F(SharedPtrThreadSafetyTest, TryAddRefReleaseRefRaceCondition)
{
    const int iterations  = 1000;
    const int num_threads = 8;

    for (int iter = 0; iter < iterations; ++iter) {
        auto obj  = mc::make_shared<thread_safe_object>();
        auto weak = obj->weak_from_this();

        std::atomic<int>  successful_upgrades{0};
        std::atomic<int>  failed_upgrades{0};
        std::atomic<bool> released{false};

        mc::runtime::thread_list            threads;
        mc::test::runtime::countdown_future upgrade_ready(num_threads - 1);
        mc::test::runtime::future_flag      release_started;

        // 创建多个线程尝试从弱引用升级到强引用
        threads.start_threads(num_threads - 1, [&]() {
            // 通知已准备好
            upgrade_ready.arrive();

            // 等待释放线程开始执行
            if (!release_started.wait_for(std::chrono::milliseconds(2000))) {
                return; // 超时，测试失败
            }

            if (auto strong = weak.lock()) {
                successful_upgrades.fetch_add(1);
            } else {
                failed_upgrades.fetch_add(1);
            }
        });

        // 一个线程负责释放最后的强引用
        threads.add_thread([&]() {
            // 等待所有升级线程都准备好
            if (!upgrade_ready.wait_for(std::chrono::milliseconds(2000))) {
                return; // 超时，测试失败
            }

            // 通知释放开始
            release_started.set();

            obj.reset(); // 释放强引用
            released.store(true);
        });

        // 等待所有线程完成
        threads.join_all();

        // 验证结果
        EXPECT_TRUE(released.load());

        // 成功升级的数量应该是合理的（不应该有不一致的状态）
        int total_attempts = successful_upgrades.load() + failed_upgrades.load();
        EXPECT_EQ(total_attempts, num_threads - 1);
    }
}

// 测试销毁标记的原子性
TEST_F(SharedPtrThreadSafetyTest, DestroyedMarkerAtomicity)
{
    const int iterations = 500;

    for (int iter = 0; iter < iterations; ++iter) {
        auto obj  = mc::make_shared<thread_safe_object>();
        auto weak = obj->weak_from_this();

        std::atomic<bool> destroy_completed{false};
        std::atomic<int>  upgrade_attempts{0};
        std::atomic<int>  successful_upgrades{0};

        mc::runtime::thread_list            threads;
        mc::test::runtime::countdown_future upgrade_ready(4);
        mc::test::runtime::future_flag      destroy_started;

        threads.add_thread([&]() {
            // 等待所有升级线程都准备好
            if (!upgrade_ready.wait_for(std::chrono::milliseconds(2000))) {
                return; // 超时，测试失败
            }

            // 通知销毁开始
            destroy_started.set();

            obj.reset();
            destroy_completed.store(true);
        });

        threads.start_threads(4, [&]() {
            // 通知已准备好
            upgrade_ready.arrive();

            // 等待销毁线程开始
            if (!destroy_started.wait_for(std::chrono::milliseconds(2000))) {
                return; // 超时，测试失败
            }

            while (!destroy_completed.load()) {
                upgrade_attempts.fetch_add(1);
                if (auto strong = weak.lock()) {
                    successful_upgrades.fetch_add(1);
                    // 立即释放，避免阻止销毁
                    strong.reset();
                }
                std::this_thread::yield();
            }
        });

        threads.join_all();

        // 在销毁完成后，不应该再有成功的升级
        EXPECT_FALSE(weak.lock());
    }
}

// 测试对已销毁对象调用 add_ref 会抛出异常
TEST_F(SharedPtrThreadSafetyTest, AddRefOnDestroyedObjectThrows)
{
    auto  obj     = mc::make_shared<thread_safe_object>();
    auto* raw_ptr = obj.get();

    // 获取弱引用以保持对象存在
    auto weak = obj->weak_from_this();

    // 释放强引用，触发销毁标记
    obj.reset();

    // 现在对象应该被标记为已销毁
    EXPECT_TRUE(raw_ptr->is_destroyed());

    // 尝试调用 add_ref 应该抛出异常
    EXPECT_THROW(raw_ptr->add_ref(), mc::invalid_op_exception);
}

// 测试引用计数的内存排序
TEST_F(SharedPtrThreadSafetyTest, MemoryOrderingConsistency)
{
    const int iterations            = 100;
    const int operations_per_thread = 1000;

    for (int iter = 0; iter < iterations; ++iter) {
        auto obj = mc::make_shared<thread_safe_object>();

        std::vector<mc::shared_ptr<thread_safe_object>> shared_ptrs;
        shared_ptrs.reserve(8);

        // 创建多个shared_ptr副本
        for (int i = 0; i < 8; ++i) {
            shared_ptrs.push_back(obj);
        }

        mc::runtime::thread_list threads;
        std::atomic<int>         total_operations{0};

        // 每个线程随机进行增减引用计数操作
        threads.start_threads(8, [&](std::size_t i) {
            auto local_ptr = shared_ptrs[i];

            for (int op = 0; op < operations_per_thread; ++op) {
                // 随机操作：复制指针或清空指针
                if (op % 2 == 0) {
                    auto temp = local_ptr; // 增加引用计数
                    local_ptr->increment();
                    // temp 自动销毁，减少引用计数
                } else {
                    local_ptr = shared_ptrs[i]; // 重新赋值
                    local_ptr->increment();
                }

                total_operations.fetch_add(1);
            }
        });

        // 等待所有操作完成
        threads.join_all();

        // 验证对象状态一致性
        EXPECT_EQ(total_operations.load(), 8 * operations_per_thread);
        EXPECT_EQ(obj->get_counter(), 8 * operations_per_thread);

        // 引用计数应该是合理的
        EXPECT_GE(obj->ref_count(), 1U); // 至少有原始的 obj

        // 清理
        shared_ptrs.clear();
    }
}