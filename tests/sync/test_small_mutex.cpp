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

#include <mc/runtime/thread_list.h>
#include <mc/sync/small_mutex.h>

namespace mc::sync::test {

class SmallMutexTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 重新初始化小锁
        mutex = small_mutex{};
    }

    small_mutex mutex;
};

// 测试基本的锁定和解锁功能
TEST_F(SmallMutexTest, BasicLockUnlock)
{
    ASSERT_TRUE(mutex.try_lock());
    mutex.unlock();
}

// 测试数据存储功能
TEST_F(SmallMutexTest, DataStorage)
{
    // 初始数据应该为 0
    EXPECT_EQ(mutex.load_data(), 0);

    // 存储数据
    mutex.store_data(42);
    EXPECT_EQ(mutex.load_data(), 42);

    // 由于有 30 位用于数据，测试大值
    mutex.store_data(0x3FFFFFFF); // 30 位全 1
    EXPECT_EQ(mutex.load_data(), 0x3FFFFFFF);

    // 超过 30 位的值应该被截断
    mutex.store_data(0xFFFFFFFF);             // 32 位全 1
    EXPECT_EQ(mutex.load_data(), 0x3FFFFFFF); // 只保留低 30 位
}

// 测试锁定状态检查
TEST_F(SmallMutexTest, LockStatus)
{
    EXPECT_FALSE(mutex.is_locked());

    mutex.lock();
    EXPECT_TRUE(mutex.is_locked());

    mutex.unlock();
    EXPECT_FALSE(mutex.is_locked());
}

// 测试竞争条件下的基本功能
TEST_F(SmallMutexTest, BasicConcurrency)
{
    constexpr int num_threads           = 4;
    constexpr int iterations_per_thread = 100;

    mc::runtime::thread_list threads;
    std::atomic<int>         counter{0};

    threads.start_threads(num_threads, [&]() {
        for (int j = 0; j < iterations_per_thread; ++j) {
            std::lock_guard<small_mutex> lock(mutex);
            ++counter;
        }
    });
    threads.join_all();

    EXPECT_EQ(counter.load(), num_threads * iterations_per_thread);
}

// 测试数据存储的并发安全性
TEST_F(SmallMutexTest, ConcurrentDataAccess)
{
    constexpr int num_threads           = 4;
    constexpr int iterations_per_thread = 50;

    mc::runtime::thread_list threads;
    std::atomic<int>         total_ops{0};

    threads.start_threads(num_threads, [&]() {
        for (int j = 0; j < iterations_per_thread; ++j) {
            std::lock_guard<small_mutex> lock(mutex);

            uint32_t current_data = mutex.load_data();
            uint32_t new_data     = (current_data + 1) % 0x40000000; // 确保不超过 30 位
            mutex.store_data(new_data);

            total_ops.fetch_add(1);
        }
    });

    threads.join_all();

    EXPECT_EQ(total_ops.load(), num_threads * iterations_per_thread);
    // 最终数据应该等于总的操作次数模 30 位最大值
    EXPECT_EQ(mutex.load_data(), (num_threads * iterations_per_thread) % 0x40000000);
}

// 测试超时锁定
TEST_F(SmallMutexTest, TimeoutLock)
{
    // 先获取锁，确保后续的 try_lock_for 只能依赖超时返回
    mutex.lock();

    auto start  = std::chrono::steady_clock::now();
    bool result = mutex.try_lock_for(std::chrono::milliseconds(20));
    auto end    = std::chrono::steady_clock::now();
    mutex.unlock();

    EXPECT_FALSE(result); // 应该超时失败

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 10);  // 至少等待了 10ms
    EXPECT_LE(duration.count(), 200); // 不超过 200ms，防止环境波动
}

// 测试内存布局：确保 small_mutex 占用 4 字节
TEST_F(SmallMutexTest, MemoryFootprint)
{
    EXPECT_EQ(sizeof(small_mutex), 4);
    EXPECT_EQ(sizeof(basic_small_mutex<standard_small_mutex_policy>), 4);
    EXPECT_EQ(sizeof(spin_mutex), 4);

    // 验证对齐
    EXPECT_EQ(alignof(small_mutex), 4);
    EXPECT_EQ(alignof(basic_small_mutex<standard_small_mutex_policy>), 4);
}

// 测试不同策略的 small_mutex
TEST_F(SmallMutexTest, DifferentPolicies)
{
    spin_mutex mutex;

    // 测试自旋策略锁
    ASSERT_TRUE(mutex.try_lock());
    mutex.store_data(20);
    EXPECT_EQ(mutex.load_data(), 20);
    mutex.unlock();
}

// 测试原子操作的直接访问
TEST_F(SmallMutexTest, AtomicWordAccess)
{
    auto& atomic_word = mutex.get_atomic_word();

    // 直接设置一些值
    atomic_word.store(0x12345678);

    // 验证数据位正确解码
    EXPECT_EQ(mutex.load_data(), 0x12345678 >> 2);

    // 验证锁位被保留
    mutex.lock();
    EXPECT_TRUE(mutex.is_locked());
    EXPECT_EQ(atomic_word.load() & 1, 1); // 锁位应该被设置
    mutex.unlock();
}

} // namespace mc::sync::test